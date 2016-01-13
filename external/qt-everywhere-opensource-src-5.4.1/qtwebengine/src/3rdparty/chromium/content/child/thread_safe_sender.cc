// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/thread_safe_sender.h"

#include "base/message_loop/message_loop_proxy.h"
#include "content/child/child_thread.h"
#include "ipc/ipc_sync_message_filter.h"

namespace content {

ThreadSafeSender::ThreadSafeSender(base::MessageLoopProxy* main_loop,
                                   IPC::SyncMessageFilter* sync_filter)
    : main_loop_(main_loop), sync_filter_(sync_filter) {
}

ThreadSafeSender::~ThreadSafeSender() {
}

bool ThreadSafeSender::Send(IPC::Message* msg) {
  if (main_loop_->BelongsToCurrentThread())
    return ChildThread::current()->Send(msg);
  return sync_filter_->Send(msg);
}

}  // namespace content
