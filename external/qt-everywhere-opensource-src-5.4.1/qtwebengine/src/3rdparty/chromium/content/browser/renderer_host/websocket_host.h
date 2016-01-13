// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_WEBSOCKET_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_WEBSOCKET_HOST_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/common/websocket.h"

class GURL;

namespace url {
class Origin;
}  // namespace url

namespace net {
class WebSocketChannel;
class URLRequestContext;
}  // namespace net

namespace IPC {
class Message;
}  // namespace IPC

namespace content {

class WebSocketDispatcherHost;

// Host of net::WebSocketChannel. The lifetime of an instance of this class is
// completely controlled by the WebSocketDispatcherHost object.
class CONTENT_EXPORT WebSocketHost {
 public:
  WebSocketHost(int routing_id,
                WebSocketDispatcherHost* dispatcher,
                net::URLRequestContext* url_request_context);
  virtual ~WebSocketHost();

  // General message dispatch. WebSocketDispatcherHost::OnMessageReceived
  // delegates to this method after looking up the |routing_id|.
  virtual bool OnMessageReceived(const IPC::Message& message);

 private:
  // Handlers for each message type, dispatched by OnMessageReceived(), as
  // defined in content/common/websocket_messages.h

  void OnAddChannelRequest(const GURL& socket_url,
                           const std::vector<std::string>& requested_protocols,
                           const url::Origin& origin,
                           int render_frame_id);

  void OnSendFrame(bool fin,
                   WebSocketMessageType type,
                   const std::vector<char>& data);

  void OnFlowControl(int64 quota);

  void OnDropChannel(bool was_clean, uint16 code, const std::string& reason);

  // The channel we use to send events to the network.
  scoped_ptr<net::WebSocketChannel> channel_;

  // The WebSocketHostDispatcher that created this object.
  WebSocketDispatcherHost* const dispatcher_;

  // The URL request context for the channel.
  net::URLRequestContext* const url_request_context_;

  // The ID used to route messages.
  const int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_WEBSOCKET_HOST_H_
