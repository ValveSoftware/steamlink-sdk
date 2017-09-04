// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_MEMORY_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_MEMORY_MEMORY_MESSAGE_FILTER_H_

#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/common/process_type.h"

namespace content {

class BrowserChildProcessHost;
class RenderProcessHost;

// This class sends memory messages from the browser process.
// See also: child_memory_message_filter.h
class CONTENT_EXPORT MemoryMessageFilter : public BrowserMessageFilter {
 public:
  MemoryMessageFilter(const BrowserChildProcessHost* child_process_host,
                      ProcessType process_type);
  MemoryMessageFilter(const RenderProcessHost* render_process_host);

  // BrowserMessageFilter implementation.
  void OnFilterAdded(IPC::Channel* channel) override;
  void OnChannelClosing() override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void SendSetPressureNotificationsSuppressed(bool suppressed);
  void SendSimulatePressureNotification(
      base::MemoryPressureListener::MemoryPressureLevel level);
  void SendPressureNotification(
      base::MemoryPressureListener::MemoryPressureLevel level);

 protected:
  friend class MemoryPressureControllerImpl;

  ~MemoryMessageFilter() override;

  const void* process_host() const { return process_host_; }
  ProcessType process_type() const { return process_type_; }

 private:
  // The untyped process host and ProcessType associated with this filter
  // instance. The process host is stored as untyped because it is only used as
  // a key in MemoryPressureControllerImpl; at no point is it ever deferenced to
  // invoke any members on a process host.
  const void* process_host_;
  ProcessType process_type_;

  DISALLOW_COPY_AND_ASSIGN(MemoryMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_MEMORY_MESSAGE_FILTER_H_
