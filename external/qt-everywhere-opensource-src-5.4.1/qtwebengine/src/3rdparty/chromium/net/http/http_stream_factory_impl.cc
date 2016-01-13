// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory_impl.h"

#include <string>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream_factory_impl_job.h"
#include "net/http/http_stream_factory_impl_request.h"
#include "net/spdy/spdy_http_stream.h"
#include "url/gurl.h"

namespace net {

namespace {

const PortAlternateProtocolPair kNoAlternateProtocol = {
  0,  UNINITIALIZED_ALTERNATE_PROTOCOL
};

GURL UpgradeUrlToHttps(const GURL& original_url, int port) {
  GURL::Replacements replacements;
  // new_sheme and new_port need to be in scope here because GURL::Replacements
  // references the memory contained by them directly.
  const std::string new_scheme = "https";
  const std::string new_port = base::IntToString(port);
  replacements.SetSchemeStr(new_scheme);
  replacements.SetPortStr(new_port);
  return original_url.ReplaceComponents(replacements);
}

}  // namespace

HttpStreamFactoryImpl::HttpStreamFactoryImpl(HttpNetworkSession* session,
                                             bool for_websockets)
    : session_(session),
      for_websockets_(for_websockets) {}

HttpStreamFactoryImpl::~HttpStreamFactoryImpl() {
  DCHECK(request_map_.empty());
  DCHECK(spdy_session_request_map_.empty());

  std::set<const Job*> tmp_job_set;
  tmp_job_set.swap(orphaned_job_set_);
  STLDeleteContainerPointers(tmp_job_set.begin(), tmp_job_set.end());
  DCHECK(orphaned_job_set_.empty());

  tmp_job_set.clear();
  tmp_job_set.swap(preconnect_job_set_);
  STLDeleteContainerPointers(tmp_job_set.begin(), tmp_job_set.end());
  DCHECK(preconnect_job_set_.empty());
}

HttpStreamRequest* HttpStreamFactoryImpl::RequestStream(
    const HttpRequestInfo& request_info,
    RequestPriority priority,
    const SSLConfig& server_ssl_config,
    const SSLConfig& proxy_ssl_config,
    HttpStreamRequest::Delegate* delegate,
    const BoundNetLog& net_log) {
  DCHECK(!for_websockets_);
  return RequestStreamInternal(request_info,
                               priority,
                               server_ssl_config,
                               proxy_ssl_config,
                               delegate,
                               NULL,
                               net_log);
}

HttpStreamRequest* HttpStreamFactoryImpl::RequestWebSocketHandshakeStream(
    const HttpRequestInfo& request_info,
    RequestPriority priority,
    const SSLConfig& server_ssl_config,
    const SSLConfig& proxy_ssl_config,
    HttpStreamRequest::Delegate* delegate,
    WebSocketHandshakeStreamBase::CreateHelper* create_helper,
    const BoundNetLog& net_log) {
  DCHECK(for_websockets_);
  DCHECK(create_helper);
  return RequestStreamInternal(request_info,
                               priority,
                               server_ssl_config,
                               proxy_ssl_config,
                               delegate,
                               create_helper,
                               net_log);
}

HttpStreamRequest* HttpStreamFactoryImpl::RequestStreamInternal(
    const HttpRequestInfo& request_info,
    RequestPriority priority,
    const SSLConfig& server_ssl_config,
    const SSLConfig& proxy_ssl_config,
    HttpStreamRequest::Delegate* delegate,
    WebSocketHandshakeStreamBase::CreateHelper*
        websocket_handshake_stream_create_helper,
    const BoundNetLog& net_log) {
  Request* request = new Request(request_info.url,
                                 this,
                                 delegate,
                                 websocket_handshake_stream_create_helper,
                                 net_log);

  GURL alternate_url;
  PortAlternateProtocolPair alternate =
      GetAlternateProtocolRequestFor(request_info.url, &alternate_url);
  Job* alternate_job = NULL;
  if (alternate.protocol != UNINITIALIZED_ALTERNATE_PROTOCOL) {
    // Never share connection with other jobs for FTP requests.
    DCHECK(!request_info.url.SchemeIs("ftp"));

    HttpRequestInfo alternate_request_info = request_info;
    alternate_request_info.url = alternate_url;
    alternate_job =
        new Job(this, session_, alternate_request_info, priority,
                server_ssl_config, proxy_ssl_config, net_log.net_log());
    request->AttachJob(alternate_job);
    alternate_job->MarkAsAlternate(request_info.url, alternate);
  }

  Job* job = new Job(this, session_, request_info, priority,
                     server_ssl_config, proxy_ssl_config, net_log.net_log());
  request->AttachJob(job);
  if (alternate_job) {
    // Never share connection with other jobs for FTP requests.
    DCHECK(!request_info.url.SchemeIs("ftp"));

    job->WaitFor(alternate_job);
    // Make sure to wait until we call WaitFor(), before starting
    // |alternate_job|, otherwise |alternate_job| will not notify |job|
    // appropriately.
    alternate_job->Start(request);
  }
  // Even if |alternate_job| has already finished, it won't have notified the
  // request yet, since we defer that to the next iteration of the MessageLoop,
  // so starting |job| is always safe.
  job->Start(request);
  return request;
}

void HttpStreamFactoryImpl::PreconnectStreams(
    int num_streams,
    const HttpRequestInfo& request_info,
    RequestPriority priority,
    const SSLConfig& server_ssl_config,
    const SSLConfig& proxy_ssl_config) {
  DCHECK(!for_websockets_);
  GURL alternate_url;
  PortAlternateProtocolPair alternate =
      GetAlternateProtocolRequestFor(request_info.url, &alternate_url);
  Job* job = NULL;
  if (alternate.protocol != UNINITIALIZED_ALTERNATE_PROTOCOL) {
    HttpRequestInfo alternate_request_info = request_info;
    alternate_request_info.url = alternate_url;
    job = new Job(this, session_, alternate_request_info, priority,
                  server_ssl_config, proxy_ssl_config, session_->net_log());
    job->MarkAsAlternate(request_info.url, alternate);
  } else {
    job = new Job(this, session_, request_info, priority,
                  server_ssl_config, proxy_ssl_config, session_->net_log());
  }
  preconnect_job_set_.insert(job);
  job->Preconnect(num_streams);
}

const HostMappingRules* HttpStreamFactoryImpl::GetHostMappingRules() const {
  return session_->params().host_mapping_rules;
}

PortAlternateProtocolPair HttpStreamFactoryImpl::GetAlternateProtocolRequestFor(
    const GURL& original_url,
    GURL* alternate_url) {
  if (!session_->params().use_alternate_protocols)
    return kNoAlternateProtocol;

  if (original_url.SchemeIs("ftp"))
    return kNoAlternateProtocol;

  HostPortPair origin = HostPortPair(original_url.HostNoBrackets(),
                                     original_url.EffectiveIntPort());

  HttpServerProperties& http_server_properties =
      *session_->http_server_properties();
  if (!http_server_properties.HasAlternateProtocol(origin))
    return kNoAlternateProtocol;

  PortAlternateProtocolPair alternate =
      http_server_properties.GetAlternateProtocol(origin);
  if (alternate.protocol == ALTERNATE_PROTOCOL_BROKEN) {
    HistogramAlternateProtocolUsage(
        ALTERNATE_PROTOCOL_USAGE_BROKEN,
        http_server_properties.GetAlternateProtocolExperiment());
    return kNoAlternateProtocol;
  }

  if (!IsAlternateProtocolValid(alternate.protocol)) {
    NOTREACHED();
    return kNoAlternateProtocol;
  }

  // Some shared unix systems may have user home directories (like
  // http://foo.com/~mike) which allow users to emit headers.  This is a bad
  // idea already, but with Alternate-Protocol, it provides the ability for a
  // single user on a multi-user system to hijack the alternate protocol.
  // These systems also enforce ports <1024 as restricted ports.  So don't
  // allow protocol upgrades to user-controllable ports.
  const int kUnrestrictedPort = 1024;
  if (!session_->params().enable_user_alternate_protocol_ports &&
      (alternate.port >= kUnrestrictedPort &&
       origin.port() < kUnrestrictedPort))
    return kNoAlternateProtocol;

  origin.set_port(alternate.port);
  if (alternate.protocol >= NPN_SPDY_MINIMUM_VERSION &&
      alternate.protocol <= NPN_SPDY_MAXIMUM_VERSION) {
    if (!HttpStreamFactory::spdy_enabled())
      return kNoAlternateProtocol;

    if (session_->HasSpdyExclusion(origin))
      return kNoAlternateProtocol;

    *alternate_url = UpgradeUrlToHttps(original_url, alternate.port);
  } else {
    DCHECK_EQ(QUIC, alternate.protocol);
    if (!session_->params().enable_quic ||
        !(original_url.SchemeIs("http") ||
          session_->params().enable_quic_https)) {
        return kNoAlternateProtocol;
    }
    // TODO(rch):  Figure out how to make QUIC iteract with PAC
    // scripts.  By not re-writing the URL, we will query the PAC script
    // for the proxy to use to reach the original URL via TCP.  But
    // the alternate request will be going via UDP to a different port.
    *alternate_url = original_url;
  }
  return alternate;
}

void HttpStreamFactoryImpl::OrphanJob(Job* job, const Request* request) {
  DCHECK(ContainsKey(request_map_, job));
  DCHECK_EQ(request_map_[job], request);
  DCHECK(!ContainsKey(orphaned_job_set_, job));

  request_map_.erase(job);

  orphaned_job_set_.insert(job);
  job->Orphan(request);
}

void HttpStreamFactoryImpl::OnNewSpdySessionReady(
    const base::WeakPtr<SpdySession>& spdy_session,
    bool direct,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    bool was_npn_negotiated,
    NextProto protocol_negotiated,
    bool using_spdy,
    const BoundNetLog& net_log) {
  while (true) {
    if (!spdy_session)
      break;
    const SpdySessionKey& spdy_session_key = spdy_session->spdy_session_key();
    // Each iteration may empty out the RequestSet for |spdy_session_key| in
    // |spdy_session_request_map_|. So each time, check for RequestSet and use
    // the first one.
    //
    // TODO(willchan): If it's important, switch RequestSet out for a FIFO
    // queue (Order by priority first, then FIFO within same priority). Unclear
    // that it matters here.
    if (!ContainsKey(spdy_session_request_map_, spdy_session_key))
      break;
    Request* request = *spdy_session_request_map_[spdy_session_key].begin();
    request->Complete(was_npn_negotiated,
                      protocol_negotiated,
                      using_spdy,
                      net_log);
    if (for_websockets_) {
      // TODO(ricea): Restore this code path when WebSocket over SPDY
      // implementation is ready.
      NOTREACHED();
    } else {
      bool use_relative_url = direct || request->url().SchemeIs("https");
      request->OnStreamReady(
          NULL,
          used_ssl_config,
          used_proxy_info,
          new SpdyHttpStream(spdy_session, use_relative_url));
    }
  }
  // TODO(mbelshe): Alert other valid requests.
}

void HttpStreamFactoryImpl::OnOrphanedJobComplete(const Job* job) {
  orphaned_job_set_.erase(job);
  delete job;
}

void HttpStreamFactoryImpl::OnPreconnectsComplete(const Job* job) {
  preconnect_job_set_.erase(job);
  delete job;
  OnPreconnectsCompleteInternal();
}

}  // namespace net
