// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_resource_message_filter.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/child/resource_dispatcher.h"
#include "content/common/resource_messages.h"

namespace content {

ChildResourceMessageFilter::ChildResourceMessageFilter(
    ResourceDispatcher* resource_dispatcher)
    : main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      resource_dispatcher_(resource_dispatcher) {}

ChildResourceMessageFilter::~ChildResourceMessageFilter() {}

bool ChildResourceMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  if (message.type() == ResourceMsg_RequestComplete::ID ||
      message.type() == ResourceMsg_ReceivedResponse::ID ||
      message.type() == ResourceMsg_ReceivedRedirect::ID) {
    main_thread_task_runner_->PostTask(FROM_HERE, base::Bind(
        &ResourceDispatcher::set_io_timestamp,
        base::Unretained(resource_dispatcher_),
        base::TimeTicks::Now()));
  }
  return false;
}

}  // namespace content
