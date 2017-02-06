// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBSOCKET_MESSAGE_FILTER_H_
#define CONTENT_CHILD_WEBSOCKET_MESSAGE_FILTER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "ipc/message_filter.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

class WebSocketDispatcher;

// Delegates IPC messages to a WebSocketDispatcher on the loading task queue
// instead of the default task queue.
class WebSocketMessageFilter : public IPC::MessageFilter {
 public:
  explicit WebSocketMessageFilter(WebSocketDispatcher* websocket_dispatcher);

  // IPC::MessageFilter implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  void SetLoadingTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner) {
    loading_task_runner_ = loading_task_runner;
  }

 private:
  ~WebSocketMessageFilter() override;
  void OnMessageReceivedOnMainThread(const IPC::Message& message);

  scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner_;

  // This actual object is owned by a ChildThreadImpl and is derefed only on the
  // main thread.
  base::WeakPtr<WebSocketDispatcher> websocket_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketMessageFilter);
};

}  // namespace content

#endif  // CONTENT_CHILD_WEBSOCKET_MESSAGE_FILTER_H_
