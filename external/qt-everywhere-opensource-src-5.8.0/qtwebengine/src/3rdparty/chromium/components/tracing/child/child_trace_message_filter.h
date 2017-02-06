// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_CHILD_CHILD_TRACE_MESSAGE_FILTER_H_
#define COMPONENTS_TRACING_CHILD_CHILD_TRACE_MESSAGE_FILTER_H_

#include <stdint.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "components/tracing/tracing_export.h"
#include "ipc/message_filter.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace tracing {

// This class sends and receives trace messages on child processes.
class TRACING_EXPORT ChildTraceMessageFilter : public IPC::MessageFilter {
 public:
  explicit ChildTraceMessageFilter(
      base::SingleThreadTaskRunner* ipc_task_runner);

  // IPC::MessageFilter implementation.
  void OnFilterAdded(IPC::Sender* sender) override;
  void OnFilterRemoved() override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void SendGlobalMemoryDumpRequest(
      const base::trace_event::MemoryDumpRequestArgs& args,
      const base::trace_event::MemoryDumpCallback& callback);

  base::SingleThreadTaskRunner* ipc_task_runner() const {
    return ipc_task_runner_;
  }

 protected:
  ~ChildTraceMessageFilter() override;

 private:
  friend class ChildTraceMessageFilterTest;

  // Message handlers.
  void OnBeginTracing(const std::string& trace_config_str,
                      base::TimeTicks browser_time,
                      uint64_t tracing_process_id);
  void OnEndTracing();
  void OnCancelTracing();
  void OnGetTraceLogStatus();
  void OnSetWatchEvent(const std::string& category_name,
                       const std::string& event_name);
  void OnCancelWatchEvent();
  void OnWatchEventMatched();
  void OnProcessMemoryDumpRequest(
      const base::trace_event::MemoryDumpRequestArgs& args);
  void OnGlobalMemoryDumpResponse(uint64_t dump_guid, bool success);
  void OnSetUMACallback(const std::string& histogram_name,
                        int histogram_lower_value,
                        int histogram_upper_value,
                        bool repeat);
  void OnClearUMACallback(const std::string& histogram_name);
  void OnHistogramChanged(const std::string& histogram_name,
                          base::Histogram::Sample reference_lower_value,
                          base::Histogram::Sample reference_upper_value,
                          bool repeat,
                          base::Histogram::Sample actual_value);
  void SendTriggerMessage(const std::string& histogram_name);
  void SendAbortBackgroundTracingMessage();

  // Callback from trace subsystem.
  void OnTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& events_str_ptr,
      bool has_more_events);

  void OnProcessMemoryDumpDone(uint64_t dump_guid, bool success);

  void SetSenderForTesting(IPC::Sender* sender);

  IPC::Sender* sender_;
  base::SingleThreadTaskRunner* ipc_task_runner_;

  // guid of the outstanding request (to the Browser's MemoryDumpManager), if
  // any. 0 if there is no request pending.
  uint64_t pending_memory_dump_guid_;

  // callback of the outstanding memory dump request, if any.
  base::trace_event::MemoryDumpCallback pending_memory_dump_callback_;

  base::Time histogram_last_changed_;

  DISALLOW_COPY_AND_ASSIGN(ChildTraceMessageFilter);
};

}  // namespace tracing

#endif  // COMPONENTS_TRACING_CHILD_CHILD_TRACE_MESSAGE_FILTER_H_
