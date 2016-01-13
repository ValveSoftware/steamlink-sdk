// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/tracing/tracing_controller_impl.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/json/string_escape.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/tracing/trace_message_filter.h"
#include "content/browser/tracing/tracing_ui.h"
#include "content/common/child_process_messages.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/common/content_switches.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#endif

#if defined(OS_WIN)
#include "content/browser/tracing/etw_system_event_consumer_win.h"
#endif

using base::debug::TraceLog;

namespace content {

namespace {

base::LazyInstance<TracingControllerImpl>::Leaky g_controller =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

TracingController* TracingController::GetInstance() {
  return TracingControllerImpl::GetInstance();
}

class TracingControllerImpl::ResultFile {
 public:
  explicit ResultFile(const base::FilePath& path);
  void Write(const scoped_refptr<base::RefCountedString>& events_str_ptr) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&TracingControllerImpl::ResultFile::WriteTask,
                   base::Unretained(this), events_str_ptr));
  }
  void Close(const base::Closure& callback) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&TracingControllerImpl::ResultFile::CloseTask,
                   base::Unretained(this), callback));
  }
  void WriteSystemTrace(
      const scoped_refptr<base::RefCountedString>& events_str_ptr) {
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&TracingControllerImpl::ResultFile::WriteSystemTraceTask,
                   base::Unretained(this), events_str_ptr));
  }

  const base::FilePath& path() const { return path_; }

 private:
  void OpenTask();
  void WriteTask(const scoped_refptr<base::RefCountedString>& events_str_ptr);
  void WriteSystemTraceTask(
      const scoped_refptr<base::RefCountedString>& events_str_ptr);
  void CloseTask(const base::Closure& callback);

  FILE* file_;
  base::FilePath path_;
  bool has_at_least_one_result_;
  scoped_refptr<base::RefCountedString> system_trace_;

  DISALLOW_COPY_AND_ASSIGN(ResultFile);
};

TracingControllerImpl::ResultFile::ResultFile(const base::FilePath& path)
    : file_(NULL),
      path_(path),
      has_at_least_one_result_(false) {
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&TracingControllerImpl::ResultFile::OpenTask,
                 base::Unretained(this)));
}

void TracingControllerImpl::ResultFile::OpenTask() {
  if (path_.empty())
    base::CreateTemporaryFile(&path_);
  file_ = base::OpenFile(path_, "w");
  if (!file_) {
    LOG(ERROR) << "Failed to open " << path_.value();
    return;
  }
  const char* preamble = "{\"traceEvents\": [";
  size_t written = fwrite(preamble, strlen(preamble), 1, file_);
  DCHECK(written == 1);
}

void TracingControllerImpl::ResultFile::WriteTask(
    const scoped_refptr<base::RefCountedString>& events_str_ptr) {
  if (!file_ || !events_str_ptr->data().size())
    return;

  // If there is already a result in the file, then put a comma
  // before the next batch of results.
  if (has_at_least_one_result_) {
    size_t written = fwrite(",", 1, 1, file_);
    DCHECK(written == 1);
  }
  has_at_least_one_result_ = true;
  size_t written = fwrite(events_str_ptr->data().c_str(),
                          events_str_ptr->data().size(), 1,
                          file_);
  DCHECK(written == 1);
}

void TracingControllerImpl::ResultFile::WriteSystemTraceTask(
    const scoped_refptr<base::RefCountedString>& events_str_ptr) {
  system_trace_ = events_str_ptr;
}

void TracingControllerImpl::ResultFile::CloseTask(
    const base::Closure& callback) {
  if (!file_)
    return;

  const char* trailevents = "]";
  size_t written = fwrite(trailevents, strlen(trailevents), 1, file_);
  DCHECK(written == 1);

  if (system_trace_) {
#if defined(OS_WIN)
    // The Windows kernel events are kept into a JSon format stored as string
    // and must not be escaped.
    std::string json_string = system_trace_->data();
#else
    std::string json_string = base::GetQuotedJSONString(system_trace_->data());
#endif

    const char* systemTraceHead = ",\n\"systemTraceEvents\": ";
    written = fwrite(systemTraceHead, strlen(systemTraceHead), 1, file_);
    DCHECK(written == 1);

    written = fwrite(json_string.data(), json_string.size(), 1, file_);
    DCHECK(written == 1);

    system_trace_ = NULL;
  }

  const char* trailout = "}";
  written = fwrite(trailout, strlen(trailout), 1, file_);
  DCHECK(written == 1);
  base::CloseFile(file_);
  file_ = NULL;

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback);
}


TracingControllerImpl::TracingControllerImpl() :
    pending_disable_recording_ack_count_(0),
    pending_capture_monitoring_snapshot_ack_count_(0),
    pending_trace_buffer_percent_full_ack_count_(0),
    maximum_trace_buffer_percent_full_(0),
    // Tracing may have been enabled by ContentMainRunner if kTraceStartup
    // is specified in command line.
#if defined(OS_CHROMEOS) || defined(OS_WIN)
    is_system_tracing_(false),
#endif
    is_recording_(TraceLog::GetInstance()->IsEnabled()),
    is_monitoring_(false) {
}

TracingControllerImpl::~TracingControllerImpl() {
  // This is a Leaky instance.
  NOTREACHED();
}

TracingControllerImpl* TracingControllerImpl::GetInstance() {
  return g_controller.Pointer();
}

bool TracingControllerImpl::GetCategories(
    const GetCategoriesDoneCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Known categories come back from child processes with the EndTracingAck
  // message. So to get known categories, just begin and end tracing immediately
  // afterwards. This will ping all the child processes for categories.
  pending_get_categories_done_callback_ = callback;
  if (!EnableRecording("*", TracingController::Options(),
                       EnableRecordingDoneCallback())) {
    pending_get_categories_done_callback_.Reset();
    return false;
  }

  bool ok = DisableRecording(base::FilePath(), TracingFileResultCallback());
  DCHECK(ok);
  return true;
}

void TracingControllerImpl::SetEnabledOnFileThread(
    const std::string& category_filter,
    int mode,
    int trace_options,
    const base::Closure& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  TraceLog::GetInstance()->SetEnabled(
      base::debug::CategoryFilter(category_filter),
      static_cast<TraceLog::Mode>(mode),
      static_cast<TraceLog::Options>(trace_options));
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback);
}

void TracingControllerImpl::SetDisabledOnFileThread(
    const base::Closure& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  TraceLog::GetInstance()->SetDisabled();
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback);
}

bool TracingControllerImpl::EnableRecording(
    const std::string& category_filter,
    TracingController::Options options,
    const EnableRecordingDoneCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_enable_recording())
    return false;
  is_recording_ = true;

#if defined(OS_ANDROID)
  if (pending_get_categories_done_callback_.is_null())
    TraceLog::GetInstance()->AddClockSyncMetadataEvent();
#endif

  options_ = options;
  int trace_options = (options & RECORD_CONTINUOUSLY) ?
      TraceLog::RECORD_CONTINUOUSLY : TraceLog::RECORD_UNTIL_FULL;
  if (options & ENABLE_SAMPLING) {
    trace_options |= TraceLog::ENABLE_SAMPLING;
  }

  if (options & ENABLE_SYSTRACE) {
#if defined(OS_CHROMEOS)
    DCHECK(!is_system_tracing_);
    chromeos::DBusThreadManager::Get()->GetDebugDaemonClient()->
      StartSystemTracing();
    is_system_tracing_ = true;
#elif defined(OS_WIN)
    DCHECK(!is_system_tracing_);
    is_system_tracing_ =
        EtwSystemEventConsumer::GetInstance()->StartSystemTracing();
#endif
  }


  base::Closure on_enable_recording_done_callback =
      base::Bind(&TracingControllerImpl::OnEnableRecordingDone,
                 base::Unretained(this),
                 category_filter, trace_options, callback);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&TracingControllerImpl::SetEnabledOnFileThread,
                 base::Unretained(this),
                 category_filter,
                 base::debug::TraceLog::RECORDING_MODE,
                 trace_options,
                 on_enable_recording_done_callback));
  return true;
}

void TracingControllerImpl::OnEnableRecordingDone(
    const std::string& category_filter,
    int trace_options,
    const EnableRecordingDoneCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Notify all child processes.
  for (TraceMessageFilterSet::iterator it = trace_message_filters_.begin();
      it != trace_message_filters_.end(); ++it) {
    it->get()->SendBeginTracing(category_filter,
                                static_cast<TraceLog::Options>(trace_options));
  }

  if (!callback.is_null())
    callback.Run();
}

bool TracingControllerImpl::DisableRecording(
    const base::FilePath& result_file_path,
    const TracingFileResultCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_disable_recording())
    return false;

  options_ = TracingController::Options();
  // Disable local trace early to avoid traces during end-tracing process from
  // interfering with the process.
  base::Closure on_disable_recording_done_callback =
      base::Bind(&TracingControllerImpl::OnDisableRecordingDone,
                 base::Unretained(this),
                 result_file_path, callback);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&TracingControllerImpl::SetDisabledOnFileThread,
                 base::Unretained(this),
                 on_disable_recording_done_callback));
  return true;
}

void TracingControllerImpl::OnDisableRecordingDone(
    const base::FilePath& result_file_path,
    const TracingFileResultCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  pending_disable_recording_done_callback_ = callback;

#if defined(OS_ANDROID)
  if (pending_get_categories_done_callback_.is_null())
    TraceLog::GetInstance()->AddClockSyncMetadataEvent();
#endif

  if (!callback.is_null() || !result_file_path.empty())
    result_file_.reset(new ResultFile(result_file_path));

  // Count myself (local trace) in pending_disable_recording_ack_count_,
  // acked below.
  pending_disable_recording_ack_count_ = trace_message_filters_.size() + 1;
  pending_disable_recording_filters_ = trace_message_filters_;

#if defined(OS_CHROMEOS) || defined(OS_WIN)
  if (is_system_tracing_) {
    // Disable system tracing.
    is_system_tracing_ = false;
    ++pending_disable_recording_ack_count_;

#if defined(OS_CHROMEOS)
    chromeos::DBusThreadManager::Get()->GetDebugDaemonClient()->
      RequestStopSystemTracing(
          base::Bind(&TracingControllerImpl::OnEndSystemTracingAcked,
                     base::Unretained(this)));
#elif defined(OS_WIN)
    EtwSystemEventConsumer::GetInstance()->StopSystemTracing(
        base::Bind(&TracingControllerImpl::OnEndSystemTracingAcked,
                   base::Unretained(this)));
#endif
  }
#endif  // defined(OS_CHROMEOS) || defined(OS_WIN)

  // Handle special case of zero child processes by immediately flushing the
  // trace log. Once the flush has completed the caller will be notified that
  // tracing has ended.
  if (pending_disable_recording_ack_count_ == 1) {
    // Flush asynchronously now, because we don't have any children to wait for.
    TraceLog::GetInstance()->Flush(
        base::Bind(&TracingControllerImpl::OnLocalTraceDataCollected,
                   base::Unretained(this)));
  }

  // Notify all child processes.
  for (TraceMessageFilterSet::iterator it = trace_message_filters_.begin();
      it != trace_message_filters_.end(); ++it) {
    it->get()->SendEndTracing();
  }
}

bool TracingControllerImpl::EnableMonitoring(
    const std::string& category_filter,
    TracingController::Options options,
    const EnableMonitoringDoneCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_enable_monitoring())
    return false;
  OnMonitoringStateChanged(true);

#if defined(OS_ANDROID)
  TraceLog::GetInstance()->AddClockSyncMetadataEvent();
#endif

  options_ = options;
  int trace_options = 0;
  if (options & ENABLE_SAMPLING)
    trace_options |= TraceLog::ENABLE_SAMPLING;

  base::Closure on_enable_monitoring_done_callback =
      base::Bind(&TracingControllerImpl::OnEnableMonitoringDone,
                 base::Unretained(this),
                 category_filter, trace_options, callback);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&TracingControllerImpl::SetEnabledOnFileThread,
                 base::Unretained(this),
                 category_filter,
                 base::debug::TraceLog::MONITORING_MODE,
                 trace_options,
                 on_enable_monitoring_done_callback));
  return true;
}

void TracingControllerImpl::OnEnableMonitoringDone(
    const std::string& category_filter,
    int trace_options,
    const EnableMonitoringDoneCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Notify all child processes.
  for (TraceMessageFilterSet::iterator it = trace_message_filters_.begin();
      it != trace_message_filters_.end(); ++it) {
    it->get()->SendEnableMonitoring(category_filter,
        static_cast<TraceLog::Options>(trace_options));
  }

  if (!callback.is_null())
    callback.Run();
}

bool TracingControllerImpl::DisableMonitoring(
    const DisableMonitoringDoneCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_disable_monitoring())
    return false;

  options_ = TracingController::Options();
  base::Closure on_disable_monitoring_done_callback =
      base::Bind(&TracingControllerImpl::OnDisableMonitoringDone,
                 base::Unretained(this), callback);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&TracingControllerImpl::SetDisabledOnFileThread,
                 base::Unretained(this),
                 on_disable_monitoring_done_callback));
  return true;
}

void TracingControllerImpl::OnDisableMonitoringDone(
    const DisableMonitoringDoneCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  OnMonitoringStateChanged(false);

  // Notify all child processes.
  for (TraceMessageFilterSet::iterator it = trace_message_filters_.begin();
      it != trace_message_filters_.end(); ++it) {
    it->get()->SendDisableMonitoring();
  }

  if (!callback.is_null())
    callback.Run();
}

void TracingControllerImpl::GetMonitoringStatus(
    bool* out_enabled,
    std::string* out_category_filter,
    TracingController::Options* out_options) {
  *out_enabled = is_monitoring_;
  *out_category_filter =
      TraceLog::GetInstance()->GetCurrentCategoryFilter().ToString();
  *out_options = options_;
}

bool TracingControllerImpl::CaptureMonitoringSnapshot(
    const base::FilePath& result_file_path,
    const TracingFileResultCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_disable_monitoring())
    return false;

  if (callback.is_null() && result_file_path.empty())
    return false;

  pending_capture_monitoring_snapshot_done_callback_ = callback;
  monitoring_snapshot_file_.reset(new ResultFile(result_file_path));

  // Count myself in pending_capture_monitoring_snapshot_ack_count_,
  // acked below.
  pending_capture_monitoring_snapshot_ack_count_ =
      trace_message_filters_.size() + 1;
  pending_capture_monitoring_filters_ = trace_message_filters_;

  // Handle special case of zero child processes by immediately flushing the
  // trace log. Once the flush has completed the caller will be notified that
  // the capture snapshot has ended.
  if (pending_capture_monitoring_snapshot_ack_count_ == 1) {
    // Flush asynchronously now, because we don't have any children to wait for.
    TraceLog::GetInstance()->FlushButLeaveBufferIntact(
        base::Bind(&TracingControllerImpl::OnLocalMonitoringTraceDataCollected,
                   base::Unretained(this)));
  }

  // Notify all child processes.
  for (TraceMessageFilterSet::iterator it = trace_message_filters_.begin();
      it != trace_message_filters_.end(); ++it) {
    it->get()->SendCaptureMonitoringSnapshot();
  }

#if defined(OS_ANDROID)
  TraceLog::GetInstance()->AddClockSyncMetadataEvent();
#endif

  return true;
}

bool TracingControllerImpl::GetTraceBufferPercentFull(
    const GetTraceBufferPercentFullCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_get_trace_buffer_percent_full() || callback.is_null())
    return false;

  pending_trace_buffer_percent_full_callback_ = callback;

  // Count myself in pending_trace_buffer_percent_full_ack_count_, acked below.
  pending_trace_buffer_percent_full_ack_count_ =
      trace_message_filters_.size() + 1;
  pending_trace_buffer_percent_full_filters_ = trace_message_filters_;
  maximum_trace_buffer_percent_full_ = 0;

  // Call OnTraceBufferPercentFullReply unconditionally for the browser process.
  // This will result in immediate execution of the callback if there are no
  // child processes.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&TracingControllerImpl::OnTraceBufferPercentFullReply,
                  base::Unretained(this),
                  scoped_refptr<TraceMessageFilter>(),
                  TraceLog::GetInstance()->GetBufferPercentFull()));

  // Notify all child processes.
  for (TraceMessageFilterSet::iterator it = trace_message_filters_.begin();
      it != trace_message_filters_.end(); ++it) {
    it->get()->SendGetTraceBufferPercentFull();
  }
  return true;
}

bool TracingControllerImpl::SetWatchEvent(
    const std::string& category_name,
    const std::string& event_name,
    const WatchEventCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (callback.is_null())
    return false;

  watch_category_name_ = category_name;
  watch_event_name_ = event_name;
  watch_event_callback_ = callback;

  TraceLog::GetInstance()->SetWatchEvent(
      category_name, event_name,
      base::Bind(&TracingControllerImpl::OnWatchEventMatched,
                 base::Unretained(this)));

  for (TraceMessageFilterSet::iterator it = trace_message_filters_.begin();
      it != trace_message_filters_.end(); ++it) {
    it->get()->SendSetWatchEvent(category_name, event_name);
  }
  return true;
}

bool TracingControllerImpl::CancelWatchEvent() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_cancel_watch_event())
    return false;

  for (TraceMessageFilterSet::iterator it = trace_message_filters_.begin();
      it != trace_message_filters_.end(); ++it) {
    it->get()->SendCancelWatchEvent();
  }

  watch_event_callback_.Reset();
  return true;
}

void TracingControllerImpl::AddTraceMessageFilter(
    TraceMessageFilter* trace_message_filter) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::AddTraceMessageFilter,
                   base::Unretained(this),
                   make_scoped_refptr(trace_message_filter)));
    return;
  }

  trace_message_filters_.insert(trace_message_filter);
  if (can_cancel_watch_event()) {
    trace_message_filter->SendSetWatchEvent(watch_category_name_,
                                            watch_event_name_);
  }
  if (can_disable_recording()) {
    trace_message_filter->SendBeginTracing(
        TraceLog::GetInstance()->GetCurrentCategoryFilter().ToString(),
        TraceLog::GetInstance()->trace_options());
  }
  if (can_disable_monitoring()) {
    trace_message_filter->SendEnableMonitoring(
        TraceLog::GetInstance()->GetCurrentCategoryFilter().ToString(),
        TraceLog::GetInstance()->trace_options());
  }
}

void TracingControllerImpl::RemoveTraceMessageFilter(
    TraceMessageFilter* trace_message_filter) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::RemoveTraceMessageFilter,
                   base::Unretained(this),
                   make_scoped_refptr(trace_message_filter)));
    return;
  }

  // If a filter is removed while a response from that filter is pending then
  // simulate the response. Otherwise the response count will be wrong and the
  // completion callback will never be executed.
  if (pending_disable_recording_ack_count_ > 0) {
    TraceMessageFilterSet::const_iterator it =
        pending_disable_recording_filters_.find(trace_message_filter);
    if (it != pending_disable_recording_filters_.end()) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
          base::Bind(&TracingControllerImpl::OnDisableRecordingAcked,
                     base::Unretained(this),
                     make_scoped_refptr(trace_message_filter),
                     std::vector<std::string>()));
    }
  }
  if (pending_capture_monitoring_snapshot_ack_count_ > 0) {
    TraceMessageFilterSet::const_iterator it =
        pending_capture_monitoring_filters_.find(trace_message_filter);
    if (it != pending_capture_monitoring_filters_.end()) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
          base::Bind(&TracingControllerImpl::OnCaptureMonitoringSnapshotAcked,
                     base::Unretained(this),
                     make_scoped_refptr(trace_message_filter)));
    }
  }
  if (pending_trace_buffer_percent_full_ack_count_ > 0) {
    TraceMessageFilterSet::const_iterator it =
        pending_trace_buffer_percent_full_filters_.find(trace_message_filter);
    if (it != pending_trace_buffer_percent_full_filters_.end()) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
          base::Bind(&TracingControllerImpl::OnTraceBufferPercentFullReply,
                     base::Unretained(this),
                     make_scoped_refptr(trace_message_filter),
                     0));
    }
  }

  trace_message_filters_.erase(trace_message_filter);
}

void TracingControllerImpl::OnDisableRecordingAcked(
    TraceMessageFilter* trace_message_filter,
    const std::vector<std::string>& known_category_groups) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::OnDisableRecordingAcked,
                   base::Unretained(this),
                   make_scoped_refptr(trace_message_filter),
                   known_category_groups));
    return;
  }

  // Merge known_category_groups with known_category_groups_
  known_category_groups_.insert(known_category_groups.begin(),
                                known_category_groups.end());

  if (pending_disable_recording_ack_count_ == 0)
    return;

  if (trace_message_filter &&
      !pending_disable_recording_filters_.erase(trace_message_filter)) {
    // The response from the specified message filter has already been received.
    return;
  }

  if (--pending_disable_recording_ack_count_ == 1) {
    // All acks from subprocesses have been received. Now flush the local trace.
    // During or after this call, our OnLocalTraceDataCollected will be
    // called with the last of the local trace data.
    TraceLog::GetInstance()->Flush(
        base::Bind(&TracingControllerImpl::OnLocalTraceDataCollected,
                   base::Unretained(this)));
    return;
  }

  if (pending_disable_recording_ack_count_ != 0)
    return;

  OnDisableRecordingComplete();
}

void TracingControllerImpl::OnDisableRecordingComplete() {
  // All acks (including from the subprocesses and the local trace) have been
  // received.
  is_recording_ = false;

  // Trigger callback if one is set.
  if (!pending_get_categories_done_callback_.is_null()) {
    pending_get_categories_done_callback_.Run(known_category_groups_);
    pending_get_categories_done_callback_.Reset();
  } else if (result_file_) {
    result_file_->Close(
        base::Bind(&TracingControllerImpl::OnResultFileClosed,
                   base::Unretained(this)));
  }
}

void TracingControllerImpl::OnResultFileClosed() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!result_file_)
    return;

  if (!pending_disable_recording_done_callback_.is_null()) {
    pending_disable_recording_done_callback_.Run(result_file_->path());
    pending_disable_recording_done_callback_.Reset();
  }
  result_file_.reset();
}

#if defined(OS_CHROMEOS) || defined(OS_WIN)
void TracingControllerImpl::OnEndSystemTracingAcked(
    const scoped_refptr<base::RefCountedString>& events_str_ptr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (result_file_)
    result_file_->WriteSystemTrace(events_str_ptr);

  DCHECK(!is_system_tracing_);
  std::vector<std::string> category_groups;
  OnDisableRecordingAcked(NULL, category_groups);
}
#endif

void TracingControllerImpl::OnCaptureMonitoringSnapshotAcked(
    TraceMessageFilter* trace_message_filter) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::OnCaptureMonitoringSnapshotAcked,
                   base::Unretained(this),
                   make_scoped_refptr(trace_message_filter)));
    return;
  }

  if (pending_capture_monitoring_snapshot_ack_count_ == 0)
    return;

  if (trace_message_filter &&
      !pending_capture_monitoring_filters_.erase(trace_message_filter)) {
    // The response from the specified message filter has already been received.
    return;
  }

  if (--pending_capture_monitoring_snapshot_ack_count_ == 1) {
    // All acks from subprocesses have been received. Now flush the local trace.
    // During or after this call, our OnLocalMonitoringTraceDataCollected
    // will be called with the last of the local trace data.
    TraceLog::GetInstance()->FlushButLeaveBufferIntact(
        base::Bind(&TracingControllerImpl::OnLocalMonitoringTraceDataCollected,
                   base::Unretained(this)));
    return;
  }

  if (pending_capture_monitoring_snapshot_ack_count_ != 0)
    return;

  if (monitoring_snapshot_file_) {
    monitoring_snapshot_file_->Close(
        base::Bind(&TracingControllerImpl::OnMonitoringSnapshotFileClosed,
                   base::Unretained(this)));
  }
}

void TracingControllerImpl::OnMonitoringSnapshotFileClosed() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!monitoring_snapshot_file_)
    return;

  if (!pending_capture_monitoring_snapshot_done_callback_.is_null()) {
    pending_capture_monitoring_snapshot_done_callback_.Run(
        monitoring_snapshot_file_->path());
    pending_capture_monitoring_snapshot_done_callback_.Reset();
  }
  monitoring_snapshot_file_.reset();
}

void TracingControllerImpl::OnTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& events_str_ptr) {
  // OnTraceDataCollected may be called from any browser thread, either by the
  // local event trace system or from child processes via TraceMessageFilter.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::OnTraceDataCollected,
                   base::Unretained(this), events_str_ptr));
    return;
  }

  if (result_file_)
    result_file_->Write(events_str_ptr);
}

void TracingControllerImpl::OnMonitoringTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& events_str_ptr) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::OnMonitoringTraceDataCollected,
                   base::Unretained(this), events_str_ptr));
    return;
  }

  if (monitoring_snapshot_file_)
    monitoring_snapshot_file_->Write(events_str_ptr);
}

void TracingControllerImpl::OnLocalTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& events_str_ptr,
    bool has_more_events) {
  if (events_str_ptr->data().size())
    OnTraceDataCollected(events_str_ptr);

  if (has_more_events)
    return;

  // Simulate an DisableRecordingAcked for the local trace.
  std::vector<std::string> category_groups;
  TraceLog::GetInstance()->GetKnownCategoryGroups(&category_groups);
  OnDisableRecordingAcked(NULL, category_groups);
}

void TracingControllerImpl::OnLocalMonitoringTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& events_str_ptr,
    bool has_more_events) {
  if (events_str_ptr->data().size())
    OnMonitoringTraceDataCollected(events_str_ptr);

  if (has_more_events)
    return;

  // Simulate an CaptureMonitoringSnapshotAcked for the local trace.
  OnCaptureMonitoringSnapshotAcked(NULL);
}

void TracingControllerImpl::OnTraceBufferPercentFullReply(
    TraceMessageFilter* trace_message_filter,
    float percent_full) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::OnTraceBufferPercentFullReply,
                   base::Unretained(this),
                   make_scoped_refptr(trace_message_filter),
                   percent_full));
    return;
  }

  if (pending_trace_buffer_percent_full_ack_count_ == 0)
    return;

  if (trace_message_filter &&
      !pending_trace_buffer_percent_full_filters_.erase(trace_message_filter)) {
    // The response from the specified message filter has already been received.
    return;
  }

  maximum_trace_buffer_percent_full_ =
      std::max(maximum_trace_buffer_percent_full_, percent_full);

  if (--pending_trace_buffer_percent_full_ack_count_ == 0) {
    // Trigger callback if one is set.
    pending_trace_buffer_percent_full_callback_.Run(
        maximum_trace_buffer_percent_full_);
    pending_trace_buffer_percent_full_callback_.Reset();
  }
}

void TracingControllerImpl::OnWatchEventMatched() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::OnWatchEventMatched,
                   base::Unretained(this)));
    return;
  }

  if (!watch_event_callback_.is_null())
    watch_event_callback_.Run();
}

void TracingControllerImpl::RegisterTracingUI(TracingUI* tracing_ui) {
  DCHECK(tracing_uis_.find(tracing_ui) == tracing_uis_.end());
  tracing_uis_.insert(tracing_ui);
}

void TracingControllerImpl::UnregisterTracingUI(TracingUI* tracing_ui) {
  std::set<TracingUI*>::iterator it = tracing_uis_.find(tracing_ui);
  DCHECK(it != tracing_uis_.end());
  tracing_uis_.erase(it);
}

void TracingControllerImpl::OnMonitoringStateChanged(bool is_monitoring) {
  if (is_monitoring_ == is_monitoring)
    return;

  is_monitoring_ = is_monitoring;
#if !defined(OS_ANDROID) && !defined(TOOLKIT_QT)
  for (std::set<TracingUI*>::iterator it = tracing_uis_.begin();
       it != tracing_uis_.end(); it++) {
    (*it)->OnMonitoringStateChanged(is_monitoring);
  }
#endif
}

}  // namespace content
