// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/trace_message_filter.h"

#include "components/tracing/tracing_messages.h"
#include "content/browser/tracing/tracing_controller_impl.h"

namespace content {

TraceMessageFilter::TraceMessageFilter()
    : BrowserMessageFilter(TracingMsgStart),
      has_child_(false),
      is_awaiting_end_ack_(false),
      is_awaiting_capture_monitoring_snapshot_ack_(false),
      is_awaiting_buffer_percent_full_ack_(false) {
}

TraceMessageFilter::~TraceMessageFilter() {}

void TraceMessageFilter::OnChannelClosing() {
  if (has_child_) {
    if (is_awaiting_end_ack_)
      OnEndTracingAck(std::vector<std::string>());

    if (is_awaiting_capture_monitoring_snapshot_ack_)
      OnCaptureMonitoringSnapshotAcked();

    if (is_awaiting_buffer_percent_full_ack_)
      OnTraceBufferPercentFullReply(0.0f);

    TracingControllerImpl::GetInstance()->RemoveTraceMessageFilter(this);
  }
}

bool TraceMessageFilter::OnMessageReceived(const IPC::Message& message) {
  // Always on IO thread (BrowserMessageFilter guarantee).
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TraceMessageFilter, message)
    IPC_MESSAGE_HANDLER(TracingHostMsg_ChildSupportsTracing,
                        OnChildSupportsTracing)
    IPC_MESSAGE_HANDLER(TracingHostMsg_EndTracingAck, OnEndTracingAck)
    IPC_MESSAGE_HANDLER(TracingHostMsg_CaptureMonitoringSnapshotAck,
                        OnCaptureMonitoringSnapshotAcked)
    IPC_MESSAGE_HANDLER(TracingHostMsg_TraceDataCollected,
                        OnTraceDataCollected)
    IPC_MESSAGE_HANDLER(TracingHostMsg_MonitoringTraceDataCollected,
                        OnMonitoringTraceDataCollected)
    IPC_MESSAGE_HANDLER(TracingHostMsg_WatchEventMatched,
                        OnWatchEventMatched)
    IPC_MESSAGE_HANDLER(TracingHostMsg_TraceBufferPercentFullReply,
                        OnTraceBufferPercentFullReply)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void TraceMessageFilter::SendBeginTracing(
    const std::string& category_filter_str,
    base::debug::TraceLog::Options options) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Send(new TracingMsg_BeginTracing(category_filter_str,
                                   base::TimeTicks::NowFromSystemTraceTime(),
                                   options));
}

void TraceMessageFilter::SendEndTracing() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!is_awaiting_end_ack_);
  is_awaiting_end_ack_ = true;
  Send(new TracingMsg_EndTracing);
}

void TraceMessageFilter::SendEnableMonitoring(
    const std::string& category_filter_str,
    base::debug::TraceLog::Options options) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Send(new TracingMsg_EnableMonitoring(category_filter_str,
      base::TimeTicks::NowFromSystemTraceTime(),
      options));
}

void TraceMessageFilter::SendDisableMonitoring() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Send(new TracingMsg_DisableMonitoring);
}

void TraceMessageFilter::SendCaptureMonitoringSnapshot() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!is_awaiting_capture_monitoring_snapshot_ack_);
  is_awaiting_capture_monitoring_snapshot_ack_ = true;
  Send(new TracingMsg_CaptureMonitoringSnapshot);
}

void TraceMessageFilter::SendGetTraceBufferPercentFull() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!is_awaiting_buffer_percent_full_ack_);
  is_awaiting_buffer_percent_full_ack_ = true;
  Send(new TracingMsg_GetTraceBufferPercentFull);
}

void TraceMessageFilter::SendSetWatchEvent(const std::string& category_name,
                                           const std::string& event_name) {
  Send(new TracingMsg_SetWatchEvent(category_name, event_name));
}

void TraceMessageFilter::SendCancelWatchEvent() {
  Send(new TracingMsg_CancelWatchEvent);
}

void TraceMessageFilter::OnChildSupportsTracing() {
  has_child_ = true;
  TracingControllerImpl::GetInstance()->AddTraceMessageFilter(this);
}

void TraceMessageFilter::OnEndTracingAck(
    const std::vector<std::string>& known_categories) {
  // is_awaiting_end_ack_ should always be true here, but check in case the
  // child process is compromised.
  if (is_awaiting_end_ack_) {
    is_awaiting_end_ack_ = false;
    TracingControllerImpl::GetInstance()->OnDisableRecordingAcked(
        this, known_categories);
  } else {
    NOTREACHED();
  }
}

void TraceMessageFilter::OnCaptureMonitoringSnapshotAcked() {
  // is_awaiting_capture_monitoring_snapshot_ack_ should always be true here,
  // but check in case the child process is compromised.
  if (is_awaiting_capture_monitoring_snapshot_ack_) {
    is_awaiting_capture_monitoring_snapshot_ack_ = false;
    TracingControllerImpl::GetInstance()->OnCaptureMonitoringSnapshotAcked(
        this);
  } else {
    NOTREACHED();
  }
}

void TraceMessageFilter::OnTraceDataCollected(const std::string& data) {
  scoped_refptr<base::RefCountedString> data_ptr(new base::RefCountedString());
  data_ptr->data() = data;
  TracingControllerImpl::GetInstance()->OnTraceDataCollected(data_ptr);
}

void TraceMessageFilter::OnMonitoringTraceDataCollected(
    const std::string& data) {
  scoped_refptr<base::RefCountedString> data_ptr(new base::RefCountedString());
  data_ptr->data() = data;
  TracingControllerImpl::GetInstance()->OnMonitoringTraceDataCollected(
      data_ptr);
}

void TraceMessageFilter::OnWatchEventMatched() {
  TracingControllerImpl::GetInstance()->OnWatchEventMatched();
}

void TraceMessageFilter::OnTraceBufferPercentFullReply(float percent_full) {
  if (is_awaiting_buffer_percent_full_ack_) {
    is_awaiting_buffer_percent_full_ack_ = false;
    TracingControllerImpl::GetInstance()->OnTraceBufferPercentFullReply(
        this, percent_full);
  } else {
    NOTREACHED();
  }
}

}  // namespace content
