// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/websocket_dispatcher.h"

#include <stdint.h>
#include <map>

#include "base/logging.h"
#include "content/child/websocket_bridge.h"
#include "content/common/websocket_messages.h"
#include "ipc/ipc_message.h"
#include "url/gurl.h"

namespace content {

WebSocketDispatcher::WebSocketDispatcher() : channel_id_max_(0) {}

WebSocketDispatcher::~WebSocketDispatcher() {}

int WebSocketDispatcher::AddBridge(WebSocketBridge* bridge) {
  ++channel_id_max_;
  bridges_.insert(std::make_pair(channel_id_max_, bridge));
  return channel_id_max_;
}

void WebSocketDispatcher::RemoveBridge(int channel_id) {
  std::map<int, WebSocketBridge*>::iterator iter = bridges_.find(channel_id);
  if (iter == bridges_.end()) {
    DVLOG(1) << "Remove a non-existent bridge(" << channel_id << ")";
    return;
  }
  bridges_.erase(iter);
}

bool WebSocketDispatcher::OnMessageReceived(const IPC::Message& msg) {
  switch (msg.type()) {
    case WebSocketMsg_AddChannelResponse::ID:
    case WebSocketMsg_NotifyStartOpeningHandshake::ID:
    case WebSocketMsg_NotifyFinishOpeningHandshake::ID:
    case WebSocketMsg_NotifyFailure::ID:
    case WebSocketMsg_SendFrame::ID:
    case WebSocketMsg_FlowControl::ID:
    case WebSocketMsg_DropChannel::ID:
    case WebSocketMsg_NotifyClosing::ID:
      break;
    default:
      return false;
  }

  WebSocketBridge* bridge = GetBridge(msg.routing_id(), msg.type());
  if (!bridge)
    return true;
  return bridge->OnMessageReceived(msg);
}

WebSocketBridge* WebSocketDispatcher::GetBridge(int channel_id, uint32 type) {
  std::map<int, WebSocketBridge*>::iterator iter = bridges_.find(channel_id);
  if (iter == bridges_.end()) {
    DVLOG(1) << "No bridge for channel_id=" << channel_id
             << ", type=" << type;
    return NULL;
  }
  return iter->second;
}

}  // namespace content
