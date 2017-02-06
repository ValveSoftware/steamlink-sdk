// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACING_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_TRACING_TRACING_CONTROLLER_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <set>
#include <string>
#include <vector>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/tracing_agent.h"
#include "content/public/browser/tracing_controller.h"

namespace base {
class RefCountedString;
class RefCountedMemory;
}

namespace content {

class TraceMessageFilter;
class TracingUI;

class TracingControllerImpl
    : public TracingController,
      public base::trace_event::MemoryDumpManagerDelegate,
      public base::trace_event::TracingAgent {
 public:
  static TracingControllerImpl* GetInstance();

  // TracingController implementation.
  bool GetCategories(const GetCategoriesDoneCallback& callback) override;
  bool StartTracing(const base::trace_event::TraceConfig& trace_config,
                    const StartTracingDoneCallback& callback) override;
  bool StopTracing(const scoped_refptr<TraceDataSink>& sink) override;
  bool GetTraceBufferUsage(
      const GetTraceBufferUsageCallback& callback) override;
  bool SetWatchEvent(const std::string& category_name,
                     const std::string& event_name,
                     const WatchEventCallback& callback) override;
  bool CancelWatchEvent() override;
  bool IsTracing() const override;

  void RegisterTracingUI(TracingUI* tracing_ui);
  void UnregisterTracingUI(TracingUI* tracing_ui);

  // base::trace_event::TracingAgent implementation.
  std::string GetTracingAgentName() override;
  std::string GetTraceEventLabel() override;
  void StartAgentTracing(const base::trace_event::TraceConfig& trace_config,
                         const StartAgentTracingCallback& callback) override;
  void StopAgentTracing(const StopAgentTracingCallback& callback) override;
  bool SupportsExplicitClockSync() override;
  void RecordClockSyncMarker(
      const std::string& sync_id,
      const RecordClockSyncMarkerCallback& callback) override;

  // base::trace_event::MemoryDumpManagerDelegate implementation.
  void RequestGlobalMemoryDump(
      const base::trace_event::MemoryDumpRequestArgs& args,
      const base::trace_event::MemoryDumpCallback& callback) override;
  uint64_t GetTracingProcessId() const override;

  class TraceMessageFilterObserver {
   public:
    virtual void OnTraceMessageFilterAdded(TraceMessageFilter* filter) = 0;
    virtual void OnTraceMessageFilterRemoved(TraceMessageFilter* filter) = 0;
  };
  void AddTraceMessageFilterObserver(TraceMessageFilterObserver* observer);
  void RemoveTraceMessageFilterObserver(TraceMessageFilterObserver* observer);

 private:
  friend struct base::DefaultLazyInstanceTraits<TracingControllerImpl>;
  friend class TraceMessageFilter;

  // The arguments and callback for an queued global memory dump request.
  struct QueuedMemoryDumpRequest {
    QueuedMemoryDumpRequest(
        const base::trace_event::MemoryDumpRequestArgs& args,
        const base::trace_event::MemoryDumpCallback& callback);
    ~QueuedMemoryDumpRequest();
    const base::trace_event::MemoryDumpRequestArgs args;
    const base::trace_event::MemoryDumpCallback callback;
  };

  TracingControllerImpl();
  ~TracingControllerImpl() override;

  bool can_start_tracing() const {
    return !is_tracing_;
  }

  bool can_stop_tracing() const {
    return is_tracing_ && !trace_data_sink_.get();
  }

  bool can_start_monitoring() const {
    return !is_monitoring_;
  }

  bool can_stop_monitoring() const {
    return is_monitoring_ && !monitoring_data_sink_.get();
  }

  bool can_get_trace_buffer_usage() const {
    return pending_trace_buffer_usage_callback_.is_null();
  }

  bool can_cancel_watch_event() const {
    return !watch_event_callback_.is_null();
  }

  void PerformNextQueuedGlobalMemoryDump();

  // Methods for use by TraceMessageFilter.
  void AddTraceMessageFilter(TraceMessageFilter* trace_message_filter);
  void RemoveTraceMessageFilter(TraceMessageFilter* trace_message_filter);

  void OnTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& events_str_ptr);

  // Callback of TraceLog::Flush() for the local trace.
  void OnLocalTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& events_str_ptr,
      bool has_more_events);

  // Adds the tracing agent with the specified agent name to the list of
  // additional tracing agents.
  void AddTracingAgent(const std::string& agent_name);

  void OnStartAgentTracingAcked(const std::string& agent_name, bool success);

  void OnStopTracingAcked(
      TraceMessageFilter* trace_message_filter,
      const std::vector<std::string>& known_category_groups);

  void OnEndAgentTracingAcked(
      const std::string& agent_name,
      const std::string& events_label,
      const scoped_refptr<base::RefCountedString>& events_str_ptr);

  void OnTraceLogStatusReply(TraceMessageFilter* trace_message_filter,
                             const base::trace_event::TraceLogStatus& status);
  void OnProcessMemoryDumpResponse(TraceMessageFilter* trace_message_filter,
                                   uint64_t dump_guid,
                                   bool success);

  // Callback of MemoryDumpManager::CreateProcessDump().
  void OnBrowserProcessMemoryDumpDone(uint64_t dump_guid, bool success);

  void FinalizeGlobalMemoryDumpIfAllProcessesReplied();

  void OnWatchEventMatched();

  void SetEnabledOnFileThread(
      const base::trace_event::TraceConfig& trace_config,
      int mode,
      const base::Closure& callback);
  void SetDisabledOnFileThread(const base::Closure& callback);
  void OnAllTracingAgentsStarted();
  void StopTracingAfterClockSync();
  void OnStopTracingDone();

  // Issue clock sync markers to the tracing agents.
  void IssueClockSyncMarker();
  void OnClockSyncMarkerRecordedByAgent(
      const std::string& sync_id,
      const base::TimeTicks& issue_ts,
      const base::TimeTicks& issue_end_ts);

  typedef std::set<scoped_refptr<TraceMessageFilter>> TraceMessageFilterSet;
  TraceMessageFilterSet trace_message_filters_;

  // Pending acks for StartTracing.
  int pending_start_tracing_ack_count_;
  base::OneShotTimer start_tracing_timer_;
  StartTracingDoneCallback start_tracing_done_callback_;
  std::unique_ptr<base::trace_event::TraceConfig> start_tracing_trace_config_;

  // Pending acks for StopTracing.
  int pending_stop_tracing_ack_count_;
  TraceMessageFilterSet pending_stop_tracing_filters_;

  // Pending acks for GetTraceLogStatus.
  int pending_trace_log_status_ack_count_;
  TraceMessageFilterSet pending_trace_log_status_filters_;
  float maximum_trace_buffer_usage_;
  size_t approximate_event_count_;

  // Pending acks for memory RequestGlobalDumpPoint.
  int pending_memory_dump_ack_count_;
  int failed_memory_dump_count_;
  TraceMessageFilterSet pending_memory_dump_filters_;
  std::list<QueuedMemoryDumpRequest> queued_memory_dump_requests_;

  std::vector<base::trace_event::TracingAgent*> additional_tracing_agents_;
  int pending_clock_sync_ack_count_;
  base::OneShotTimer clock_sync_timer_;

  bool is_tracing_;
  bool is_monitoring_;

  GetCategoriesDoneCallback pending_get_categories_done_callback_;
  GetTraceBufferUsageCallback pending_trace_buffer_usage_callback_;

  std::string watch_category_name_;
  std::string watch_event_name_;
  WatchEventCallback watch_event_callback_;

  base::ObserverList<TraceMessageFilterObserver>
      trace_message_filter_observers_;

  std::set<std::string> known_category_groups_;
  std::set<TracingUI*> tracing_uis_;
  scoped_refptr<TraceDataSink> trace_data_sink_;
  scoped_refptr<TraceDataSink> monitoring_data_sink_;

  DISALLOW_COPY_AND_ASSIGN(TracingControllerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACING_CONTROLLER_IMPL_H_
