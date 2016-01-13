// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_TRACING_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_TRACING_HANDLER_H_

#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/devtools_protocol.h"
#include "content/public/browser/tracing_controller.h"

namespace base {
class RefCountedString;
class Timer;
}

namespace content {

// This class bridges DevTools remote debugging server with the trace
// infrastructure.
class DevToolsTracingHandler : public DevToolsProtocol::Handler {
 public:
  enum Target { Browser, Renderer };
  explicit DevToolsTracingHandler(Target target);
  virtual ~DevToolsTracingHandler();

  void OnClientDetached();

 private:
  void BeginReadingRecordingResult(const base::FilePath& path);
  void ReadRecordingResult(const scoped_refptr<base::RefCountedString>& result);
  void OnTraceDataCollected(const std::string& trace_fragment);
  void OnTracingStarted(scoped_refptr<DevToolsProtocol::Command> command);
  void OnBufferUsage(float usage);

  scoped_refptr<DevToolsProtocol::Response> OnStart(
      scoped_refptr<DevToolsProtocol::Command> command);
  scoped_refptr<DevToolsProtocol::Response> OnEnd(
      scoped_refptr<DevToolsProtocol::Command> command);

  scoped_refptr<DevToolsProtocol::Response> OnGetCategories(
      scoped_refptr<DevToolsProtocol::Command> command);
  void OnCategoriesReceived(scoped_refptr<DevToolsProtocol::Command> command,
                            const std::set<std::string>& category_set);

  TracingController::Options TraceOptionsFromString(const std::string& options);

  void DisableRecording(
      const TracingController::TracingFileResultCallback& callback =
          TracingController::TracingFileResultCallback());

  base::WeakPtrFactory<DevToolsTracingHandler> weak_factory_;
  scoped_ptr<base::Timer> buffer_usage_poll_timer_;
  Target target_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsTracingHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_TRACING_HANDLER_H_
