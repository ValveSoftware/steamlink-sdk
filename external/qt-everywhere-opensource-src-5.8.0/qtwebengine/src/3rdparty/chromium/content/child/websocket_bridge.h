// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBSOCKET_BRIDGE_H_
#define CONTENT_CHILD_WEBSOCKET_BRIDGE_H_

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

#include "content/common/websocket.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/platform/modules/websockets/WebSocketHandle.h"

namespace blink {
class WebSecurityOrigin;
class WebString;
class WebURL;
}  // namespace blink

namespace content {

class WebSocketBridge : public blink::WebSocketHandle {
 public:
  WebSocketBridge();

  // Handles an IPC message from the browser process.
  bool OnMessageReceived(const IPC::Message& message);

  // WebSocketHandle functions.
  void connect(const blink::WebURL& url,
               const blink::WebVector<blink::WebString>& protocols,
               const blink::WebSecurityOrigin& origin,
               const blink::WebURL& first_party_for_cookies,
               const blink::WebString& user_agent_override,
               blink::WebSocketHandleClient* client) override;
  void send(bool fin,
            WebSocketHandle::MessageType type,
            const char* data,
            size_t size) override;
  void flowControl(int64_t quota) override;
  void close(unsigned short code, const blink::WebString& reason) override;

  void Disconnect();

  void set_render_frame_id(int id) {
    render_frame_id_ = id;
  }

 private:
  ~WebSocketBridge() override;

  void DidConnect(const std::string& selected_protocol,
                  const std::string& extensions);
  void DidStartOpeningHandshake(const WebSocketHandshakeRequest& request);
  void DidFinishOpeningHandshake(const WebSocketHandshakeResponse& response);
  void DidFail(const std::string& message);
  void DidReceiveData(bool fin,
                      WebSocketMessageType type,
                      const std::vector<char>& data);
  void DidReceiveFlowControl(int64_t quota);
  void DidClose(bool was_clean, unsigned short code, const std::string& reason);
  void DidStartClosingHandshake();

  int channel_id_;
  int render_frame_id_;
  blink::WebSocketHandleClient* client_;

  static const int kInvalidChannelId = -1;
};

}  // namespace content

#endif  // CONTENT_CHILD_WEBSOCKET_BRIDGE_H_
