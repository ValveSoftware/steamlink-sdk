// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/websocket_message_filter.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "content/child/websocket_dispatcher.h"
#include "ipc/ipc_message.h"

namespace content {

WebSocketMessageFilter::WebSocketMessageFilter(
    WebSocketDispatcher* websocket_dispatcher)
  : websocket_dispatcher_(websocket_dispatcher->GetWeakPtr()) {}

WebSocketMessageFilter::~WebSocketMessageFilter() {}

bool WebSocketMessageFilter::OnMessageReceived(const IPC::Message& message) {
  if (!WebSocketDispatcher::CanHandleMessage(message))
    return false;
  loading_task_runner_->PostTask(FROM_HERE, base::Bind(
      &WebSocketMessageFilter::OnMessageReceivedOnMainThread,
      this, message));
  return true;
}

void WebSocketMessageFilter::OnMessageReceivedOnMainThread(
    const IPC::Message& message) {
  if (!websocket_dispatcher_.get())
    return;
  websocket_dispatcher_->OnMessageReceived(message);
}

}  // namespace content
