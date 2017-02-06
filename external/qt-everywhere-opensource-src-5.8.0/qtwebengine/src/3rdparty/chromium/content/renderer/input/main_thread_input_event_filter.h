// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_MAIN_THREAD_INPUT_EVENT_FILTER_H_
#define CONTENT_RENDERER_INPUT_MAIN_THREAD_INPUT_EVENT_FILTER_H_

#include <stdint.h>

#include "base/callback.h"
#include "ipc/message_filter.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace IPC {
class Listener;
}

namespace content {

// Used to intercept InputMsg* messages and deliver them to the given listener
// via the provided main thread task runner. Note that usage should be
// restricted to cases where compositor-thread event filtering (via
// |InputEventFilter|) is disabled or otherwise unavailable.
class MainThreadInputEventFilter : public IPC::MessageFilter {
 public:
  MainThreadInputEventFilter(
      const base::Callback<void(const IPC::Message&)>& main_listener,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner);

  // IPC::MessageFilter implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  bool GetSupportedMessageClasses(
      std::vector<uint32_t>* supported_message_classes) const override;

 private:
  ~MainThreadInputEventFilter() override;

  base::Callback<void(const IPC::Message&)> main_listener_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_MAIN_THREAD_INPUT_EVENT_FILTER_H_
