// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/websocket_bridge.h"

#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "content/child/child_thread_impl.h"
#include "content/child/websocket_dispatcher.h"
#include "content/common/websocket.h"
#include "content/common/websocket_messages.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/platform/modules/websockets/WebSocketHandle.h"
#include "third_party/WebKit/public/platform/modules/websockets/WebSocketHandleClient.h"
#include "third_party/WebKit/public/platform/modules/websockets/WebSocketHandshakeRequestInfo.h"
#include "third_party/WebKit/public/platform/modules/websockets/WebSocketHandshakeResponseInfo.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::WebSecurityOrigin;
using blink::WebSocketHandle;
using blink::WebSocketHandleClient;
using blink::WebString;
using blink::WebURL;
using blink::WebVector;

namespace content {

namespace {

const unsigned short kAbnormalShutdownOpCode = 1006;

}  // namespace

WebSocketBridge::WebSocketBridge()
    : channel_id_(kInvalidChannelId),
      render_frame_id_(MSG_ROUTING_NONE),
      client_(NULL) {}

WebSocketBridge::~WebSocketBridge() {
  if (channel_id_ != kInvalidChannelId) {
    // The connection is abruptly disconnected by the renderer without
    // closing handshake.
    ChildThreadImpl::current()->Send(
        new WebSocketMsg_DropChannel(channel_id_,
                                     false,
                                     kAbnormalShutdownOpCode,
                                     std::string()));
  }
  Disconnect();
}

bool WebSocketBridge::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebSocketBridge, msg)
    IPC_MESSAGE_HANDLER(WebSocketMsg_AddChannelResponse, DidConnect)
    IPC_MESSAGE_HANDLER(WebSocketMsg_NotifyStartOpeningHandshake,
                        DidStartOpeningHandshake)
    IPC_MESSAGE_HANDLER(WebSocketMsg_NotifyFinishOpeningHandshake,
                        DidFinishOpeningHandshake)
    IPC_MESSAGE_HANDLER(WebSocketMsg_NotifyFailure, DidFail)
    IPC_MESSAGE_HANDLER(WebSocketMsg_SendFrame, DidReceiveData)
    IPC_MESSAGE_HANDLER(WebSocketMsg_FlowControl, DidReceiveFlowControl)
    IPC_MESSAGE_HANDLER(WebSocketMsg_DropChannel, DidClose)
    IPC_MESSAGE_HANDLER(WebSocketMsg_NotifyClosing,
                        DidStartClosingHandshake)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WebSocketBridge::DidConnect(const std::string& selected_protocol,
                                 const std::string& extensions) {
  WebSocketHandleClient* client = client_;
  DVLOG(1) << "WebSocketBridge::DidConnect("
           << selected_protocol << ", "
           << extensions << ")";
  if (!client)
    return;

  WebString protocol_to_pass = WebString::fromUTF8(selected_protocol);
  WebString extensions_to_pass = WebString::fromUTF8(extensions);
  client->didConnect(this, protocol_to_pass, extensions_to_pass);
  // |this| can be deleted here.
}

void WebSocketBridge::DidStartOpeningHandshake(
    const WebSocketHandshakeRequest& request) {
  DVLOG(1) << "WebSocketBridge::DidStartOpeningHandshake("
           << request.url << ")";
  // All strings are already encoded to ASCII in the browser.
  blink::WebSocketHandshakeRequestInfo request_to_pass;
  request_to_pass.setURL(WebURL(request.url));
  for (size_t i = 0; i < request.headers.size(); ++i) {
    const std::pair<std::string, std::string>& header = request.headers[i];
    request_to_pass.addHeaderField(WebString::fromLatin1(header.first),
                                   WebString::fromLatin1(header.second));
  }
  request_to_pass.setHeadersText(WebString::fromLatin1(request.headers_text));
  client_->didStartOpeningHandshake(this, request_to_pass);
}

void WebSocketBridge::DidFinishOpeningHandshake(
    const WebSocketHandshakeResponse& response) {
  DVLOG(1) << "WebSocketBridge::DidFinishOpeningHandshake("
           << response.url << ")";
  // All strings are already encoded to ASCII in the browser.
  blink::WebSocketHandshakeResponseInfo response_to_pass;
  response_to_pass.setStatusCode(response.status_code);
  response_to_pass.setStatusText(WebString::fromLatin1(response.status_text));
  for (size_t i = 0; i < response.headers.size(); ++i) {
    const std::pair<std::string, std::string>& header = response.headers[i];
    response_to_pass.addHeaderField(WebString::fromLatin1(header.first),
                                    WebString::fromLatin1(header.second));
  }
  response_to_pass.setHeadersText(WebString::fromLatin1(response.headers_text));
  client_->didFinishOpeningHandshake(this, response_to_pass);
}

void WebSocketBridge::DidFail(const std::string& message) {
  DVLOG(1) << "WebSocketBridge::DidFail(" << message << ")";
  WebSocketHandleClient* client = client_;
  Disconnect();
  if (!client)
    return;

  WebString message_to_pass = WebString::fromUTF8(message);
  client->didFail(this, message_to_pass);
  // |this| can be deleted here.
}

void WebSocketBridge::DidReceiveData(bool fin,
                                     WebSocketMessageType type,
                                     const std::vector<char>& data) {
  DVLOG(1) << "WebSocketBridge::DidReceiveData("
           << fin << ", "
           << type << ", "
           << "(data size = " << data.size() << "))";
  if (!client_)
    return;

  WebSocketHandle::MessageType type_to_pass =
      WebSocketHandle::MessageTypeContinuation;
  switch (type) {
    case WEB_SOCKET_MESSAGE_TYPE_CONTINUATION:
      type_to_pass = WebSocketHandle::MessageTypeContinuation;
      break;
    case WEB_SOCKET_MESSAGE_TYPE_TEXT:
      type_to_pass = WebSocketHandle::MessageTypeText;
      break;
    case WEB_SOCKET_MESSAGE_TYPE_BINARY:
      type_to_pass = WebSocketHandle::MessageTypeBinary;
      break;
  }
  const char* data_to_pass = data.empty() ? NULL : &data[0];
  client_->didReceiveData(this, fin, type_to_pass, data_to_pass, data.size());
  // |this| can be deleted here.
}

void WebSocketBridge::DidReceiveFlowControl(int64_t quota) {
  DVLOG(1) << "WebSocketBridge::DidReceiveFlowControl(" << quota << ")";
  if (!client_)
    return;

  client_->didReceiveFlowControl(this, quota);
  // |this| can be deleted here.
}

void WebSocketBridge::DidClose(bool was_clean,
                               unsigned short code,
                               const std::string& reason) {
  DVLOG(1) << "WebSocketBridge::DidClose("
           << was_clean << ", "
           << code << ", "
           << reason << ")";
  WebSocketHandleClient* client = client_;
  Disconnect();
  if (!client)
    return;

  WebString reason_to_pass = WebString::fromUTF8(reason);
  client->didClose(this, was_clean, code, reason_to_pass);
  // |this| can be deleted here.
}

void WebSocketBridge::DidStartClosingHandshake() {
  DVLOG(1) << "WebSocketBridge::DidStartClosingHandshake()";
  if (!client_)
    return;

  client_->didStartClosingHandshake(this);
  // |this| can be deleted here.
}

void WebSocketBridge::connect(const WebURL& url,
                              const WebVector<WebString>& protocols,
                              const WebSecurityOrigin& origin,
                              const WebURL& first_party_for_cookies,
                              const WebString& user_agent_override,
                              WebSocketHandleClient* client) {
  DCHECK_EQ(kInvalidChannelId, channel_id_);
  WebSocketDispatcher* dispatcher =
      ChildThreadImpl::current()->websocket_dispatcher();
  channel_id_ = dispatcher->AddBridge(this);
  client_ = client;

  std::vector<std::string> protocols_to_pass;
  for (size_t i = 0; i < protocols.size(); ++i)
    protocols_to_pass.push_back(protocols[i].utf8());

  DVLOG(1) << "Bridge#" << channel_id_ << " Connect(" << url << ", ("
           << base::JoinString(protocols_to_pass, ", ") << "), "
           << origin.toString().utf8() << ")";

  WebSocketHostMsg_AddChannelRequest_Params params;
  params.socket_url = url;
  params.requested_protocols = protocols_to_pass;
  params.origin = origin;
  params.first_party_for_cookies = first_party_for_cookies;
  params.user_agent_override = user_agent_override.latin1();
  params.render_frame_id = render_frame_id_;

  // Headers (ie: User-Agent) are ISO Latin 1.
  ChildThreadImpl::current()->Send(new WebSocketHostMsg_AddChannelRequest(
      channel_id_, params));
}

void WebSocketBridge::send(bool fin,
                           WebSocketHandle::MessageType type,
                           const char* data,
                           size_t size) {
  if (channel_id_ == kInvalidChannelId)
    return;

  WebSocketMessageType type_to_pass = WEB_SOCKET_MESSAGE_TYPE_CONTINUATION;
  switch (type) {
    case WebSocketHandle::MessageTypeContinuation:
      type_to_pass = WEB_SOCKET_MESSAGE_TYPE_CONTINUATION;
      break;
    case WebSocketHandle::MessageTypeText:
      type_to_pass = WEB_SOCKET_MESSAGE_TYPE_TEXT;
      break;
    case WebSocketHandle::MessageTypeBinary:
      type_to_pass = WEB_SOCKET_MESSAGE_TYPE_BINARY;
      break;
  }

  DVLOG(1) << "Bridge #" << channel_id_ << " Send("
           << fin << ", " << type_to_pass << ", "
           << "(data size = "  << size << "))";

  ChildThreadImpl::current()->Send(
      new WebSocketMsg_SendFrame(channel_id_,
                                 fin,
                                 type_to_pass,
                                 std::vector<char>(data, data + size)));
}

void WebSocketBridge::flowControl(int64_t quota) {
  if (channel_id_ == kInvalidChannelId)
    return;

  DVLOG(1) << "Bridge #" << channel_id_ << " FlowControl(" << quota << ")";

  ChildThreadImpl::current()->Send(
      new WebSocketMsg_FlowControl(channel_id_, quota));
}

void WebSocketBridge::close(unsigned short code,
                            const WebString& reason) {
  if (channel_id_ == kInvalidChannelId)
    return;

  std::string reason_to_pass = reason.utf8();
  DVLOG(1) << "Bridge #" << channel_id_ << " Close("
           << code << ", " << reason_to_pass << ")";
  // This method is for closing handshake and hence |was_clean| shall be true.
  ChildThreadImpl::current()->Send(
      new WebSocketMsg_DropChannel(channel_id_, true, code, reason_to_pass));
}

void WebSocketBridge::Disconnect() {
  if (channel_id_ == kInvalidChannelId)
    return;
  WebSocketDispatcher* dispatcher =
      ChildThreadImpl::current()->websocket_dispatcher();
  dispatcher->RemoveBridge(channel_id_);

  channel_id_ = kInvalidChannelId;
  client_ = NULL;
}

}  // namespace content
