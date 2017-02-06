// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/etw_tracing_agent_win.h"

#include <stdint.h>

#include "base/base64.h"
#include "base/json/json_string_value_serializer.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

const char kETWTracingAgentName[] = "etw";
const char kETWTraceLabel[] = "systemTraceEvents";

const int kEtwBufferSizeInKBytes = 16;
const int kEtwBufferFlushTimeoutInSeconds = 1;

std::string GuidToString(const GUID& guid) {
  return base::StringPrintf("%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
      guid.Data1, guid.Data2, guid.Data3,
      guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
      guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
}

}  // namespace

EtwTracingAgent::EtwTracingAgent()
    : thread_("EtwConsumerThread") {
}

EtwTracingAgent::~EtwTracingAgent() {
}

std::string EtwTracingAgent::GetTracingAgentName() {
  return kETWTracingAgentName;
}

std::string EtwTracingAgent::GetTraceEventLabel() {
  return kETWTraceLabel;
}

void EtwTracingAgent::StartAgentTracing(
    const base::trace_event::TraceConfig& trace_config,
    const StartAgentTracingCallback& callback) {
  // Activate kernel tracing.
  if (!StartKernelSessionTracing()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, GetTracingAgentName(), false /* success */));
    return;
  }

  // Start the consumer thread and start consuming events.
  thread_.Start();
  thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&EtwTracingAgent::TraceAndConsumeOnThread,
                 base::Unretained(this)));

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, GetTracingAgentName(), true /* success */));
}

void EtwTracingAgent::StopAgentTracing(
    const StopAgentTracingCallback& callback) {
  // Deactivate kernel tracing.
  if (!StopKernelSessionTracing()) {
    LOG(FATAL) << "Could not stop system tracing.";
  }

  // Stop consuming and flush events.
  thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&EtwTracingAgent::FlushOnThread,
                 base::Unretained(this),
                 callback));
}

void EtwTracingAgent::OnStopSystemTracingDone(
    const StopAgentTracingCallback& callback,
    const scoped_refptr<base::RefCountedString>& result) {

  // Stop the consumer thread.
  thread_.Stop();

  // Pass the serialized events.
  callback.Run(GetTracingAgentName(), GetTraceEventLabel(), result);
}

bool EtwTracingAgent::StartKernelSessionTracing() {
  // Enabled flags (tracing facilities).
  uint32_t enabled_flags = EVENT_TRACE_FLAG_IMAGE_LOAD |
                           EVENT_TRACE_FLAG_PROCESS | EVENT_TRACE_FLAG_THREAD |
                           EVENT_TRACE_FLAG_CSWITCH;

  EVENT_TRACE_PROPERTIES& p = *properties_.get();
  p.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
  p.FlushTimer = kEtwBufferFlushTimeoutInSeconds;
  p.BufferSize = kEtwBufferSizeInKBytes;
  p.LogFileNameOffset = 0;
  p.EnableFlags = enabled_flags;
  p.Wnode.ClientContext = 1;  // QPC timer accuracy.

  HRESULT hr = base::win::EtwTraceController::Start(
      KERNEL_LOGGER_NAME, &properties_, &session_handle_);

  // It's possible that a previous tracing session has been orphaned. If so
  // try stopping and restarting it.
  if (hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) {
    VLOG(1) << "Session already exists, stopping and restarting it.";
    hr = base::win::EtwTraceController::Stop(
        KERNEL_LOGGER_NAME, &properties_);
    if (FAILED(hr)) {
      VLOG(1) << "EtwTraceController::Stop failed with " << hr << ".";
      return false;
    }

    // The session was successfully shutdown so try to restart it.
    hr = base::win::EtwTraceController::Start(
        KERNEL_LOGGER_NAME, &properties_, &session_handle_);
  }

  if (FAILED(hr)) {
    VLOG(1) << "EtwTraceController::Start failed with " << hr << ".";
    return false;
  }

  return true;
}

bool EtwTracingAgent::StopKernelSessionTracing() {
  HRESULT hr = base::win::EtwTraceController::Stop(
      KERNEL_LOGGER_NAME, &properties_);
  return SUCCEEDED(hr);
}

// static
EtwTracingAgent* EtwTracingAgent::GetInstance() {
  return base::Singleton<EtwTracingAgent>::get();
}

// static
void EtwTracingAgent::ProcessEvent(EVENT_TRACE* event) {
  EtwTracingAgent::GetInstance()->AppendEventToBuffer(event);
}

void EtwTracingAgent::AddSyncEventToBuffer() {
  // Sync the clocks.
  base::Time walltime = base::Time::NowFromSystemTime();
  base::TimeTicks now = base::TimeTicks::Now();

  LARGE_INTEGER walltime_in_us;
  walltime_in_us.QuadPart = walltime.ToInternalValue();
  LARGE_INTEGER now_in_us;
  now_in_us.QuadPart = now.ToInternalValue();

  // Add fields to the event.
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->Set("guid", new base::StringValue("ClockSync"));
  value->Set("walltime", new base::StringValue(
      base::StringPrintf("%08X%08X",
                         walltime_in_us.HighPart,
                         walltime_in_us.LowPart)));
  value->Set("tick", new base::StringValue(
      base::StringPrintf("%08X%08X",
                         now_in_us.HighPart,
                         now_in_us.LowPart)));

  // Append it to the events buffer.
  events_->Append(value.release());
}

void EtwTracingAgent::AppendEventToBuffer(EVENT_TRACE* event) {
  using base::FundamentalValue;

  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());

  // Add header fields to the event.
  LARGE_INTEGER ts_us;
  ts_us.QuadPart = event->Header.TimeStamp.QuadPart / 10;
  value->Set("ts", new base::StringValue(
      base::StringPrintf("%08X%08X", ts_us.HighPart, ts_us.LowPart)));

  value->Set("guid", new base::StringValue(GuidToString(event->Header.Guid)));

  value->Set("op", new FundamentalValue(event->Header.Class.Type));
  value->Set("ver", new FundamentalValue(event->Header.Class.Version));
  value->Set("pid",
             new FundamentalValue(static_cast<int>(event->Header.ProcessId)));
  value->Set("tid",
             new FundamentalValue(static_cast<int>(event->Header.ThreadId)));
  value->Set("cpu", new FundamentalValue(event->BufferContext.ProcessorNumber));

  // Base64 encode the payload bytes.
  base::StringPiece buffer(static_cast<const char*>(event->MofData),
                           event->MofLength);
  std::string payload;
  base::Base64Encode(buffer, &payload);
  value->Set("payload", new base::StringValue(payload));

  // Append it to the events buffer.
  events_->Append(value.release());
}

void EtwTracingAgent::TraceAndConsumeOnThread() {
  // Create the events buffer.
  events_.reset(new base::ListValue());

  // Output a clock sync event.
  AddSyncEventToBuffer();

  HRESULT hr = OpenRealtimeSession(KERNEL_LOGGER_NAME);
  if (FAILED(hr))
    return;
  Consume();
  Close();
}

void EtwTracingAgent::FlushOnThread(
    const StopAgentTracingCallback& callback) {
  // Add the header information to the stream.
  std::unique_ptr<base::DictionaryValue> header(new base::DictionaryValue());
  header->Set("name", new base::StringValue("ETW"));

  // Release and pass the events buffer.
  header->Set("content", events_.release());

  // Serialize the results as a JSon string.
  std::string output;
  JSONStringValueSerializer serializer(&output);
  serializer.Serialize(*header.get());

  // Pass the result to the UI Thread.
  scoped_refptr<base::RefCountedString> result =
      base::RefCountedString::TakeString(&output);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&EtwTracingAgent::OnStopSystemTracingDone,
                 base::Unretained(this),
                 callback,
                 result));
}

}  // namespace content
