// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/memory/child_memory_message_filter.h"

#include "content/common/memory_messages.h"

namespace content {

ChildMemoryMessageFilter::ChildMemoryMessageFilter() {}

ChildMemoryMessageFilter::~ChildMemoryMessageFilter() {}

bool ChildMemoryMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChildMemoryMessageFilter, message)
    IPC_MESSAGE_HANDLER(MemoryMsg_SetPressureNotificationsSuppressed,
                        OnSetPressureNotificationsSuppressed)
    IPC_MESSAGE_HANDLER(MemoryMsg_SimulatePressureNotification,
                        OnSimulatePressureNotification)
    IPC_MESSAGE_HANDLER(MemoryMsg_PressureNotification, OnPressureNotification)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ChildMemoryMessageFilter::OnSetPressureNotificationsSuppressed(
    bool suppressed) {
  // Enable/disable suppressing memory notifications in the child process.
  base::MemoryPressureListener::SetNotificationsSuppressed(suppressed);
}

void ChildMemoryMessageFilter::OnSimulatePressureNotification(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  // Pass the message to SimulatePressureNotification. This will emit the
  // message to all listeners even if notifications are suppressed.
  base::MemoryPressureListener::SimulatePressureNotification(level);
}

void ChildMemoryMessageFilter::OnPressureNotification(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  // Forward the message along the normal notification path. If notifications
  // are suppressed then the notification will be swallowed.
  base::MemoryPressureListener::NotifyMemoryPressure(level);
}

}  // namespace content
