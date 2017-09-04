// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/child/child_trace_message_filter.h"

#include <memory>

#include "base/memory/ref_counted_memory.h"
#include "base/metrics/statistics_recorder.h"
#include "base/trace_event/trace_event.h"
#include "components/tracing/child/child_memory_dump_manager_delegate_impl.h"
#include "components/tracing/common/tracing_messages.h"
#include "ipc/ipc_channel.h"

using base::trace_event::TraceLog;

namespace tracing {

namespace {

const int kMinTimeBetweenHistogramChangesInSeconds = 10;

}  // namespace

ChildTraceMessageFilter::ChildTraceMessageFilter(
    base::SingleThreadTaskRunner* ipc_task_runner)
    : sender_(NULL),
      ipc_task_runner_(ipc_task_runner),
      pending_memory_dump_guid_(0) {
}

void ChildTraceMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  sender_ = channel;
  sender_->Send(new TracingHostMsg_ChildSupportsTracing());
  ChildMemoryDumpManagerDelegateImpl::GetInstance()->SetChildTraceMessageFilter(
      this);
}

void ChildTraceMessageFilter::SetSenderForTesting(IPC::Sender* sender) {
  sender_ = sender;
}

void ChildTraceMessageFilter::OnFilterRemoved() {
  ChildMemoryDumpManagerDelegateImpl::GetInstance()->SetChildTraceMessageFilter(
      nullptr);
  sender_ = NULL;
}

bool ChildTraceMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChildTraceMessageFilter, message)
    IPC_MESSAGE_HANDLER(TracingMsg_BeginTracing, OnBeginTracing)
    IPC_MESSAGE_HANDLER(TracingMsg_EndTracing, OnEndTracing)
    IPC_MESSAGE_HANDLER(TracingMsg_CancelTracing, OnCancelTracing)
    IPC_MESSAGE_HANDLER(TracingMsg_GetTraceLogStatus, OnGetTraceLogStatus)
    IPC_MESSAGE_HANDLER(TracingMsg_ProcessMemoryDumpRequest,
                        OnProcessMemoryDumpRequest)
    IPC_MESSAGE_HANDLER(TracingMsg_GlobalMemoryDumpResponse,
                        OnGlobalMemoryDumpResponse)
    IPC_MESSAGE_HANDLER(TracingMsg_SetUMACallback, OnSetUMACallback)
    IPC_MESSAGE_HANDLER(TracingMsg_ClearUMACallback, OnClearUMACallback)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

ChildTraceMessageFilter::~ChildTraceMessageFilter() {}

void ChildTraceMessageFilter::OnBeginTracing(
    const std::string& trace_config_str,
    base::TimeTicks browser_time,
    uint64_t tracing_process_id) {
#if defined(__native_client__)
  // NaCl and system times are offset by a bit, so subtract some time from
  // the captured timestamps. The value might be off by a bit due to messaging
  // latency.
  base::TimeDelta time_offset = base::TimeTicks::Now() - browser_time;
  TraceLog::GetInstance()->SetTimeOffset(time_offset);
#endif
  ChildMemoryDumpManagerDelegateImpl::GetInstance()->set_tracing_process_id(
      tracing_process_id);
  TraceLog::GetInstance()->SetEnabled(
      base::trace_event::TraceConfig(trace_config_str),
      base::trace_event::TraceLog::RECORDING_MODE);
}

void ChildTraceMessageFilter::OnEndTracing() {
  TraceLog::GetInstance()->SetDisabled();

  // Flush will generate one or more callbacks to OnTraceDataCollected
  // synchronously or asynchronously. EndTracingAck will be sent in the last
  // OnTraceDataCollected. We are already on the IO thread, so the
  // OnTraceDataCollected calls will not be deferred.
  TraceLog::GetInstance()->Flush(
      base::Bind(&ChildTraceMessageFilter::OnTraceDataCollected, this));

  ChildMemoryDumpManagerDelegateImpl::GetInstance()->set_tracing_process_id(
      base::trace_event::MemoryDumpManager::kInvalidTracingProcessId);
}

void ChildTraceMessageFilter::OnCancelTracing() {
  TraceLog::GetInstance()->CancelTracing(
      base::Bind(&ChildTraceMessageFilter::OnTraceDataCollected, this));
}

void ChildTraceMessageFilter::OnGetTraceLogStatus() {
  sender_->Send(new TracingHostMsg_TraceLogStatusReply(
      TraceLog::GetInstance()->GetStatus()));
}

void ChildTraceMessageFilter::OnTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& events_str_ptr,
    bool has_more_events) {
  if (!ipc_task_runner_->BelongsToCurrentThread()) {
    ipc_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ChildTraceMessageFilter::OnTraceDataCollected,
                              this, events_str_ptr, has_more_events));
    return;
  }
  if (events_str_ptr->data().size()) {
    sender_->Send(new TracingHostMsg_TraceDataCollected(
        events_str_ptr->data()));
  }
  if (!has_more_events) {
    std::vector<std::string> category_groups;
    TraceLog::GetInstance()->GetKnownCategoryGroups(&category_groups);
    sender_->Send(new TracingHostMsg_EndTracingAck(category_groups));
  }
}

// Sent by the Browser's MemoryDumpManager when coordinating a global dump.
void ChildTraceMessageFilter::OnProcessMemoryDumpRequest(
    const base::trace_event::MemoryDumpRequestArgs& args) {
  ChildMemoryDumpManagerDelegateImpl::GetInstance()->CreateProcessDump(
      args,
      base::Bind(&ChildTraceMessageFilter::OnProcessMemoryDumpDone, this));
}

void ChildTraceMessageFilter::OnProcessMemoryDumpDone(uint64_t dump_guid,
                                                      bool success) {
  sender_->Send(
      new TracingHostMsg_ProcessMemoryDumpResponse(dump_guid, success));
}

// Initiates a dump request, asking the Browser's MemoryDumpManager to
// coordinate a global memory dump. The Browser's MDM will answer back with a
// MemoryDumpResponse when all the child processes (including this one) have
// dumped, or with a NACK (|success| == false) if the dump failed (e.g., due to
// a collision with a concurrent request from another child process).
void ChildTraceMessageFilter::SendGlobalMemoryDumpRequest(
    const base::trace_event::MemoryDumpRequestArgs& args,
    const base::trace_event::MemoryDumpCallback& callback) {
  // If there is already another dump request pending from this child process,
  // there is no point bothering the Browser's MemoryDumpManager.
  if (pending_memory_dump_guid_) {
    if (!callback.is_null())
      callback.Run(args.dump_guid, false);
    return;
  }

  pending_memory_dump_guid_ = args.dump_guid;
  pending_memory_dump_callback_ = callback;
  sender_->Send(new TracingHostMsg_GlobalMemoryDumpRequest(args));
}

// Sent by the Browser's MemoryDumpManager in response of a dump request
// initiated by this child process.
void ChildTraceMessageFilter::OnGlobalMemoryDumpResponse(uint64_t dump_guid,
                                                         bool success) {
  DCHECK_NE(0U, pending_memory_dump_guid_);
  pending_memory_dump_guid_ = 0;
  if (pending_memory_dump_callback_.is_null())
    return;
  pending_memory_dump_callback_.Run(dump_guid, success);
}

void ChildTraceMessageFilter::OnHistogramChanged(
    const std::string& histogram_name,
    base::Histogram::Sample reference_lower_value,
    base::Histogram::Sample reference_upper_value,
    bool repeat,
    base::Histogram::Sample actual_value) {
  if (actual_value < reference_lower_value ||
      actual_value > reference_upper_value) {
    if (!repeat) {
      ipc_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(
              &ChildTraceMessageFilter::SendAbortBackgroundTracingMessage,
              this));
    }
  }

  ipc_task_runner_->PostTask(
      FROM_HERE, base::Bind(&ChildTraceMessageFilter::SendTriggerMessage, this,
                            histogram_name));
}

void ChildTraceMessageFilter::SendTriggerMessage(
    const std::string& histogram_name) {
  if (!histogram_last_changed_.is_null()) {
    base::Time computed_next_allowed_time =
        histogram_last_changed_ +
        base::TimeDelta::FromSeconds(kMinTimeBetweenHistogramChangesInSeconds);
    if (computed_next_allowed_time > base::Time::Now())
      return;
  }
  histogram_last_changed_ = base::Time::Now();

  if (sender_)
    sender_->Send(new TracingHostMsg_TriggerBackgroundTrace(histogram_name));
}

void ChildTraceMessageFilter::SendAbortBackgroundTracingMessage() {
  if (sender_)
    sender_->Send(new TracingHostMsg_AbortBackgroundTrace());
}

void ChildTraceMessageFilter::OnSetUMACallback(
    const std::string& histogram_name,
    int histogram_lower_value,
    int histogram_upper_value,
    bool repeat) {
  histogram_last_changed_ = base::Time();
  base::StatisticsRecorder::SetCallback(
      histogram_name, base::Bind(&ChildTraceMessageFilter::OnHistogramChanged,
                                 this, histogram_name, histogram_lower_value,
                                 histogram_upper_value, repeat));

  base::HistogramBase* existing_histogram =
      base::StatisticsRecorder::FindHistogram(histogram_name);
  if (!existing_histogram)
    return;

  std::unique_ptr<base::HistogramSamples> samples =
      existing_histogram->SnapshotSamples();
  if (!samples)
    return;

  std::unique_ptr<base::SampleCountIterator> sample_iterator =
      samples->Iterator();
  if (!sample_iterator)
    return;

  while (!sample_iterator->Done()) {
    base::HistogramBase::Sample min;
    base::HistogramBase::Sample max;
    base::HistogramBase::Count count;
    sample_iterator->Get(&min, &max, &count);

    if (min >= histogram_lower_value && max <= histogram_upper_value) {
      ipc_task_runner_->PostTask(
          FROM_HERE, base::Bind(&ChildTraceMessageFilter::SendTriggerMessage,
                                this, histogram_name));
      break;
    } else if (!repeat) {
      ipc_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(
              &ChildTraceMessageFilter::SendAbortBackgroundTracingMessage,
              this));
      break;
    }

    sample_iterator->Next();
  }
}

void ChildTraceMessageFilter::OnClearUMACallback(
    const std::string& histogram_name) {
  histogram_last_changed_ = base::Time();
  base::StatisticsRecorder::ClearCallback(histogram_name);
}

}  // namespace tracing
