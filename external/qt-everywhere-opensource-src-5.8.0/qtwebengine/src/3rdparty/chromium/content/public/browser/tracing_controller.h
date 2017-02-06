// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_TRACING_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_TRACING_CONTROLLER_H_

#include <stddef.h>

#include <set>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/public/browser/tracing_delegate.h"

namespace content {

class TracingController;

// TracingController is used on the browser processes to enable/disable
// tracing status and collect trace data. Only the browser UI thread is allowed
// to interact with the TracingController object. All callbacks are called on
// the UI thread.
class TracingController {
 public:

  CONTENT_EXPORT static TracingController* GetInstance();

  // An interface for trace data consumer. An implementation of this interface
  // is passed to DisableTracing() and receives the trace data
  // followed by a notification that all child processes
  // have completed tracing and the data collection is over.
  // All methods are called on the UI thread.
  // Close method will be called exactly once and no methods will be
  // called after that.
  class CONTENT_EXPORT TraceDataSink
      : public base::RefCountedThreadSafe<TraceDataSink> {
   public:
    TraceDataSink();

    virtual void AddTraceChunk(const std::string& chunk) {}

    // Add a TracingAgent's trace to the data sink.
    virtual void AddAgentTrace(const std::string& trace_label,
                               const std::string& trace_data);

    // Notice that TracingController adds some default metadata when
    // StopTracing is called, which may override metadata that you would
    // set beforehand in case of key collision.
    virtual void AddMetadata(const base::DictionaryValue& data);
    virtual std::unique_ptr<const base::DictionaryValue> GetMetadataCopy()
        const;
    virtual void SetMetadataFilterPredicate(
        const MetadataFilterPredicate& metadata_filter_predicate);
    virtual void Close() {}

   protected:
    friend class base::RefCountedThreadSafe<TraceDataSink>;

    // Get a map of TracingAgent's data, which is previously added by
    // AddAgentTrace(). The map's key is the trace label and the map's value is
    // the trace data.
    virtual const std::map<std::string, std::string>& GetAgentTrace() const;

    virtual ~TraceDataSink();

   private:
    std::map<std::string, std::string> additional_tracing_agent_trace_;
    MetadataFilterPredicate metadata_filter_predicate_;
    base::DictionaryValue metadata_;
  };

  // An implementation of this interface is passed when constructing a
  // TraceDataSink, and receives chunks of the final trace data as it's being
  // constructed.
  // Methods may be called from any thread.
  class CONTENT_EXPORT TraceDataEndpoint
      : public base::RefCountedThreadSafe<TraceDataEndpoint> {
   public:
    virtual void ReceiveTraceChunk(const std::string& chunk) {}
    virtual void ReceiveTraceFinalContents(
        std::unique_ptr<const base::DictionaryValue> metadata,
        const std::string& contents) {}

   protected:
    friend class base::RefCountedThreadSafe<TraceDataEndpoint>;
    virtual ~TraceDataEndpoint() {}
  };

  // Create a trace sink that may be supplied to StopTracing
  // to capture the trace data as a string.
  CONTENT_EXPORT static scoped_refptr<TraceDataSink> CreateStringSink(
      const base::Callback<void(std::unique_ptr<const base::DictionaryValue>,
                                base::RefCountedString*)>& callback);

  CONTENT_EXPORT static scoped_refptr<TraceDataSink> CreateCompressedStringSink(
      scoped_refptr<TraceDataEndpoint> endpoint);

  // Create a trace sink that may be supplied to StopTracing
  // to dump the trace data to a file.
  CONTENT_EXPORT static scoped_refptr<TraceDataSink> CreateFileSink(
      const base::FilePath& file_path,
      const base::Closure& callback);

  // Create an endpoint that may be supplied to any TraceDataSink to
  // dump the trace data to a callback.
  CONTENT_EXPORT static scoped_refptr<TraceDataEndpoint> CreateCallbackEndpoint(
      const base::Callback<void(std::unique_ptr<const base::DictionaryValue>,
                                base::RefCountedString*)>& callback);

  // Create an endpoint that may be supplied to any TraceDataSink to
  // dump the trace data to a file.
  CONTENT_EXPORT static scoped_refptr<TraceDataEndpoint> CreateFileEndpoint(
      const base::FilePath& file_path,
      const base::Closure& callback);

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

  // Start tracing (recording traces) on all processes.
  //
  // Tracing begins immediately locally, and asynchronously on child processes
  // as soon as they receive the StartTracing request.
  //
  // Once all child processes have acked to the StartTracing request,
  // StartTracingDoneCallback will be called back.
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
  // |trace_config| controls what kind of tracing is enabled.
  typedef base::Callback<void()> StartTracingDoneCallback;
  virtual bool StartTracing(
      const base::trace_event::TraceConfig& trace_config,
      const StartTracingDoneCallback& callback) = 0;

  // Stop tracing (recording traces) on all processes.
  //
  // Child processes typically are caching trace data and only rarely flush
  // and send trace data back to the browser process. That is because it may be
  // an expensive operation to send the trace data over IPC, and we would like
  // to avoid much runtime overhead of tracing. So, to end tracing, we must
  // asynchronously ask all child processes to flush any pending trace data.
  //
  // Once all child processes have acked to the StopTracing request,
  // TracingFileResultCallback will be called back with a file that contains
  // the traced data.
  //
  // If |trace_data_sink| is not null, it will receive chunks of trace data
  // as a comma-separated sequences of JSON-stringified events, followed by
  // a notification that the trace collection is finished.
  //
  virtual bool StopTracing(
      const scoped_refptr<TraceDataSink>& trace_data_sink) = 0;

  // Get the maximum across processes of trace buffer percent full state.
  // When the TraceBufferUsage value is determined, the callback is
  // called.
  typedef base::Callback<void(float, size_t)> GetTraceBufferUsageCallback;
  virtual bool GetTraceBufferUsage(
      const GetTraceBufferUsageCallback& callback) = 0;

  // |callback| will will be called every time the given event occurs on any
  // process.
  typedef base::Callback<void()> WatchEventCallback;
  virtual bool SetWatchEvent(const std::string& category_name,
                             const std::string& event_name,
                             const WatchEventCallback& callback) = 0;

  // Cancel the watch event. If tracing is enabled, this may race with the
  // watch event callback.
  virtual bool CancelWatchEvent() = 0;

  // Check if the tracing system is tracing
  virtual bool IsTracing() const = 0;

 protected:
  virtual ~TracingController() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_TRACING_CONTROLLER_H_
