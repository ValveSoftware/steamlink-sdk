// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_CHILD_TRACE_MESSAGE_FILTER_H_
#define COMPONENTS_TRACING_CHILD_TRACE_MESSAGE_FILTER_H_

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "ipc/message_filter.h"

namespace base {
class MessageLoopProxy;
}

namespace tracing {

// This class sends and receives trace messages on child processes.
class ChildTraceMessageFilter : public IPC::MessageFilter {
 public:
  explicit ChildTraceMessageFilter(base::MessageLoopProxy* ipc_message_loop);

  // IPC::MessageFilter implementation.
  virtual void OnFilterAdded(IPC::Sender* sender) OVERRIDE;
  virtual void OnFilterRemoved() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 protected:
  virtual ~ChildTraceMessageFilter();

 private:
  // Message handlers.
  void OnBeginTracing(const std::string& category_filter_str,
                      base::TimeTicks browser_time,
                      int options);
  void OnEndTracing();
  void OnEnableMonitoring(const std::string& category_filter_str,
                          base::TimeTicks browser_time,
                          int options);
  void OnDisableMonitoring();
  void OnCaptureMonitoringSnapshot();
  void OnGetTraceBufferPercentFull();
  void OnSetWatchEvent(const std::string& category_name,
                       const std::string& event_name);
  void OnCancelWatchEvent();
  void OnWatchEventMatched();

  // Callback from trace subsystem.
  void OnTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& events_str_ptr,
      bool has_more_events);

  void OnMonitoringTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& events_str_ptr,
      bool has_more_events);

  IPC::Sender* sender_;
  base::MessageLoopProxy* ipc_message_loop_;

  DISALLOW_COPY_AND_ASSIGN(ChildTraceMessageFilter);
};

}  // namespace tracing

#endif  // COMPONENTS_TRACING_CHILD_TRACE_MESSAGE_FILTER_H_
