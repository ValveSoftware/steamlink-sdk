// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_TRACING_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_TRACING_CONTROLLER_H_

#include <set>
#include <string>

#include "base/callback.h"
#include "content/common/content_export.h"

namespace base {
class FilePath;
};

namespace content {

class TracingController;

// TracingController is used on the browser processes to enable/disable
// trace status and collect trace data. Only the browser UI thread is allowed
// to interact with the TracingController object. All callbacks are called on
// the UI thread.
class TracingController {
 public:
  enum Options {
    DEFAULT_OPTIONS = 0,
    ENABLE_SYSTRACE = 1 << 0,
    ENABLE_SAMPLING = 1 << 1,
    RECORD_CONTINUOUSLY = 1 << 2,  // For EnableRecording() only.
  };

  CONTENT_EXPORT static TracingController* GetInstance();

  // Get a set of category groups. The category groups can change as
  // new code paths are reached.
  //
  // Once all child processes have acked to the GetCategories request,
  // GetCategoriesDoneCallback is called back with a set of category
  // groups.
  typedef base::Callback<void(const std::set<std::string>&)>
      GetCategoriesDoneCallback;
  virtual bool GetCategories(
      const GetCategoriesDoneCallback& callback) = 0;

  // Start recording on all processes.
  //
  // Recording begins immediately locally, and asynchronously on child processes
  // as soon as they receive the EnableRecording request.
  //
  // Once all child processes have acked to the EnableRecording request,
  // EnableRecordingDoneCallback will be called back.
  //
  // |category_filter| is a filter to control what category groups should be
  // traced. A filter can have an optional '-' prefix to exclude category groups
  // that contain a matching category. Having both included and excluded
  // category patterns in the same list would not be supported.
  //
  // Examples: "test_MyTest*",
  //           "test_MyTest*,test_OtherStuff",
  //           "-excluded_category1,-excluded_category2"
  //
  // |options| controls what kind of tracing is enabled.
  typedef base::Callback<void()> EnableRecordingDoneCallback;
  virtual bool EnableRecording(
      const std::string& category_filter,
      TracingController::Options options,
      const EnableRecordingDoneCallback& callback) = 0;

  // Stop recording on all processes.
  //
  // Child processes typically are caching trace data and only rarely flush
  // and send trace data back to the browser process. That is because it may be
  // an expensive operation to send the trace data over IPC, and we would like
  // to avoid much runtime overhead of tracing. So, to end tracing, we must
  // asynchronously ask all child processes to flush any pending trace data.
  //
  // Once all child processes have acked to the DisableRecording request,
  // TracingFileResultCallback will be called back with a file that contains
  // the traced data.
  //
  // Trace data will be written into |result_file_path| if it is not empty, or
  // into a temporary file. The actual file path will be passed to |callback| if
  // it's not null.
  //
  // If |result_file_path| is empty and |callback| is null, trace data won't be
  // written to any file.
  typedef base::Callback<void(const base::FilePath&)> TracingFileResultCallback;
  virtual bool DisableRecording(const base::FilePath& result_file_path,
                                const TracingFileResultCallback& callback) = 0;

  // Start monitoring on all processes.
  //
  // Monitoring begins immediately locally, and asynchronously on child
  // processes as soon as they receive the EnableMonitoring request.
  //
  // Once all child processes have acked to the EnableMonitoring request,
  // EnableMonitoringDoneCallback will be called back.
  //
  // |category_filter| is a filter to control what category groups should be
  // traced.
  //
  // |options| controls what kind of tracing is enabled.
  typedef base::Callback<void()> EnableMonitoringDoneCallback;
  virtual bool EnableMonitoring(
      const std::string& category_filter,
      TracingController::Options options,
      const EnableMonitoringDoneCallback& callback) = 0;

  // Stop monitoring on all processes.
  //
  // Once all child processes have acked to the DisableMonitoring request,
  // DisableMonitoringDoneCallback is called back.
  typedef base::Callback<void()> DisableMonitoringDoneCallback;
  virtual bool DisableMonitoring(
      const DisableMonitoringDoneCallback& callback) = 0;

  // Get the current monitoring configuration.
  virtual void GetMonitoringStatus(bool* out_enabled,
                                   std::string* out_category_filter,
                                   TracingController::Options* out_options) = 0;

  // Get the current monitoring traced data.
  //
  // Child processes typically are caching trace data and only rarely flush
  // and send trace data back to the browser process. That is because it may be
  // an expensive operation to send the trace data over IPC, and we would like
  // to avoid much runtime overhead of tracing. So, to end tracing, we must
  // asynchronously ask all child processes to flush any pending trace data.
  //
  // Once all child processes have acked to the CaptureMonitoringSnapshot
  // request, TracingFileResultCallback will be called back with a file that
  // contains the traced data.
  //
  // Trace data will be written into |result_file_path| if it is not empty, or
  // into a temporary file. The actual file path will be passed to |callback|.
  //
  // If |result_file_path| is empty and |callback| is null, trace data won't be
  // written to any file.
  virtual bool CaptureMonitoringSnapshot(
      const base::FilePath& result_file_path,
      const TracingFileResultCallback& callback) = 0;

  // Get the maximum across processes of trace buffer percent full state.
  // When the TraceBufferPercentFull value is determined, the callback is
  // called.
  typedef base::Callback<void(float)> GetTraceBufferPercentFullCallback;
  virtual bool GetTraceBufferPercentFull(
      const GetTraceBufferPercentFullCallback& callback) = 0;

  // |callback| will will be called every time the given event occurs on any
  // process.
  typedef base::Callback<void()> WatchEventCallback;
  virtual bool SetWatchEvent(const std::string& category_name,
                             const std::string& event_name,
                             const WatchEventCallback& callback) = 0;

  // Cancel the watch event. If tracing is enabled, this may race with the
  // watch event callback.
  virtual bool CancelWatchEvent() = 0;

 protected:
  virtual ~TracingController() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_TRACING_CONTROLLER_H_
