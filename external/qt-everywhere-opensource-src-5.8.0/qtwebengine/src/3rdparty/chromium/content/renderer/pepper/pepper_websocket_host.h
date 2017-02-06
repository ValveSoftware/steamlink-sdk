// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_WEBSOCKET_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_WEBSOCKET_HOST_H_

#include <stdint.h>

#include <memory>
#include <queue>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/resource_message_params.h"
#include "third_party/WebKit/public/web/WebPepperSocket.h"
#include "third_party/WebKit/public/web/WebPepperSocketClient.h"

namespace ppapi {
class StringVar;
class Var;
}  // namespace ppapi

namespace content {

class RendererPpapiHost;

class CONTENT_EXPORT PepperWebSocketHost
    : public ppapi::host::ResourceHost,
      public NON_EXPORTED_BASE(::blink::WebPepperSocketClient) {
 public:
  explicit PepperWebSocketHost(RendererPpapiHost* host,
                               PP_Instance instance,
                               PP_Resource resource);
  ~PepperWebSocketHost() override;

  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

  // WebPepperSocketClient implementation.
  void didConnect() override;
  void didReceiveMessage(const blink::WebString& message) override;
  void didReceiveArrayBuffer(const blink::WebArrayBuffer& binaryData) override;
  void didReceiveMessageError() override;
  void didUpdateBufferedAmount(unsigned long buffered_amount) override;
  void didStartClosingHandshake() override;
  void didClose(unsigned long unhandled_buffered_amount,
                ClosingHandshakeCompletionStatus status,
                unsigned short code,
                const blink::WebString& reason) override;

 private:
  // IPC message handlers.
  int32_t OnHostMsgConnect(ppapi::host::HostMessageContext* context,
                           const std::string& url,
                           const std::vector<std::string>& protocols);
  int32_t OnHostMsgClose(ppapi::host::HostMessageContext* context,
                         int32_t code,
                         const std::string& reason);
  int32_t OnHostMsgSendText(ppapi::host::HostMessageContext* context,
                            const std::string& message);
  int32_t OnHostMsgSendBinary(ppapi::host::HostMessageContext* context,
                              const std::vector<uint8_t>& message);
  int32_t OnHostMsgFail(ppapi::host::HostMessageContext* context,
                        const std::string& message);

  // Non-owning pointer.
  RendererPpapiHost* renderer_ppapi_host_;

  // IPC reply parameters.
  ppapi::host::ReplyMessageContext connect_reply_;
  ppapi::host::ReplyMessageContext close_reply_;

  // The server URL to which this instance connects.
  std::string url_;

  // A flag to indicate if opening handshake is going on.
  bool connecting_;

  // A flag to indicate if client initiated closing handshake is performed.
  bool initiating_close_;

  // A flag to indicate if server initiated closing handshake is performed.
  bool accepting_close_;

  // Becomes true if any error is detected. Incoming data will be disposed
  // if this variable is true.
  bool error_was_received_;

  // Keeps the WebKit side WebSocket object. This is used for calling WebKit
  // side functions via WebKit API.
  std::unique_ptr<blink::WebPepperSocket> websocket_;

  DISALLOW_COPY_AND_ASSIGN(PepperWebSocketHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_WEBSOCKET_HOST_H_
