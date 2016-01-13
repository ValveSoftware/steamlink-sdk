// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SOCKET_STREAM_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_SOCKET_STREAM_DISPATCHER_HOST_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/id_map.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/ssl/ssl_error_handler.h"
#include "content/public/browser/browser_message_filter.h"
#include "net/socket_stream/socket_stream.h"

class GURL;

namespace net {
class SSLInfo;
}

namespace content {
class ResourceContext;
class SocketStreamHost;

// Dispatches ViewHostMsg_SocketStream_* messages sent from renderer.
// It also acts as SocketStream::Delegate so that it sends
// ViewMsg_SocketStream_* messages back to renderer.
class SocketStreamDispatcherHost
    : public BrowserMessageFilter,
      public net::SocketStream::Delegate,
      public SSLErrorHandler::Delegate {
 public:
  typedef base::Callback<net::URLRequestContext*(ResourceType::Type)>
      GetRequestContextCallback;
  SocketStreamDispatcherHost(
      int render_process_id,
      const GetRequestContextCallback& request_context_callback,
      ResourceContext* resource_context);

  // BrowserMessageFilter:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Make this object inactive.
  // Remove all active SocketStreamHost objects.
  void Shutdown();

  // SocketStream::Delegate:
  virtual void OnConnected(net::SocketStream* socket,
                           int max_pending_send_allowed) OVERRIDE;
  virtual void OnSentData(net::SocketStream* socket, int amount_sent) OVERRIDE;
  virtual void OnReceivedData(net::SocketStream* socket,
                              const char* data, int len) OVERRIDE;
  virtual void OnClose(net::SocketStream* socket) OVERRIDE;
  virtual void OnError(const net::SocketStream* socket, int error) OVERRIDE;
  virtual void OnSSLCertificateError(net::SocketStream* socket,
                                     const net::SSLInfo& ssl_info,
                                     bool fatal) OVERRIDE;
  virtual bool CanGetCookies(net::SocketStream* socket,
                             const GURL& url) OVERRIDE;
  virtual bool CanSetCookie(net::SocketStream* request,
                            const GURL& url,
                            const std::string& cookie_line,
                            net::CookieOptions* options) OVERRIDE;

  // SSLErrorHandler::Delegate methods:
  virtual void CancelSSLRequest(const GlobalRequestID& id,
                                int error,
                                const net::SSLInfo* ssl_info) OVERRIDE;
  virtual void ContinueSSLRequest(const GlobalRequestID& id) OVERRIDE;

 protected:
  virtual ~SocketStreamDispatcherHost();

 private:
  // Message handlers called by OnMessageReceived.
  void OnConnect(int render_frame_id, const GURL& url, int socket_id);
  void OnSendData(int socket_id, const std::vector<char>& data);
  void OnCloseReq(int socket_id);

  void DeleteSocketStreamHost(int socket_id);

  net::URLRequestContext* GetURLRequestContext();

  IDMap<SocketStreamHost> hosts_;
  int render_process_id_;
  GetRequestContextCallback request_context_callback_;
  ResourceContext* resource_context_;

  base::WeakPtrFactory<SocketStreamDispatcherHost> weak_ptr_factory_;
  bool on_shutdown_;

  DISALLOW_COPY_AND_ASSIGN(SocketStreamDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_SOCKET_STREAM_DISPATCHER_HOST_H_
