// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_MEMORY_CHILD_MEMORY_MESSAGE_FILTER_H_
#define CONTENT_CHILD_MEMORY_CHILD_MEMORY_MESSAGE_FILTER_H_

#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "ipc/message_filter.h"

namespace content {

// This class receives memory messages in child processes.
// See also: memory_message_filter.h
class ChildMemoryMessageFilter : public IPC::MessageFilter {
 public:
  ChildMemoryMessageFilter();

  // IPC::MessageFilter implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

 protected:
  ~ChildMemoryMessageFilter() override;

 private:
  void OnSetPressureNotificationsSuppressed(bool suppressed);
  void OnSimulatePressureNotification(
      base::MemoryPressureListener::MemoryPressureLevel level);
  void OnPressureNotification(
      base::MemoryPressureListener::MemoryPressureLevel level);

  DISALLOW_COPY_AND_ASSIGN(ChildMemoryMessageFilter);
};

}  // namespace content

#endif  // CONTENT_CHILD_MEMORY_CHILD_MEMORY_MESSAGE_FILTER_H_
