// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACING_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_TRACING_TRACING_CONTROLLER_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "content/public/browser/tracing_controller.h"

namespace base {
class RefCountedString;
}

namespace content {

class TraceMessageFilter;
class TracingUI;

class TracingControllerImpl : public TracingController {
 public:
  static TracingControllerImpl* GetInstance();

  // TracingController implementation.
  virtual bool GetCategories(
      const GetCategoriesDoneCallback& callback) OVERRIDE;
  virtual bool EnableRecording(
      const std::string& category_filter,
      TracingController::Options options,
      const EnableRecordingDoneCallback& callback) OVERRIDE;
  virtual bool DisableRecording(
      const base::FilePath& result_file_path,
      const TracingFileResultCallback& callback) OVERRIDE;
  virtual bool EnableMonitoring(const std::string& category_filter,
      TracingController::Options options,
      const EnableMonitoringDoneCallback& callback) OVERRIDE;
  virtual bool DisableMonitoring(
      const DisableMonitoringDoneCallback& callback) OVERRIDE;
  virtual void GetMonitoringStatus(
      bool* out_enabled,
      std::string* out_category_filter,
      TracingController::Options* out_options) OVERRIDE;
  virtual bool CaptureMonitoringSnapshot(
      const base::FilePath& result_file_path,
      const TracingFileResultCallback& callback) OVERRIDE;
  virtual bool GetTraceBufferPercentFull(
      const GetTraceBufferPercentFullCallback& callback) OVERRIDE;
  virtual bool SetWatchEvent(const std::string& category_name,
                             const std::string& event_name,
                             const WatchEventCallback& callback) OVERRIDE;
  virtual bool CancelWatchEvent() OVERRIDE;

  void RegisterTracingUI(TracingUI* tracing_ui);
  void UnregisterTracingUI(TracingUI* tracing_ui);

 private:
  typedef std::set<scoped_refptr<TraceMessageFilter> > TraceMessageFilterSet;
  class ResultFile;

  friend struct base::DefaultLazyInstanceTraits<TracingControllerImpl>;
  friend class TraceMessageFilter;

  TracingControllerImpl();
  virtual ~TracingControllerImpl();

  bool can_enable_recording() const {
    return !is_recording_;
  }

  bool can_disable_recording() const {
    return is_recording_ && !result_file_;
  }

  bool can_enable_monitoring() const {
    return !is_monitoring_;
  }

  bool can_disable_monitoring() const {
    return is_monitoring_ && !monitoring_snapshot_file_;
  }

  bool can_get_trace_buffer_percent_full() const {
    return pending_trace_buffer_percent_full_callback_.is_null();
  }

  bool can_cancel_watch_event() const {
    return !watch_event_callback_.is_null();
  }

  // Methods for use by TraceMessageFilter.
  void AddTraceMessageFilter(TraceMessageFilter* trace_message_filter);
  void RemoveTraceMessageFilter(TraceMessageFilter* trace_message_filter);

  void OnTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& events_str_ptr);
  void OnMonitoringTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& events_str_ptr);

  // Callback of TraceLog::Flush() for the local trace.
  void OnLocalTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& events_str_ptr,
      bool has_more_events);
  // Callback of TraceLog::FlushMonitoring() for the local trace.
  void OnLocalMonitoringTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& events_str_ptr,
      bool has_more_events);

  void OnDisableRecordingAcked(
      TraceMessageFilter* trace_message_filter,
      const std::vector<std::string>& known_category_groups);
  void OnDisableRecordingComplete();
  void OnResultFileClosed();

#if defined(OS_CHROMEOS) || defined(OS_WIN)
  void OnEndSystemTracingAcked(
      const scoped_refptr<base::RefCountedString>& events_str_ptr);
#endif

  void OnCaptureMonitoringSnapshotAcked(
      TraceMessageFilter* trace_message_filter);
  void OnMonitoringSnapshotFileClosed();

  void OnTraceBufferPercentFullReply(
      TraceMessageFilter* trace_message_filter,
      float percent_full);

  void OnWatchEventMatched();

  void SetEnabledOnFileThread(const std::string& category_filter,
                              int mode,
                              int options,
                              const base::Closure& callback);
  void SetDisabledOnFileThread(const base::Closure& callback);
  void OnEnableRecordingDone(const std::string& category_filter,
                             int trace_options,
                             const EnableRecordingDoneCallback& callback);
  void OnDisableRecordingDone(const base::FilePath& result_file_path,
                              const TracingFileResultCallback& callback);
  void OnEnableMonitoringDone(const std::string& category_filter,
                              int trace_options,
                              const EnableMonitoringDoneCallback& callback);
  void OnDisableMonitoringDone(const DisableMonitoringDoneCallback& callback);

  void OnMonitoringStateChanged(bool is_monitoring);

  TraceMessageFilterSet trace_message_filters_;

  // Pending acks for DisableRecording.
  int pending_disable_recording_ack_count_;
  TraceMessageFilterSet pending_disable_recording_filters_;
  // Pending acks for CaptureMonitoringSnapshot.
  int pending_capture_monitoring_snapshot_ack_count_;
  TraceMessageFilterSet pending_capture_monitoring_filters_;
  // Pending acks for GetTraceBufferPercentFull.
  int pending_trace_buffer_percent_full_ack_count_;
  TraceMessageFilterSet pending_trace_buffer_percent_full_filters_;
  float maximum_trace_buffer_percent_full_;

#if defined(OS_CHROMEOS) || defined(OS_WIN)
  bool is_system_tracing_;
#endif
  bool is_recording_;
  bool is_monitoring_;
  TracingController::Options options_;

  GetCategoriesDoneCallback pending_get_categories_done_callback_;
  TracingFileResultCallback pending_disable_recording_done_callback_;
  TracingFileResultCallback pending_capture_monitoring_snapshot_done_callback_;
  GetTraceBufferPercentFullCallback pending_trace_buffer_percent_full_callback_;

  std::string watch_category_name_;
  std::string watch_event_name_;
  WatchEventCallback watch_event_callback_;

  std::set<std::string> known_category_groups_;
  std::set<TracingUI*> tracing_uis_;
  scoped_ptr<ResultFile> result_file_;
  scoped_ptr<ResultFile> monitoring_snapshot_file_;
  DISALLOW_COPY_AND_ASSIGN(TracingControllerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACING_CONTROLLER_IMPL_H_
