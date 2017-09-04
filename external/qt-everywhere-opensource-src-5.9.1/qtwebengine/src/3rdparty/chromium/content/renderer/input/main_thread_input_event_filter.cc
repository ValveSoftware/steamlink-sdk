// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/main_thread_input_event_filter.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "content/common/input_messages.h"
#include "ipc/ipc_listener.h"

namespace content {

MainThreadInputEventFilter::MainThreadInputEventFilter(
    const base::Callback<void(const IPC::Message&)>& main_listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner)
    : main_listener_(main_listener), main_task_runner_(main_task_runner) {
  DCHECK(main_task_runner.get());
}

bool MainThreadInputEventFilter::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK_EQ(static_cast<int>(IPC_MESSAGE_ID_CLASS(message.type())),
            InputMsgStart);
  bool handled = main_task_runner_->PostTask(
      FROM_HERE, base::Bind(main_listener_, message));
  return handled;
}

bool MainThreadInputEventFilter::GetSupportedMessageClasses(
    std::vector<uint32_t>* supported_message_classes) const {
  supported_message_classes->push_back(InputMsgStart);
  return true;
}

MainThreadInputEventFilter::~MainThreadInputEventFilter() {
}

}  // namespace content
