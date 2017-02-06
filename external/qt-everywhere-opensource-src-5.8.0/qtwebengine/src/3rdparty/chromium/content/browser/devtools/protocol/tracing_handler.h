// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TRACING_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TRACING_HANDLER_H_

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/devtools/protocol/devtools_protocol_dispatcher.h"
#include "content/common/content_export.h"
#include "content/public/browser/tracing_controller.h"

namespace base {
class RefCountedString;
class Timer;
}

namespace content {
namespace devtools {

class DevToolsIOContext;

namespace tracing {

class TracingHandler {
 public:
  typedef DevToolsProtocolClient::Response Response;

  enum Target { Browser, Renderer };
  TracingHandler(Target target,
                 int frame_tree_node_id,
                 DevToolsIOContext* io_context);
  virtual ~TracingHandler();

  void SetClient(std::unique_ptr<Client> client);
  void Detached();

  void OnTraceDataCollected(const std::string& trace_fragment);
  void OnTraceComplete();
  void OnTraceToStreamComplete(const std::string& stream_handle);

  // Protocol methods.
  Response Start(DevToolsCommandId command_id,
                 const std::string* categories,
                 const std::string* options,
                 const double* buffer_usage_reporting_interval,
                 const std::string* transfer_mode,
                 const std::unique_ptr<base::DictionaryValue>& config);
  Response End(DevToolsCommandId command_id);
  Response GetCategories(DevToolsCommandId command);
  Response RequestMemoryDump(DevToolsCommandId command_id);
  Response RecordClockSyncMarker(const std::string& sync_id);

  bool did_initiate_recording() { return did_initiate_recording_; }

 private:
  void OnRecordingEnabled(DevToolsCommandId command_id);
  void OnBufferUsage(float percent_full, size_t approximate_event_count);
  void OnCategoriesReceived(DevToolsCommandId command_id,
                            const std::set<std::string>& category_set);
  void OnMemoryDumpFinished(DevToolsCommandId command_id,
                            uint64_t dump_guid,
                            bool success);

  void SetupTimer(double usage_reporting_interval);
  void StopTracing(
      const scoped_refptr<TracingController::TraceDataSink>& trace_data_sink);
  bool IsTracing() const;
  static bool IsStartupTracingActive();
  CONTENT_EXPORT static base::trace_event::TraceConfig
      GetTraceConfigFromDevToolsConfig(
          const base::DictionaryValue& devtools_config);

  std::unique_ptr<base::Timer> buffer_usage_poll_timer_;
  Target target_;

  std::unique_ptr<Client> client_;
  DevToolsIOContext* io_context_;
  int frame_tree_node_id_;
  bool did_initiate_recording_;
  bool return_as_stream_;
  base::WeakPtrFactory<TracingHandler> weak_factory_;

  FRIEND_TEST_ALL_PREFIXES(TracingHandlerTest,
                           GetTraceConfigFromDevToolsConfig);
  DISALLOW_COPY_AND_ASSIGN(TracingHandler);
};

}  // namespace tracing
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TRACING_HANDLER_H_
