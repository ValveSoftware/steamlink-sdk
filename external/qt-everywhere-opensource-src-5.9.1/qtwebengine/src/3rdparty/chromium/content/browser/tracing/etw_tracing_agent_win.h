// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_ETW_TRACING_AGENT_H_
#define CONTENT_BROWSER_TRACING_ETW_TRACING_AGENT_H_

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/threading/thread.h"
#include "base/trace_event/tracing_agent.h"
#include "base/values.h"
#include "base/win/event_trace_consumer.h"
#include "base/win/event_trace_controller.h"

namespace base {
template <typename Type>
struct DefaultSingletonTraits;
}

namespace content {

class EtwTracingAgent
    : public base::win::EtwTraceConsumerBase<EtwTracingAgent>,
      public base::trace_event::TracingAgent {
 public:
  // base::trace_event::TracingAgent implementation.
  std::string GetTracingAgentName() override;
  std::string GetTraceEventLabel() override;
  void StartAgentTracing(const base::trace_event::TraceConfig& trace_config,
                         const StartAgentTracingCallback& callback) override;
  void StopAgentTracing(const StopAgentTracingCallback& callback) override;

  // Retrieve the ETW consumer instance.
  static EtwTracingAgent* GetInstance();

 private:
  // This allows constructor and destructor to be private and usable only
  // by the Singleton class.
  friend struct base::DefaultSingletonTraits<EtwTracingAgent>;

  // Constructor.
  EtwTracingAgent();
  ~EtwTracingAgent() override;

  void AddSyncEventToBuffer();
  void AppendEventToBuffer(EVENT_TRACE* event);

  // Static override of EtwTraceConsumerBase::ProcessEvent.
  // @param event the raw ETW event to process.
  friend class base::win::EtwTraceConsumerBase<EtwTracingAgent>;
  static void ProcessEvent(EVENT_TRACE* event);

  // Request the ETW trace controller to activate the kernel tracing.
  // returns true on success, false if the kernel tracing isn't activated.
  bool StartKernelSessionTracing();

  // Request the ETW trace controller to deactivate the kernel tracing.
  // @param callback the callback to call with the consumed events.
  // @returns true on success, false if an error occurred.
  bool StopKernelSessionTracing();

  void OnStopSystemTracingDone(
    const StopAgentTracingCallback& callback,
    const scoped_refptr<base::RefCountedString>& result);

  void TraceAndConsumeOnThread();
  void FlushOnThread(const StopAgentTracingCallback& callback);

  std::unique_ptr<base::ListValue> events_;
  base::Thread thread_;
  TRACEHANDLE session_handle_;
  base::win::EtwTraceProperties properties_;

  DISALLOW_COPY_AND_ASSIGN(EtwTracingAgent);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_ETW_TRACING_AGENT_H_
