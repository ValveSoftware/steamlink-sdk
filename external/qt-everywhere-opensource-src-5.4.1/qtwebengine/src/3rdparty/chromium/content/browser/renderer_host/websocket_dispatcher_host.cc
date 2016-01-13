// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/websocket_dispatcher_host.h"

#include <string>

#include "base/callback.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/websocket_host.h"
#include "content/common/websocket_messages.h"

namespace content {

namespace {

// Many methods defined in this file return a WebSocketHostState enum
// value. Make WebSocketHostState visible at file scope so it doesn't have to be
// fully-qualified every time.
typedef WebSocketDispatcherHost::WebSocketHostState WebSocketHostState;

}  // namespace

WebSocketDispatcherHost::WebSocketDispatcherHost(
    int process_id,
    const GetRequestContextCallback& get_context_callback)
    : BrowserMessageFilter(WebSocketMsgStart),
      process_id_(process_id),
      get_context_callback_(get_context_callback),
      websocket_host_factory_(
          base::Bind(&WebSocketDispatcherHost::CreateWebSocketHost,
                     base::Unretained(this))) {}

WebSocketDispatcherHost::WebSocketDispatcherHost(
    int process_id,
    const GetRequestContextCallback& get_context_callback,
    const WebSocketHostFactory& websocket_host_factory)
    : BrowserMessageFilter(WebSocketMsgStart),
      process_id_(process_id),
      get_context_callback_(get_context_callback),
      websocket_host_factory_(websocket_host_factory) {}

WebSocketHost* WebSocketDispatcherHost::CreateWebSocketHost(int routing_id) {
  return new WebSocketHost(routing_id, this, get_context_callback_.Run());
}

bool WebSocketDispatcherHost::OnMessageReceived(const IPC::Message& message) {
  switch (message.type()) {
    case WebSocketHostMsg_AddChannelRequest::ID:
    case WebSocketMsg_SendFrame::ID:
    case WebSocketMsg_FlowControl::ID:
    case WebSocketMsg_DropChannel::ID:
      break;

    default:
      // Every message that has not been handled by a previous filter passes
      // through here, so it is good to pass them on as efficiently as possible.
      return false;
  }

  int routing_id = message.routing_id();
  WebSocketHost* host = GetHost(routing_id);
  if (message.type() == WebSocketHostMsg_AddChannelRequest::ID) {
    if (host) {
      DVLOG(1) << "routing_id=" << routing_id << " already in use.";
      // The websocket multiplexing spec says to should drop the physical
      // connection in this case, but there isn't a real physical connection
      // to the renderer, and killing the renderer for this would seem to be a
      // little extreme. So for now just ignore the bogus request.
      return true;  // We handled the message (by ignoring it).
    }
    host = websocket_host_factory_.Run(routing_id);
    hosts_.insert(WebSocketHostTable::value_type(routing_id, host));
  }
  if (!host) {
    DVLOG(1) << "Received invalid routing ID " << routing_id
             << " from renderer.";
    return true;  // We handled the message (by ignoring it).
  }
  return host->OnMessageReceived(message);
}

bool WebSocketDispatcherHost::CanReadRawCookies() const {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  return policy->CanReadRawCookies(process_id_);
}

WebSocketHost* WebSocketDispatcherHost::GetHost(int routing_id) const {
  WebSocketHostTable::const_iterator it = hosts_.find(routing_id);
  return it == hosts_.end() ? NULL : it->second;
}

WebSocketHostState WebSocketDispatcherHost::SendOrDrop(IPC::Message* message) {
  const uint32 message_type = message->type();
  const int32 message_routing_id = message->routing_id();
  if (!Send(message)) {
    message = NULL;
    DVLOG(1) << "Sending of message type " << message_type
             << " failed. Dropping channel.";
    DeleteWebSocketHost(message_routing_id);
    return WEBSOCKET_HOST_DELETED;
  }
  return WEBSOCKET_HOST_ALIVE;
}

WebSocketHostState WebSocketDispatcherHost::SendAddChannelResponse(
    int routing_id,
    bool fail,
    const std::string& selected_protocol,
    const std::string& extensions) {
  if (SendOrDrop(new WebSocketMsg_AddChannelResponse(
          routing_id, fail, selected_protocol, extensions)) ==
      WEBSOCKET_HOST_DELETED)
    return WEBSOCKET_HOST_DELETED;
  if (fail) {
    DeleteWebSocketHost(routing_id);
    return WEBSOCKET_HOST_DELETED;
  }
  return WEBSOCKET_HOST_ALIVE;
}

WebSocketHostState WebSocketDispatcherHost::SendFrame(
    int routing_id,
    bool fin,
    WebSocketMessageType type,
    const std::vector<char>& data) {
  return SendOrDrop(new WebSocketMsg_SendFrame(routing_id, fin, type, data));
}

WebSocketHostState WebSocketDispatcherHost::SendFlowControl(int routing_id,
                                                            int64 quota) {
  return SendOrDrop(new WebSocketMsg_FlowControl(routing_id, quota));
}

WebSocketHostState WebSocketDispatcherHost::NotifyClosingHandshake(
    int routing_id) {
  return SendOrDrop(new WebSocketMsg_NotifyClosing(routing_id));
}

WebSocketHostState WebSocketDispatcherHost::NotifyStartOpeningHandshake(
    int routing_id, const WebSocketHandshakeRequest& request) {
  return SendOrDrop(new WebSocketMsg_NotifyStartOpeningHandshake(
      routing_id, request));
}

WebSocketHostState WebSocketDispatcherHost::NotifyFinishOpeningHandshake(
    int routing_id, const WebSocketHandshakeResponse& response) {
  return SendOrDrop(new WebSocketMsg_NotifyFinishOpeningHandshake(
      routing_id, response));
}

WebSocketHostState WebSocketDispatcherHost::NotifyFailure(
    int routing_id,
    const std::string& message) {
  if (SendOrDrop(new WebSocketMsg_NotifyFailure(
          routing_id, message)) == WEBSOCKET_HOST_DELETED) {
    return WEBSOCKET_HOST_DELETED;
  }
  DeleteWebSocketHost(routing_id);
  return WEBSOCKET_HOST_DELETED;
}

WebSocketHostState WebSocketDispatcherHost::DoDropChannel(
    int routing_id,
    bool was_clean,
    uint16 code,
    const std::string& reason) {
  if (SendOrDrop(
          new WebSocketMsg_DropChannel(routing_id, was_clean, code, reason)) ==
      WEBSOCKET_HOST_DELETED)
    return WEBSOCKET_HOST_DELETED;
  DeleteWebSocketHost(routing_id);
  return WEBSOCKET_HOST_DELETED;
}

WebSocketDispatcherHost::~WebSocketDispatcherHost() {
  STLDeleteContainerPairSecondPointers(hosts_.begin(), hosts_.end());
}

void WebSocketDispatcherHost::DeleteWebSocketHost(int routing_id) {
  WebSocketHostTable::iterator it = hosts_.find(routing_id);
  DCHECK(it != hosts_.end());
  delete it->second;
  hosts_.erase(it);
}

}  // namespace content
