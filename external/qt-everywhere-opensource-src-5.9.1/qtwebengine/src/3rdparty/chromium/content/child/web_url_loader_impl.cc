// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/web_url_loader_impl.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/mime_util/mime_util.h"
#include "content/child/child_thread_impl.h"
#include "content/child/ftp_directory_listing_response_delegate.h"
#include "content/child/request_extra_data.h"
#include "content/child/resource_dispatcher.h"
#include "content/child/shared_memory_data_consumer_handle.h"
#include "content/child/sync_load_response.h"
#include "content/child/web_url_request_util.h"
#include "content/child/weburlresponse_extradata_impl.h"
#include "content/common/resource_messages.h"
#include "content/common/resource_request.h"
#include "content/common/resource_request_body_impl.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/url_loader.mojom.h"
#include "content/public/child/fixed_received_data.h"
#include "content/public/child/request_peer.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "net/base/data_url.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/ct_sct_to_string.h"
#include "net/cert/x509_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/url_request/url_request_data_job.h"
#include "third_party/WebKit/public/platform/WebHTTPLoadInfo.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebSecurityStyle.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLLoadTiming.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

using base::Time;
using base::TimeTicks;
using blink::WebData;
using blink::WebHTTPBody;
using blink::WebHTTPHeaderVisitor;
using blink::WebHTTPLoadInfo;
using blink::WebReferrerPolicy;
using blink::WebSecurityPolicy;
using blink::WebString;
using blink::WebURL;
using blink::WebURLError;
using blink::WebURLLoadTiming;
using blink::WebURLLoader;
using blink::WebURLLoaderClient;
using blink::WebURLRequest;
using blink::WebURLResponse;

namespace content {

// Utilities ------------------------------------------------------------------

namespace {

using HeadersVector = ResourceDevToolsInfo::HeadersVector;

// TODO(estark): Figure out a way for the embedder to provide the
// security style for a resource. Ideally, the logic for assigning
// per-resource security styles should live in the same place as the
// logic for assigning per-page security styles (which lives in the
// embedder). It would also be nice for the embedder to have the chance
// to control the per-resource security style beyond the simple logic
// here. (For example, the embedder might want to mark certain resources
// differently if they use SHA1 signatures.) https://crbug.com/648326
blink::WebSecurityStyle GetSecurityStyleForResource(
    const GURL& url,
    net::CertStatus cert_status) {
  if (!url.SchemeIsCryptographic())
    return blink::WebSecurityStyleUnauthenticated;

  // Minor errors don't lower the security style to
  // WebSecurityStyleAuthenticationBroken.
  if (net::IsCertStatusError(cert_status) &&
      !net::IsCertStatusMinorError(cert_status)) {
    return blink::WebSecurityStyleAuthenticationBroken;
  }

  return blink::WebSecurityStyleAuthenticated;
}

// Converts timing data from |load_timing| to the format used by WebKit.
void PopulateURLLoadTiming(const net::LoadTimingInfo& load_timing,
                           WebURLLoadTiming* url_timing) {
  DCHECK(!load_timing.request_start.is_null());

  const TimeTicks kNullTicks;
  url_timing->initialize();
  url_timing->setRequestTime(
      (load_timing.request_start - kNullTicks).InSecondsF());
  url_timing->setProxyStart(
      (load_timing.proxy_resolve_start - kNullTicks).InSecondsF());
  url_timing->setProxyEnd(
      (load_timing.proxy_resolve_end - kNullTicks).InSecondsF());
  url_timing->setDNSStart(
      (load_timing.connect_timing.dns_start - kNullTicks).InSecondsF());
  url_timing->setDNSEnd(
      (load_timing.connect_timing.dns_end - kNullTicks).InSecondsF());
  url_timing->setConnectStart(
      (load_timing.connect_timing.connect_start - kNullTicks).InSecondsF());
  url_timing->setConnectEnd(
      (load_timing.connect_timing.connect_end - kNullTicks).InSecondsF());
  url_timing->setSSLStart(
      (load_timing.connect_timing.ssl_start - kNullTicks).InSecondsF());
  url_timing->setSSLEnd(
      (load_timing.connect_timing.ssl_end - kNullTicks).InSecondsF());
  url_timing->setSendStart(
      (load_timing.send_start - kNullTicks).InSecondsF());
  url_timing->setSendEnd(
      (load_timing.send_end - kNullTicks).InSecondsF());
  url_timing->setReceiveHeadersEnd(
      (load_timing.receive_headers_end - kNullTicks).InSecondsF());
  url_timing->setPushStart((load_timing.push_start - kNullTicks).InSecondsF());
  url_timing->setPushEnd((load_timing.push_end - kNullTicks).InSecondsF());
}

net::RequestPriority ConvertWebKitPriorityToNetPriority(
    const WebURLRequest::Priority& priority) {
  switch (priority) {
    case WebURLRequest::PriorityVeryHigh:
      return net::HIGHEST;

    case WebURLRequest::PriorityHigh:
      return net::MEDIUM;

    case WebURLRequest::PriorityMedium:
      return net::LOW;

    case WebURLRequest::PriorityLow:
      return net::LOWEST;

    case WebURLRequest::PriorityVeryLow:
      return net::IDLE;

    case WebURLRequest::PriorityUnresolved:
    default:
      NOTREACHED();
      return net::LOW;
  }
}

blink::WebReferrerPolicy NetReferrerPolicyToBlinkReferrerPolicy(
    net::URLRequest::ReferrerPolicy net_policy) {
  switch (net_policy) {
    case net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
      return blink::WebReferrerPolicyNoReferrerWhenDowngrade;
    case net::URLRequest::
        REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN:
      return blink::
          WebReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin;
    case net::URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN:
      return blink::WebReferrerPolicyOriginWhenCrossOrigin;
    case net::URLRequest::NEVER_CLEAR_REFERRER:
      return blink::WebReferrerPolicyAlways;
    case net::URLRequest::ORIGIN:
      return blink::WebReferrerPolicyOrigin;
    case net::URLRequest::NO_REFERRER:
      return blink::WebReferrerPolicyNever;
    case net::URLRequest::MAX_REFERRER_POLICY:
      NOTREACHED();
      return blink::WebReferrerPolicyDefault;
  }
  NOTREACHED();
  return blink::WebReferrerPolicyDefault;
}

// Extracts info from a data scheme URL |url| into |info| and |data|. Returns
// net::OK if successful. Returns a net error code otherwise.
int GetInfoFromDataURL(const GURL& url,
                       ResourceResponseInfo* info,
                       std::string* data) {
  // Assure same time for all time fields of data: URLs.
  Time now = Time::Now();
  info->load_timing.request_start = TimeTicks::Now();
  info->load_timing.request_start_time = now;
  info->request_time = now;
  info->response_time = now;

  std::string mime_type;
  std::string charset;
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(std::string()));
  int result = net::URLRequestDataJob::BuildResponse(
      url, &mime_type, &charset, data, headers.get());
  if (result != net::OK)
    return result;

  info->headers = headers;
  info->mime_type.swap(mime_type);
  info->charset.swap(charset);
  info->content_length = data->length();
  info->encoded_data_length = 0;
  info->encoded_body_length = 0;

  return net::OK;
}

// Convert a net::SignedCertificateTimestampAndStatus object to a
// blink::WebURLResponse::SignedCertificateTimestamp object.
blink::WebURLResponse::SignedCertificateTimestamp NetSCTToBlinkSCT(
    const net::SignedCertificateTimestampAndStatus& sct_and_status) {
  return blink::WebURLResponse::SignedCertificateTimestamp(
      WebString::fromUTF8(net::ct::StatusToString(sct_and_status.status)),
      WebString::fromUTF8(net::ct::OriginToString(sct_and_status.sct->origin)),
      WebString::fromUTF8(sct_and_status.sct->log_description),
      WebString::fromUTF8(base::HexEncode(sct_and_status.sct->log_id.c_str(),
                                          sct_and_status.sct->log_id.length())),
      sct_and_status.sct->timestamp.ToJavaTime(),
      WebString::fromUTF8(net::ct::HashAlgorithmToString(
          sct_and_status.sct->signature.hash_algorithm)),
      WebString::fromUTF8(net::ct::SignatureAlgorithmToString(
          sct_and_status.sct->signature.signature_algorithm)),
      WebString::fromUTF8(
          base::HexEncode(sct_and_status.sct->signature.signature_data.c_str(),
          sct_and_status.sct->signature.signature_data.length())));
}

void SetSecurityStyleAndDetails(const GURL& url,
                                const ResourceResponseInfo& info,
                                WebURLResponse* response,
                                bool report_security_info) {
  if (!report_security_info) {
    response->setSecurityStyle(blink::WebSecurityStyleUnknown);
    return;
  }
  if (!url.SchemeIsCryptographic()) {
    response->setSecurityStyle(blink::WebSecurityStyleUnauthenticated);
    return;
  }

  // There are cases where an HTTPS request can come in without security
  // info attached (such as a redirect response).
  if (info.certificate.empty()) {
    response->setSecurityStyle(blink::WebSecurityStyleUnknown);
    return;
  }

  int ssl_version =
      net::SSLConnectionStatusToVersion(info.ssl_connection_status);
  const char* protocol;
  net::SSLVersionToString(&protocol, ssl_version);

  const char* key_exchange;
  const char* cipher;
  const char* mac;
  bool is_aead;
  bool is_tls13;
  uint16_t cipher_suite =
      net::SSLConnectionStatusToCipherSuite(info.ssl_connection_status);
  net::SSLCipherSuiteToStrings(&key_exchange, &cipher, &mac, &is_aead,
                               &is_tls13, cipher_suite);
  if (key_exchange == nullptr) {
    DCHECK(is_tls13);
    key_exchange = "";
  }

  if (mac == nullptr) {
    DCHECK(is_aead);
    mac = "";
  }

  const char* key_exchange_group = "";
  if (info.ssl_key_exchange_group != 0) {
    // Historically the field was named 'curve' rather than 'group'.
    key_exchange_group = SSL_get_curve_name(info.ssl_key_exchange_group);
    if (!key_exchange_group) {
      NOTREACHED();
      key_exchange_group = "";
    }
  }

  response->setSecurityStyle(
      GetSecurityStyleForResource(url, info.cert_status));

  blink::WebURLResponse::SignedCertificateTimestampList sct_list(
      info.signed_certificate_timestamps.size());

  for (size_t i = 0; i < sct_list.size(); ++i)
    sct_list[i] = NetSCTToBlinkSCT(info.signed_certificate_timestamps[i]);

  std::string subject, issuer;
  base::Time valid_start, valid_expiry;
  std::vector<std::string> san;
  bool rv = net::x509_util::ParseCertificateSandboxed(
      info.certificate[0], &subject, &issuer, &valid_start, &valid_expiry, &san,
      &san);
  if (!rv) {
    NOTREACHED();
    response->setSecurityStyle(blink::WebSecurityStyleUnknown);
    return;
  }

  blink::WebVector<blink::WebString> web_san(san.size());
  std::transform(
      san.begin(),
      san.end(), web_san.begin(),
      [](const std::string& h) { return blink::WebString::fromLatin1(h); });

  blink::WebVector<blink::WebString> web_cert(info.certificate.size());
  std::transform(
      info.certificate.begin(),
      info.certificate.end(), web_cert.begin(),
      [](const std::string& h) { return blink::WebString::fromLatin1(h); });

  blink::WebURLResponse::WebSecurityDetails webSecurityDetails(
      WebString::fromUTF8(protocol), WebString::fromUTF8(key_exchange),
      WebString::fromUTF8(key_exchange_group),
      WebString::fromUTF8(cipher), WebString::fromUTF8(mac),
      WebString::fromUTF8(subject),
      web_san,
      WebString::fromUTF8(issuer),
      valid_start.ToDoubleT(),
      valid_expiry.ToDoubleT(),
      web_cert,
      sct_list);

  response->setSecurityDetails(webSecurityDetails);
}

}  // namespace

StreamOverrideParameters::StreamOverrideParameters() {}
StreamOverrideParameters::~StreamOverrideParameters() {}

// This inner class exists since the WebURLLoader may be deleted while inside a
// call to WebURLLoaderClient.  Refcounting is to keep the context from being
// deleted if it may have work to do after calling into the client.
class WebURLLoaderImpl::Context : public base::RefCounted<Context> {
 public:
  using ReceivedData = RequestPeer::ReceivedData;

  Context(WebURLLoaderImpl* loader,
          ResourceDispatcher* resource_dispatcher,
          mojom::URLLoaderFactory* factory,
          mojo::AssociatedGroup* associated_group);

  WebURLLoaderClient* client() const { return client_; }
  void set_client(WebURLLoaderClient* client) { client_ = client; }

  void Cancel();
  void SetDefersLoading(bool value);
  void DidChangePriority(WebURLRequest::Priority new_priority,
                         int intra_priority_value);
  void Start(const WebURLRequest& request,
             SyncLoadResponse* sync_load_response);
  void SetTaskRunner(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  void OnUploadProgress(uint64_t position, uint64_t size);
  bool OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          const ResourceResponseInfo& info);
  void OnReceivedResponse(const ResourceResponseInfo& info);
  void OnDownloadedData(int len, int encoded_data_length);
  void OnReceivedData(std::unique_ptr<ReceivedData> data);
  void OnReceivedCachedMetadata(const char* data, int len);
  void OnCompletedRequest(int error_code,
                          bool was_ignored_by_handler,
                          bool stale_copy_in_cache,
                          const base::TimeTicks& completion_time,
                          int64_t total_transfer_size);

 private:
  friend class base::RefCounted<Context>;
  ~Context();

  // Called when the body data stream is detached from the reader side.
  void CancelBodyStreaming();
  // We can optimize the handling of data URLs in most cases.
  bool CanHandleDataURLRequestLocally() const;
  void HandleDataURL();

  WebURLLoaderImpl* loader_;
  WebURLRequest request_;
  WebURLLoaderClient* client_;
  ResourceDispatcher* resource_dispatcher_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<FtpDirectoryListingResponseDelegate> ftp_listing_delegate_;
  std::unique_ptr<StreamOverrideParameters> stream_override_;
  std::unique_ptr<SharedMemoryDataConsumerHandle::Writer> body_stream_writer_;
  enum DeferState {NOT_DEFERRING, SHOULD_DEFER, DEFERRED_DATA};
  DeferState defers_loading_;
  int request_id_;

  // These are owned by the Blink::Platform singleton.
  mojom::URLLoaderFactory* url_loader_factory_;
  mojo::AssociatedGroup* associated_group_;
};

// A thin wrapper class for Context to ensure its lifetime while it is
// handling IPC messages coming from ResourceDispatcher. Owns one ref to
// Context and held by ResourceDispatcher.
class WebURLLoaderImpl::RequestPeerImpl : public RequestPeer {
 public:
  explicit RequestPeerImpl(Context* context);

  // RequestPeer methods:
  void OnUploadProgress(uint64_t position, uint64_t size) override;
  bool OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          const ResourceResponseInfo& info) override;
  void OnReceivedResponse(const ResourceResponseInfo& info) override;
  void OnDownloadedData(int len, int encoded_data_length) override;
  void OnReceivedData(std::unique_ptr<ReceivedData> data) override;
  void OnReceivedCachedMetadata(const char* data, int len) override;
  void OnCompletedRequest(int error_code,
                          bool was_ignored_by_handler,
                          bool stale_copy_in_cache,
                          const base::TimeTicks& completion_time,
                          int64_t total_transfer_size) override;

 private:
  scoped_refptr<Context> context_;
  DISALLOW_COPY_AND_ASSIGN(RequestPeerImpl);
};

// WebURLLoaderImpl::Context --------------------------------------------------

WebURLLoaderImpl::Context::Context(WebURLLoaderImpl* loader,
                                   ResourceDispatcher* resource_dispatcher,
                                   mojom::URLLoaderFactory* url_loader_factory,
                                   mojo::AssociatedGroup* associated_group)
    : loader_(loader),
      client_(NULL),
      resource_dispatcher_(resource_dispatcher),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      defers_loading_(NOT_DEFERRING),
      request_id_(-1),
      url_loader_factory_(url_loader_factory),
      associated_group_(associated_group) {}

void WebURLLoaderImpl::Context::Cancel() {
  TRACE_EVENT_WITH_FLOW0("loading", "WebURLLoaderImpl::Context::Cancel", this,
                         TRACE_EVENT_FLAG_FLOW_IN);
  if (resource_dispatcher_ && // NULL in unittest.
      request_id_ != -1) {
    resource_dispatcher_->Cancel(request_id_);
    request_id_ = -1;
  }

  if (body_stream_writer_)
    body_stream_writer_->Fail();

  // Ensure that we do not notify the delegate anymore as it has
  // its own pointer to the client.
  if (ftp_listing_delegate_)
    ftp_listing_delegate_->Cancel();

  // Do not make any further calls to the client.
  client_ = NULL;
  loader_ = NULL;
}

void WebURLLoaderImpl::Context::SetDefersLoading(bool value) {
  if (request_id_ != -1)
    resource_dispatcher_->SetDefersLoading(request_id_, value);
  if (value && defers_loading_ == NOT_DEFERRING) {
    defers_loading_ = SHOULD_DEFER;
  } else if (!value && defers_loading_ != NOT_DEFERRING) {
    if (defers_loading_ == DEFERRED_DATA) {
      task_runner_->PostTask(FROM_HERE,
                             base::Bind(&Context::HandleDataURL, this));
    }
    defers_loading_ = NOT_DEFERRING;
  }
}

void WebURLLoaderImpl::Context::DidChangePriority(
    WebURLRequest::Priority new_priority, int intra_priority_value) {
  if (request_id_ != -1) {
    resource_dispatcher_->DidChangePriority(
        request_id_,
        ConvertWebKitPriorityToNetPriority(new_priority),
        intra_priority_value);
  }
}

void WebURLLoaderImpl::Context::Start(const WebURLRequest& request,
                                      SyncLoadResponse* sync_load_response) {
  DCHECK(request_id_ == -1);
  request_ = request;  // Save the request.
  GURL url = request.url();

  if (CanHandleDataURLRequestLocally()) {
    if (sync_load_response) {
      // This is a sync load. Do the work now.
      sync_load_response->url = url;
      sync_load_response->error_code =
          GetInfoFromDataURL(sync_load_response->url, sync_load_response,
                             &sync_load_response->data);
    } else {
      task_runner_->PostTask(FROM_HERE,
                             base::Bind(&Context::HandleDataURL, this));
    }
    return;
  }

  if (request.getExtraData()) {
    RequestExtraData* extra_data =
        static_cast<RequestExtraData*>(request.getExtraData());
    stream_override_ = extra_data->TakeStreamOverrideOwnership();
  }


  // PlzNavigate: outside of tests, the only navigation requests going through
  // the WebURLLoader are the ones created by CommitNavigation. Several browser
  // tests load HTML directly through a data url which will be handled by the
  // block above.
  DCHECK(!IsBrowserSideNavigationEnabled() || stream_override_.get() ||
         request.getFrameType() == WebURLRequest::FrameTypeNone);

  GURL referrer_url(
      request.httpHeaderField(WebString::fromUTF8("Referer")).latin1());
  const std::string& method = request.httpMethod().latin1();

  // TODO(brettw) this should take parameter encoding into account when
  // creating the GURLs.

  // TODO(horo): Check credentials flag is unset when credentials mode is omit.
  //             Check credentials flag is set when credentials mode is include.

  std::unique_ptr<ResourceRequest> resource_request(new ResourceRequest);

  resource_request->method = method;
  resource_request->url = url;
  resource_request->first_party_for_cookies = request.firstPartyForCookies();
  resource_request->request_initiator =
      request.requestorOrigin().isNull()
          ? base::Optional<url::Origin>()
          : base::Optional<url::Origin>(request.requestorOrigin());
  resource_request->referrer = referrer_url;

  resource_request->referrer_policy = request.referrerPolicy();

  resource_request->headers = GetWebURLRequestHeaders(request);
  resource_request->load_flags = GetLoadFlagsForWebURLRequest(request);
  // origin_pid only needs to be non-zero if the request originates outside
  // the render process, so we can use requestorProcessID even for requests
  // from in-process plugins.
  resource_request->origin_pid = request.requestorProcessID();
  resource_request->resource_type = WebURLRequestToResourceType(request);
  resource_request->priority =
      ConvertWebKitPriorityToNetPriority(request.getPriority());
  resource_request->appcache_host_id = request.appCacheHostID();
  resource_request->should_reset_appcache = request.shouldResetAppCache();
  resource_request->skip_service_worker =
      GetSkipServiceWorkerForWebURLRequest(request);
  resource_request->fetch_request_mode =
      GetFetchRequestModeForWebURLRequest(request);
  resource_request->fetch_credentials_mode =
      GetFetchCredentialsModeForWebURLRequest(request);
  resource_request->fetch_redirect_mode =
      GetFetchRedirectModeForWebURLRequest(request);
  resource_request->fetch_request_context_type =
      GetRequestContextTypeForWebURLRequest(request);
  resource_request->fetch_frame_type =
      GetRequestContextFrameTypeForWebURLRequest(request);
  resource_request->request_body =
      GetRequestBodyForWebURLRequest(request).get();
  resource_request->download_to_file = request.downloadToFile();
  resource_request->has_user_gesture = request.hasUserGesture();
  resource_request->enable_load_timing = true;
  resource_request->enable_upload_progress = request.reportUploadProgress();
  if (request.getRequestContext() ==
          WebURLRequest::RequestContextXMLHttpRequest &&
      (url.has_username() || url.has_password())) {
    resource_request->do_not_prompt_for_login = true;
  }
  resource_request->report_raw_headers = request.reportRawHeaders();
  resource_request->lofi_state = static_cast<LoFiState>(request.getLoFiState());

  // PlzNavigate: during navigation, the renderer should request a stream which
  // contains the body of the response. The network request has already been
  // made by the browser.
  if (stream_override_.get()) {
    CHECK(IsBrowserSideNavigationEnabled());
    DCHECK(!sync_load_response);
    DCHECK_NE(WebURLRequest::FrameTypeNone, request.getFrameType());
    resource_request->resource_body_stream_url = stream_override_->stream_url;
  }

  const RequestExtraData empty_extra_data;
  const RequestExtraData* extra_data;
  if (request.getExtraData())
    extra_data = static_cast<RequestExtraData*>(request.getExtraData());
  else
    extra_data = &empty_extra_data;
  extra_data->CopyToResourceRequest(resource_request.get());

  if (sync_load_response) {
    DCHECK(defers_loading_ == NOT_DEFERRING);
    resource_dispatcher_->StartSync(
        std::move(resource_request), request.requestorID(), sync_load_response,
        request.getLoadingIPCType(), url_loader_factory_);
    return;
  }

  TRACE_EVENT_WITH_FLOW0("loading", "WebURLLoaderImpl::Context::Start", this,
                         TRACE_EVENT_FLAG_FLOW_OUT);
  request_id_ = resource_dispatcher_->StartAsync(
      std::move(resource_request), request.requestorID(), task_runner_,
      extra_data->frame_origin(),
      base::MakeUnique<WebURLLoaderImpl::RequestPeerImpl>(this),
      request.getLoadingIPCType(), url_loader_factory_, associated_group_);

  if (defers_loading_ != NOT_DEFERRING)
    resource_dispatcher_->SetDefersLoading(request_id_, true);
}

void WebURLLoaderImpl::Context::SetTaskRunner(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  task_runner_ = task_runner;
}

void WebURLLoaderImpl::Context::OnUploadProgress(uint64_t position,
                                                 uint64_t size) {
  if (client_)
    client_->didSendData(loader_, position, size);
}

bool WebURLLoaderImpl::Context::OnReceivedRedirect(
    const net::RedirectInfo& redirect_info,
    const ResourceResponseInfo& info) {
  if (!client_)
    return false;

  TRACE_EVENT_WITH_FLOW0(
      "loading", "WebURLLoaderImpl::Context::OnReceivedRedirect",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  WebURLResponse response;
  PopulateURLResponse(request_.url(), info, &response,
                      request_.reportRawHeaders());

  WebURLRequest new_request = PopulateURLRequestForRedirect(
      request_, redirect_info,
      info.was_fetched_via_service_worker
          ? blink::WebURLRequest::SkipServiceWorker::None
          : blink::WebURLRequest::SkipServiceWorker::All);

  bool follow = client_->willFollowRedirect(loader_, new_request, response);
  if (!follow) {
    request_ = WebURLRequest();

    return false;
  }

  DCHECK(WebURL(redirect_info.new_url) == new_request.url());

  request_ = new_request;

  // First-party cookie logic moved from DocumentLoader in Blink to
  // net::URLRequest in the browser. Assert that Blink didn't try to change it
  // to something else.
  DCHECK_EQ(redirect_info.new_first_party_for_cookies.spec(),
            request_.firstPartyForCookies().string().utf8());
  return true;
}

void WebURLLoaderImpl::Context::OnReceivedResponse(
    const ResourceResponseInfo& initial_info) {
  if (!client_)
    return;

  TRACE_EVENT_WITH_FLOW0(
      "loading", "WebURLLoaderImpl::Context::OnReceivedResponse",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  ResourceResponseInfo info = initial_info;

  // PlzNavigate: during navigations, the ResourceResponse has already been
  // received on the browser side, and has been passed down to the renderer.
  if (stream_override_.get()) {
    CHECK(IsBrowserSideNavigationEnabled());
    info = stream_override_->response;
  }

  WebURLResponse response;
  PopulateURLResponse(request_.url(), info, &response,
                      request_.reportRawHeaders());

  if (stream_override_.get()) {
    CHECK(IsBrowserSideNavigationEnabled());
    DCHECK(stream_override_->redirect_responses.size() ==
           stream_override_->redirects.size());
    for (size_t i = 0; i < stream_override_->redirects.size(); ++i) {
      WebURLResponse previous_response;
      // TODO(arthursonzogni) Once Devtool is supported by PlzNavigate, the
      // |report_raw_header| argument must be checked.
      WebURLLoaderImpl::PopulateURLResponse(
          stream_override_->redirects[i],
          stream_override_->redirect_responses[i],
          &previous_response,
          request_.reportRawHeaders());
      response.appendRedirectResponse(previous_response);
    }
  }

  bool show_raw_listing = (GURL(request_.url()).query() == "raw");

  if (info.mime_type == "text/vnd.chromium.ftp-dir") {
    if (show_raw_listing) {
      // Set the MIME type to plain text to prevent any active content.
      response.setMIMEType("text/plain");
    } else {
      // We're going to produce a parsed listing in HTML.
      response.setMIMEType("text/html");
    }
  }
  if (info.headers.get() && info.mime_type == "multipart/x-mixed-replace") {
    std::string content_type;
    info.headers->EnumerateHeader(NULL, "content-type", &content_type);

    std::string mime_type;
    std::string charset;
    bool had_charset = false;
    std::string boundary;
    net::HttpUtil::ParseContentType(content_type, &mime_type, &charset,
                                    &had_charset, &boundary);
    base::TrimString(boundary, " \"", &boundary);
    response.setMultipartBoundary(boundary.data(), boundary.size());
  }

  if (request_.useStreamOnResponse()) {
    SharedMemoryDataConsumerHandle::BackpressureMode mode =
        SharedMemoryDataConsumerHandle::kDoNotApplyBackpressure;
    if (info.headers &&
        info.headers->HasHeaderValue("Cache-Control", "no-store")) {
      mode = SharedMemoryDataConsumerHandle::kApplyBackpressure;
    }

    auto read_handle = base::MakeUnique<SharedMemoryDataConsumerHandle>(
        mode, base::Bind(&Context::CancelBodyStreaming, this),
        &body_stream_writer_);

    // Here |body_stream_writer_| has an indirect reference to |this| and that
    // creates a reference cycle, but it is not a problem because the cycle
    // will break if one of the following happens:
    //  1) The body data transfer is done (with or without an error).
    //  2) |read_handle| (and its reader) is detached.

    // The client takes |read_handle|'s ownership.
    client_->didReceiveResponse(loader_, response, read_handle.release());
    // TODO(yhirano): Support ftp listening and multipart
    return;
  } else {
    client_->didReceiveResponse(loader_, response);
  }

  // We may have been cancelled after didReceiveResponse, which would leave us
  // without a client and therefore without much need to do further handling.
  if (!client_)
    return;

  DCHECK(!ftp_listing_delegate_.get());
  if (info.mime_type == "text/vnd.chromium.ftp-dir" && !show_raw_listing) {
    ftp_listing_delegate_.reset(
        new FtpDirectoryListingResponseDelegate(client_, loader_, response));
  }
}

void WebURLLoaderImpl::Context::OnDownloadedData(int len,
                                                 int encoded_data_length) {
  if (client_)
    client_->didDownloadData(loader_, len, encoded_data_length);
}

void WebURLLoaderImpl::Context::OnReceivedData(
    std::unique_ptr<ReceivedData> data) {
  const char* payload = data->payload();
  int data_length = data->length();
  int encoded_data_length = data->encoded_data_length();
  if (!client_)
    return;

  TRACE_EVENT_WITH_FLOW0(
      "loading", "WebURLLoaderImpl::Context::OnReceivedData",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  if (ftp_listing_delegate_) {
    // The FTP listing delegate will make the appropriate calls to
    // client_->didReceiveData and client_->didReceiveResponse.
    ftp_listing_delegate_->OnReceivedData(payload, data_length);
  } else {
    // We dispatch the data even when |useStreamOnResponse()| is set, in order
    // to make Devtools work.
    client_->didReceiveData(loader_, payload, data_length, encoded_data_length,
                            data->encoded_body_length());

    if (request_.useStreamOnResponse()) {
      // We don't support ftp_listening_delegate_ for now.
      // TODO(yhirano): Support ftp listening.
      body_stream_writer_->AddData(std::move(data));
    }
  }
}

void WebURLLoaderImpl::Context::OnReceivedCachedMetadata(
    const char* data, int len) {
  if (!client_)
    return;
  TRACE_EVENT_WITH_FLOW0(
      "loading", "WebURLLoaderImpl::Context::OnReceivedCachedMetadata",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  client_->didReceiveCachedMetadata(loader_, data, len);
}

void WebURLLoaderImpl::Context::OnCompletedRequest(
    int error_code,
    bool was_ignored_by_handler,
    bool stale_copy_in_cache,
    const base::TimeTicks& completion_time,
    int64_t total_transfer_size) {
  if (ftp_listing_delegate_) {
    ftp_listing_delegate_->OnCompletedRequest();
    ftp_listing_delegate_.reset(NULL);
  }

  if (body_stream_writer_ && error_code != net::OK)
    body_stream_writer_->Fail();
  body_stream_writer_.reset();

  if (client_) {
    TRACE_EVENT_WITH_FLOW0(
        "loading", "WebURLLoaderImpl::Context::OnCompletedRequest",
        this, TRACE_EVENT_FLAG_FLOW_IN);

    if (error_code != net::OK) {
      client_->didFail(
          loader_,
          CreateWebURLError(request_.url(), stale_copy_in_cache, error_code,
              was_ignored_by_handler));
    } else {
      client_->didFinishLoading(loader_,
                                (completion_time - TimeTicks()).InSecondsF(),
                                total_transfer_size);
    }
  }
}

WebURLLoaderImpl::Context::~Context() {
  // We must be already cancelled at this point.
  DCHECK_LT(request_id_, 0);
}

void WebURLLoaderImpl::Context::CancelBodyStreaming() {
  scoped_refptr<Context> protect(this);

  // Notify renderer clients that the request is canceled.
  if (ftp_listing_delegate_) {
    ftp_listing_delegate_->OnCompletedRequest();
    ftp_listing_delegate_.reset(NULL);
  }

  if (body_stream_writer_) {
    body_stream_writer_->Fail();
    body_stream_writer_.reset();
  }
  if (client_) {
    // TODO(yhirano): Set |stale_copy_in_cache| appropriately if possible.
    client_->didFail(
        loader_, CreateWebURLError(request_.url(), false, net::ERR_ABORTED));
  }

  // Notify the browser process that the request is canceled.
  Cancel();
}

bool WebURLLoaderImpl::Context::CanHandleDataURLRequestLocally() const {
  GURL url = request_.url();
  if (!url.SchemeIs(url::kDataScheme))
    return false;

  // The fast paths for data URL, Start() and HandleDataURL(), don't support
  // the downloadToFile option.
  if (request_.downloadToFile())
    return false;

  // Data url requests from object tags may need to be intercepted as streams
  // and so need to be sent to the browser.
  if (request_.getRequestContext() == WebURLRequest::RequestContextObject)
    return false;

  // Optimize for the case where we can handle a data URL locally.  We must
  // skip this for data URLs targetted at frames since those could trigger a
  // download.
  //
  // NOTE: We special case MIME types we can render both for performance
  // reasons as well as to support unit tests.

#if defined(OS_ANDROID)
  // For compatibility reasons on Android we need to expose top-level data://
  // to the browser. In tests resource_dispatcher_ can be null, and test pages
  // need to be loaded locally.
  if (resource_dispatcher_ &&
      request_.getFrameType() == WebURLRequest::FrameTypeTopLevel)
    return false;
#endif

  if (request_.getFrameType() != WebURLRequest::FrameTypeTopLevel &&
      request_.getFrameType() != WebURLRequest::FrameTypeNested)
    return true;

  std::string mime_type, unused_charset;
  if (net::DataURL::Parse(request_.url(), &mime_type, &unused_charset, NULL) &&
      mime_util::IsSupportedMimeType(mime_type))
    return true;

  return false;
}

void WebURLLoaderImpl::Context::HandleDataURL() {
  DCHECK_NE(defers_loading_, DEFERRED_DATA);
  if (defers_loading_ == SHOULD_DEFER) {
      defers_loading_ = DEFERRED_DATA;
      return;
  }

  ResourceResponseInfo info;
  std::string data;

  int error_code = GetInfoFromDataURL(request_.url(), &info, &data);

  if (error_code == net::OK) {
    OnReceivedResponse(info);
    auto size = data.size();
    if (size != 0)
      OnReceivedData(
          base::MakeUnique<FixedReceivedData>(data.data(), size, 0, size));
  }

  OnCompletedRequest(error_code, false, false, base::TimeTicks::Now(), 0);
}

// WebURLLoaderImpl::RequestPeerImpl ------------------------------------------

WebURLLoaderImpl::RequestPeerImpl::RequestPeerImpl(Context* context)
    : context_(context) {}

void WebURLLoaderImpl::RequestPeerImpl::OnUploadProgress(uint64_t position,
                                                         uint64_t size) {
  context_->OnUploadProgress(position, size);
}

bool WebURLLoaderImpl::RequestPeerImpl::OnReceivedRedirect(
    const net::RedirectInfo& redirect_info,
    const ResourceResponseInfo& info) {
  return context_->OnReceivedRedirect(redirect_info, info);
}

void WebURLLoaderImpl::RequestPeerImpl::OnReceivedResponse(
    const ResourceResponseInfo& info) {
  context_->OnReceivedResponse(info);
}

void WebURLLoaderImpl::RequestPeerImpl::OnDownloadedData(
    int len,
    int encoded_data_length) {
  context_->OnDownloadedData(len, encoded_data_length);
}

void WebURLLoaderImpl::RequestPeerImpl::OnReceivedData(
    std::unique_ptr<ReceivedData> data) {
  context_->OnReceivedData(std::move(data));
}

void WebURLLoaderImpl::RequestPeerImpl::OnReceivedCachedMetadata(
    const char* data,
    int len) {
  context_->OnReceivedCachedMetadata(data, len);
}

void WebURLLoaderImpl::RequestPeerImpl::OnCompletedRequest(
    int error_code,
    bool was_ignored_by_handler,
    bool stale_copy_in_cache,
    const base::TimeTicks& completion_time,
    int64_t total_transfer_size) {
  context_->OnCompletedRequest(error_code, was_ignored_by_handler,
                               stale_copy_in_cache, completion_time,
                               total_transfer_size);
}

// WebURLLoaderImpl -----------------------------------------------------------

WebURLLoaderImpl::WebURLLoaderImpl(ResourceDispatcher* resource_dispatcher,
                                   mojom::URLLoaderFactory* url_loader_factory,
                                   mojo::AssociatedGroup* associated_group)
    : context_(new Context(this,
                           resource_dispatcher,
                           url_loader_factory,
                           associated_group)) {}

WebURLLoaderImpl::~WebURLLoaderImpl() {
  cancel();
}

void WebURLLoaderImpl::PopulateURLResponse(const GURL& url,
                                           const ResourceResponseInfo& info,
                                           WebURLResponse* response,
                                           bool report_security_info) {
  response->setURL(url);
  response->setResponseTime(info.response_time.ToInternalValue());
  response->setMIMEType(WebString::fromUTF8(info.mime_type));
  response->setTextEncodingName(WebString::fromUTF8(info.charset));
  response->setExpectedContentLength(info.content_length);
  response->setHasMajorCertificateErrors(info.has_major_certificate_errors);
  response->setAppCacheID(info.appcache_id);
  response->setAppCacheManifestURL(info.appcache_manifest_url);
  response->setWasCached(!info.load_timing.request_start_time.is_null() &&
      info.response_time < info.load_timing.request_start_time);
  response->setRemoteIPAddress(
      WebString::fromUTF8(info.socket_address.HostForURL()));
  response->setRemotePort(info.socket_address.port());
  response->setConnectionID(info.load_timing.socket_log_id);
  response->setConnectionReused(info.load_timing.socket_reused);
  response->setDownloadFilePath(info.download_file_path.AsUTF16Unsafe());
  response->setWasFetchedViaSPDY(info.was_fetched_via_spdy);
  response->setWasFetchedViaServiceWorker(info.was_fetched_via_service_worker);
  response->setWasFetchedViaForeignFetch(info.was_fetched_via_foreign_fetch);
  response->setWasFallbackRequiredByServiceWorker(
      info.was_fallback_required_by_service_worker);
  response->setServiceWorkerResponseType(info.response_type_via_service_worker);
  response->setOriginalURLViaServiceWorker(
      info.original_url_via_service_worker);
  response->setCacheStorageCacheName(
      info.is_in_cache_storage
          ? blink::WebString::fromUTF8(info.cache_storage_cache_name)
          : blink::WebString());
  blink::WebVector<blink::WebString> cors_exposed_header_names(
      info.cors_exposed_header_names.size());
  std::transform(
      info.cors_exposed_header_names.begin(),
      info.cors_exposed_header_names.end(), cors_exposed_header_names.begin(),
      [](const std::string& h) { return blink::WebString::fromLatin1(h); });
  response->setCorsExposedHeaderNames(cors_exposed_header_names);
  response->addToEncodedDataLength(info.encoded_data_length);

  SetSecurityStyleAndDetails(url, info, response, report_security_info);

  WebURLResponseExtraDataImpl* extra_data =
      new WebURLResponseExtraDataImpl(info.alpn_negotiated_protocol);
  response->setExtraData(extra_data);
  extra_data->set_was_fetched_via_spdy(info.was_fetched_via_spdy);
  extra_data->set_was_alpn_negotiated(info.was_alpn_negotiated);
  extra_data->set_was_alternate_protocol_available(
      info.was_alternate_protocol_available);
  extra_data->set_connection_info(info.connection_info);
  extra_data->set_is_using_lofi(info.is_using_lofi);
  extra_data->set_effective_connection_type(info.effective_connection_type);

  // If there's no received headers end time, don't set load timing.  This is
  // the case for non-HTTP requests, requests that don't go over the wire, and
  // certain error cases.
  if (!info.load_timing.receive_headers_end.is_null()) {
    WebURLLoadTiming timing;
    PopulateURLLoadTiming(info.load_timing, &timing);
    const TimeTicks kNullTicks;
    timing.setWorkerStart(
        (info.service_worker_start_time - kNullTicks).InSecondsF());
    timing.setWorkerReady(
        (info.service_worker_ready_time - kNullTicks).InSecondsF());
    response->setLoadTiming(timing);
  }

  if (info.devtools_info.get()) {
    WebHTTPLoadInfo load_info;

    load_info.setHTTPStatusCode(info.devtools_info->http_status_code);
    load_info.setHTTPStatusText(WebString::fromLatin1(
        info.devtools_info->http_status_text));
    load_info.setEncodedDataLength(info.encoded_data_length);

    load_info.setRequestHeadersText(WebString::fromLatin1(
        info.devtools_info->request_headers_text));
    load_info.setResponseHeadersText(WebString::fromLatin1(
        info.devtools_info->response_headers_text));
    const HeadersVector& request_headers = info.devtools_info->request_headers;
    for (HeadersVector::const_iterator it = request_headers.begin();
         it != request_headers.end(); ++it) {
      load_info.addRequestHeader(WebString::fromLatin1(it->first),
          WebString::fromLatin1(it->second));
    }
    const HeadersVector& response_headers =
        info.devtools_info->response_headers;
    for (HeadersVector::const_iterator it = response_headers.begin();
         it != response_headers.end(); ++it) {
      load_info.addResponseHeader(WebString::fromLatin1(it->first),
          WebString::fromLatin1(it->second));
    }
    load_info.setNPNNegotiatedProtocol(
        WebString::fromLatin1(info.alpn_negotiated_protocol));
    response->setHTTPLoadInfo(load_info);
  }

  const net::HttpResponseHeaders* headers = info.headers.get();
  if (!headers)
    return;

  WebURLResponse::HTTPVersion version = WebURLResponse::HTTPVersionUnknown;
  if (headers->GetHttpVersion() == net::HttpVersion(0, 9))
    version = WebURLResponse::HTTPVersion_0_9;
  else if (headers->GetHttpVersion() == net::HttpVersion(1, 0))
    version = WebURLResponse::HTTPVersion_1_0;
  else if (headers->GetHttpVersion() == net::HttpVersion(1, 1))
    version = WebURLResponse::HTTPVersion_1_1;
  else if (headers->GetHttpVersion() == net::HttpVersion(2, 0))
    version = WebURLResponse::HTTPVersion_2_0;
  response->setHTTPVersion(version);
  response->setHTTPStatusCode(headers->response_code());
  response->setHTTPStatusText(WebString::fromLatin1(headers->GetStatusText()));

  // TODO(darin): We should leverage HttpResponseHeaders for this, and this
  // should be using the same code as ResourceDispatcherHost.
  // TODO(jungshik): Figure out the actual value of the referrer charset and
  // pass it to GetSuggestedFilename.
  std::string value;
  headers->EnumerateHeader(NULL, "content-disposition", &value);
  response->setSuggestedFileName(
      net::GetSuggestedFilename(url,
                                value,
                                std::string(),  // referrer_charset
                                std::string(),  // suggested_name
                                std::string(),  // mime_type
                                std::string()));  // default_name

  Time time_val;
  if (headers->GetLastModifiedValue(&time_val))
    response->setLastModifiedDate(time_val.ToDoubleT());

  // Build up the header map.
  size_t iter = 0;
  std::string name;
  while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
    response->addHTTPHeaderField(WebString::fromLatin1(name),
                                 WebString::fromLatin1(value));
  }
}

WebURLRequest WebURLLoaderImpl::PopulateURLRequestForRedirect(
    const blink::WebURLRequest& request,
    const net::RedirectInfo& redirect_info,
    blink::WebURLRequest::SkipServiceWorker skip_service_worker) {
  // TODO(darin): We lack sufficient information to construct the actual
  // request that resulted from the redirect.
  WebURLRequest new_request(redirect_info.new_url);
  new_request.setFirstPartyForCookies(
      redirect_info.new_first_party_for_cookies);
  new_request.setDownloadToFile(request.downloadToFile());
  new_request.setUseStreamOnResponse(request.useStreamOnResponse());
  new_request.setRequestContext(request.getRequestContext());
  new_request.setFrameType(request.getFrameType());
  new_request.setSkipServiceWorker(skip_service_worker);
  new_request.setShouldResetAppCache(request.shouldResetAppCache());
  new_request.setFetchRequestMode(request.getFetchRequestMode());
  new_request.setFetchCredentialsMode(request.getFetchCredentialsMode());

  new_request.setHTTPReferrer(WebString::fromUTF8(redirect_info.new_referrer),
                               NetReferrerPolicyToBlinkReferrerPolicy(
                                   redirect_info.new_referrer_policy));
  new_request.setPriority(request.getPriority());

  std::string old_method = request.httpMethod().utf8();
  new_request.setHTTPMethod(WebString::fromUTF8(redirect_info.new_method));
  if (redirect_info.new_method == old_method)
    new_request.setHTTPBody(request.httpBody());
  return new_request;
}

void WebURLLoaderImpl::loadSynchronously(const WebURLRequest& request,
                                         WebURLResponse& response,
                                         WebURLError& error,
                                         WebData& data,
                                         int64_t& encoded_data_length) {
  TRACE_EVENT0("loading", "WebURLLoaderImpl::loadSynchronously");
  SyncLoadResponse sync_load_response;
  context_->Start(request, &sync_load_response);

  const GURL& final_url = sync_load_response.url;

  // TODO(tc): For file loads, we may want to include a more descriptive
  // status code or status text.
  int error_code = sync_load_response.error_code;
  if (error_code != net::OK) {
    response.setURL(final_url);
    error.domain = WebString::fromUTF8(net::kErrorDomain);
    error.reason = error_code;
    error.unreachableURL = final_url;
    return;
  }

  PopulateURLResponse(final_url, sync_load_response, &response,
                      request.reportRawHeaders());
  response.addToEncodedBodyLength(sync_load_response.encoded_body_length);
  response.addToDecodedBodyLength(sync_load_response.data.size());
  encoded_data_length = sync_load_response.encoded_data_length;

  data.assign(sync_load_response.data.data(), sync_load_response.data.size());
}

void WebURLLoaderImpl::loadAsynchronously(const WebURLRequest& request,
                                          WebURLLoaderClient* client) {
  TRACE_EVENT_WITH_FLOW0("loading", "WebURLLoaderImpl::loadAsynchronously",
                         this, TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(!context_->client());

  context_->set_client(client);
  context_->Start(request, NULL);
}

void WebURLLoaderImpl::cancel() {
  context_->Cancel();
}

void WebURLLoaderImpl::setDefersLoading(bool value) {
  context_->SetDefersLoading(value);
}

void WebURLLoaderImpl::didChangePriority(WebURLRequest::Priority new_priority,
                                         int intra_priority_value) {
  context_->DidChangePriority(new_priority, intra_priority_value);
}

void WebURLLoaderImpl::setLoadingTaskRunner(
    base::SingleThreadTaskRunner* loading_task_runner) {
  context_->SetTaskRunner(loading_task_runner);
}

}  // namespace content
