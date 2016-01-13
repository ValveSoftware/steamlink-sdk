// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/socket_stream_dispatcher_host.h"

#include <string>

#include "base/logging.h"
#include "content/browser/renderer_host/socket_stream_host.h"
#include "content/browser/ssl/ssl_manager.h"
#include "content/common/resource_messages.h"
#include "content/common/socket_stream.h"
#include "content/common/socket_stream_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/websockets/websocket_job.h"
#include "net/websockets/websocket_throttle.h"

namespace content {

namespace {

const size_t kMaxSocketStreamHosts = 16 * 1024;

}  // namespace

SocketStreamDispatcherHost::SocketStreamDispatcherHost(
    int render_process_id,
    const GetRequestContextCallback& request_context_callback,
    ResourceContext* resource_context)
    : BrowserMessageFilter(SocketStreamMsgStart),
      render_process_id_(render_process_id),
      request_context_callback_(request_context_callback),
      resource_context_(resource_context),
      weak_ptr_factory_(this),
      on_shutdown_(false) {
  net::WebSocketJob::EnsureInit();
}

bool SocketStreamDispatcherHost::OnMessageReceived(
    const IPC::Message& message) {
  if (on_shutdown_)
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SocketStreamDispatcherHost, message)
    IPC_MESSAGE_HANDLER(SocketStreamHostMsg_Connect, OnConnect)
    IPC_MESSAGE_HANDLER(SocketStreamHostMsg_SendData, OnSendData)
    IPC_MESSAGE_HANDLER(SocketStreamHostMsg_Close, OnCloseReq)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

// SocketStream::Delegate methods implementations.
void SocketStreamDispatcherHost::OnConnected(net::SocketStream* socket,
                                             int max_pending_send_allowed) {
  int socket_id = SocketStreamHost::SocketIdFromSocketStream(socket);
  DVLOG(2) << "SocketStreamDispatcherHost::OnConnected socket_id=" << socket_id
           << " max_pending_send_allowed=" << max_pending_send_allowed;
  if (socket_id == kNoSocketId) {
    DVLOG(1) << "NoSocketId in OnConnected";
    return;
  }
  if (!Send(new SocketStreamMsg_Connected(
          socket_id, max_pending_send_allowed))) {
    DVLOG(1) << "SocketStreamMsg_Connected failed.";
    DeleteSocketStreamHost(socket_id);
  }
}

void SocketStreamDispatcherHost::OnSentData(net::SocketStream* socket,
                                            int amount_sent) {
  int socket_id = SocketStreamHost::SocketIdFromSocketStream(socket);
  DVLOG(2) << "SocketStreamDispatcherHost::OnSentData socket_id=" << socket_id
           << " amount_sent=" << amount_sent;
  if (socket_id == kNoSocketId) {
    DVLOG(1) << "NoSocketId in OnSentData";
    return;
  }
  if (!Send(new SocketStreamMsg_SentData(socket_id, amount_sent))) {
    DVLOG(1) << "SocketStreamMsg_SentData failed.";
    DeleteSocketStreamHost(socket_id);
  }
}

void SocketStreamDispatcherHost::OnReceivedData(
    net::SocketStream* socket, const char* data, int len) {
  int socket_id = SocketStreamHost::SocketIdFromSocketStream(socket);
  DVLOG(2) << "SocketStreamDispatcherHost::OnReceiveData socket_id="
           << socket_id;
  if (socket_id == kNoSocketId) {
    DVLOG(1) << "NoSocketId in OnReceivedData";
    return;
  }
  if (!Send(new SocketStreamMsg_ReceivedData(
          socket_id, std::vector<char>(data, data + len)))) {
    DVLOG(1) << "SocketStreamMsg_ReceivedData failed.";
    DeleteSocketStreamHost(socket_id);
  }
}

void SocketStreamDispatcherHost::OnClose(net::SocketStream* socket) {
  int socket_id = SocketStreamHost::SocketIdFromSocketStream(socket);
  DVLOG(2) << "SocketStreamDispatcherHost::OnClosed socket_id=" << socket_id;
  if (socket_id == kNoSocketId) {
    DVLOG(1) << "NoSocketId in OnClose";
    return;
  }
  DeleteSocketStreamHost(socket_id);
}

void SocketStreamDispatcherHost::OnError(const net::SocketStream* socket,
                                         int error) {
  int socket_id = SocketStreamHost::SocketIdFromSocketStream(socket);
  DVLOG(2) << "SocketStreamDispatcherHost::OnError socket_id=" << socket_id;
  if (socket_id == content::kNoSocketId) {
    DVLOG(1) << "NoSocketId in OnError";
    return;
  }
  // SocketStream::Delegate::OnError() events are handled as WebSocket error
  // event when user agent was required to fail WebSocket connection or the
  // WebSocket connection is closed with prejudice.
  if (!Send(new SocketStreamMsg_Failed(socket_id, error))) {
    DVLOG(1) << "SocketStreamMsg_Failed failed.";
    DeleteSocketStreamHost(socket_id);
  }
}

void SocketStreamDispatcherHost::OnSSLCertificateError(
    net::SocketStream* socket, const net::SSLInfo& ssl_info, bool fatal) {
  int socket_id = SocketStreamHost::SocketIdFromSocketStream(socket);
  DVLOG(2) << "SocketStreamDispatcherHost::OnSSLCertificateError socket_id="
           << socket_id;
  if (socket_id == kNoSocketId) {
    DVLOG(1) << "NoSocketId in OnSSLCertificateError";
    return;
  }
  SocketStreamHost* socket_stream_host = hosts_.Lookup(socket_id);
  DCHECK(socket_stream_host);
  GlobalRequestID request_id(-1, socket_id);
  SSLManager::OnSSLCertificateError(
      weak_ptr_factory_.GetWeakPtr(), request_id, ResourceType::SUB_RESOURCE,
      socket->url(), render_process_id_, socket_stream_host->render_frame_id(),
      ssl_info, fatal);
}

bool SocketStreamDispatcherHost::CanGetCookies(net::SocketStream* socket,
                                               const GURL& url) {
  int socket_id = SocketStreamHost::SocketIdFromSocketStream(socket);
  if (socket_id == kNoSocketId) {
    return false;
  }
  SocketStreamHost* socket_stream_host = hosts_.Lookup(socket_id);
  DCHECK(socket_stream_host);
  return GetContentClient()->browser()->AllowGetCookie(
      url,
      url,
      net::CookieList(),
      resource_context_,
      render_process_id_,
      socket_stream_host->render_frame_id());
}

bool SocketStreamDispatcherHost::CanSetCookie(net::SocketStream* request,
                                              const GURL& url,
                                              const std::string& cookie_line,
                                              net::CookieOptions* options) {
  int socket_id = SocketStreamHost::SocketIdFromSocketStream(request);
  if (socket_id == kNoSocketId) {
    return false;
  }
  SocketStreamHost* socket_stream_host = hosts_.Lookup(socket_id);
  DCHECK(socket_stream_host);
  return GetContentClient()->browser()->AllowSetCookie(
      url,
      url,
      cookie_line,
      resource_context_,
      render_process_id_,
      socket_stream_host->render_frame_id(),
      options);
}

void SocketStreamDispatcherHost::CancelSSLRequest(
    const GlobalRequestID& id,
    int error,
    const net::SSLInfo* ssl_info) {
  int socket_id = id.request_id;
  DVLOG(2) << "SocketStreamDispatcherHost::CancelSSLRequest socket_id="
           << socket_id;
  DCHECK_NE(kNoSocketId, socket_id);
  SocketStreamHost* socket_stream_host = hosts_.Lookup(socket_id);
  DCHECK(socket_stream_host);
  if (ssl_info)
    socket_stream_host->CancelWithSSLError(*ssl_info);
  else
    socket_stream_host->CancelWithError(error);
}

void SocketStreamDispatcherHost::ContinueSSLRequest(
    const GlobalRequestID& id) {
  int socket_id = id.request_id;
  DVLOG(2) << "SocketStreamDispatcherHost::ContinueSSLRequest socket_id="
           << socket_id;
  DCHECK_NE(kNoSocketId, socket_id);
  SocketStreamHost* socket_stream_host = hosts_.Lookup(socket_id);
  DCHECK(socket_stream_host);
  socket_stream_host->ContinueDespiteError();
}

SocketStreamDispatcherHost::~SocketStreamDispatcherHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  Shutdown();
}

// Message handlers called by OnMessageReceived.
void SocketStreamDispatcherHost::OnConnect(int render_frame_id,
                                           const GURL& url,
                                           int socket_id) {
  DVLOG(2) << "SocketStreamDispatcherHost::OnConnect"
           << " render_frame_id=" << render_frame_id
           << " url=" << url
           << " socket_id=" << socket_id;
  DCHECK_NE(kNoSocketId, socket_id);

  if (hosts_.size() >= kMaxSocketStreamHosts) {
    if (!Send(new SocketStreamMsg_Failed(socket_id,
                                         net::ERR_TOO_MANY_SOCKET_STREAMS))) {
      DVLOG(1) << "SocketStreamMsg_Failed failed.";
    }
    if (!Send(new SocketStreamMsg_Closed(socket_id))) {
      DVLOG(1) << "SocketStreamMsg_Closed failed.";
    }
    return;
  }

  if (hosts_.Lookup(socket_id)) {
    DVLOG(1) << "socket_id=" << socket_id << " already registered.";
    return;
  }

  // Note that the SocketStreamHost is responsible for checking that |url|
  // is valid.
  SocketStreamHost* socket_stream_host =
      new SocketStreamHost(this, render_process_id_, render_frame_id,
                           socket_id);
  hosts_.AddWithID(socket_stream_host, socket_id);
  socket_stream_host->Connect(url, GetURLRequestContext());
  DVLOG(2) << "SocketStreamDispatcherHost::OnConnect -> " << socket_id;
}

void SocketStreamDispatcherHost::OnSendData(
    int socket_id, const std::vector<char>& data) {
  DVLOG(2) << "SocketStreamDispatcherHost::OnSendData socket_id=" << socket_id;
  SocketStreamHost* socket_stream_host = hosts_.Lookup(socket_id);
  if (!socket_stream_host) {
    DVLOG(1) << "socket_id=" << socket_id << " already closed.";
    return;
  }
  if (!socket_stream_host->SendData(data)) {
    // Cannot accept more data to send.
    socket_stream_host->Close();
  }
}

void SocketStreamDispatcherHost::OnCloseReq(int socket_id) {
  DVLOG(2) << "SocketStreamDispatcherHost::OnCloseReq socket_id=" << socket_id;
  SocketStreamHost* socket_stream_host = hosts_.Lookup(socket_id);
  if (!socket_stream_host)
    return;
  socket_stream_host->Close();
}

void SocketStreamDispatcherHost::DeleteSocketStreamHost(int socket_id) {
  SocketStreamHost* socket_stream_host = hosts_.Lookup(socket_id);
  DCHECK(socket_stream_host);
  delete socket_stream_host;
  hosts_.Remove(socket_id);
  if (!Send(new SocketStreamMsg_Closed(socket_id))) {
    DVLOG(1) << "SocketStreamMsg_Closed failed.";
  }
}

net::URLRequestContext* SocketStreamDispatcherHost::GetURLRequestContext() {
  return request_context_callback_.Run(ResourceType::SUB_RESOURCE);
}

void SocketStreamDispatcherHost::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(ukai): Implement IDMap::RemoveAll().
  for (IDMap<SocketStreamHost>::const_iterator iter(&hosts_);
       !iter.IsAtEnd();
       iter.Advance()) {
    int socket_id = iter.GetCurrentKey();
    const SocketStreamHost* socket_stream_host = iter.GetCurrentValue();
    delete socket_stream_host;
    hosts_.Remove(socket_id);
  }
  on_shutdown_ = true;
}

}  // namespace content
