// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACE_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_TRACING_TRACE_MESSAGE_FILTER_H_

#include <string>
#include <vector>

#include "base/debug/trace_event.h"
#include "content/public/browser/browser_message_filter.h"

namespace content {

// This class sends and receives trace messages on the browser process.
// See also: tracing_controller.h
// See also: child_trace_message_filter.h
class TraceMessageFilter : public BrowserMessageFilter {
 public:
  TraceMessageFilter();

  // BrowserMessageFilter implementation.
  virtual void OnChannelClosing() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void SendBeginTracing(const std::string& category_filter_str,
                        base::debug::TraceLog::Options options);
  void SendEndTracing();
  void SendEnableMonitoring(const std::string& category_filter_str,
                            base::debug::TraceLog::Options options);
  void SendDisableMonitoring();
  void SendCaptureMonitoringSnapshot();
  void SendGetTraceBufferPercentFull();
  void SendSetWatchEvent(const std::string& category_name,
                         const std::string& event_name);
  void SendCancelWatchEvent();

 protected:
  virtual ~TraceMessageFilter();

 private:
  // Message handlers.
  void OnChildSupportsTracing();
  void OnEndTracingAck(const std::vector<std::string>& known_categories);
  void OnCaptureMonitoringSnapshotAcked();
  void OnWatchEventMatched();
  void OnTraceBufferPercentFullReply(float percent_full);
  void OnTraceDataCollected(const std::string& data);
  void OnMonitoringTraceDataCollected(const std::string& data);

  // ChildTraceMessageFilter exists:
  bool has_child_;

  // Awaiting ack for previously sent SendEndTracing
  bool is_awaiting_end_ack_;
  // Awaiting ack for previously sent SendCaptureMonitoringSnapshot
  bool is_awaiting_capture_monitoring_snapshot_ack_;
  // Awaiting ack for previously sent SendGetTraceBufferPercentFull
  bool is_awaiting_buffer_percent_full_ack_;

  DISALLOW_COPY_AND_ASSIGN(TraceMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACE_MESSAGE_FILTER_H_
