// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/etw_system_event_consumer_win.h"

#include "base/base64.h"
#include "base/debug/trace_event_impl.h"
#include "base/json/json_string_value_serializer.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

const int kEtwBufferSizeInKBytes = 16;
const int kEtwBufferFlushTimeoutInSeconds = 1;

std::string GuidToString(const GUID& guid) {
  return base::StringPrintf("%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
      guid.Data1, guid.Data2, guid.Data3,
      guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
      guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
}

}  // namespace

EtwSystemEventConsumer::EtwSystemEventConsumer()
    : thread_("EtwConsumerThread") {
}

EtwSystemEventConsumer::~EtwSystemEventConsumer() {
}

bool EtwSystemEventConsumer::StartSystemTracing() {

  // Activate kernel tracing.
  if (!StartKernelSessionTracing())
    return false;

  // Start the consumer thread and start consuming events.
  thread_.Start();
  thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&EtwSystemEventConsumer::TraceAndConsumeOnThread,
                 base::Unretained(this)));

  return true;
}

void EtwSystemEventConsumer::StopSystemTracing(const OutputCallback& callback) {
  // Deactivate kernel tracing.
  if (!StopKernelSessionTracing()) {
    LOG(FATAL) << "Could not stop system tracing.";
  }

  // Stop consuming and flush events.
  OutputCallback on_stop_system_tracing_done_callback =
      base::Bind(&EtwSystemEventConsumer::OnStopSystemTracingDone,
                 base::Unretained(this),
                 callback);
  thread_.message_loop()->PostTask(FROM_HERE,
      base::Bind(&EtwSystemEventConsumer::FlushOnThread,
                 base::Unretained(this), on_stop_system_tracing_done_callback));
}

void EtwSystemEventConsumer::OnStopSystemTracingDone(
    const OutputCallback& callback,
    const scoped_refptr<base::RefCountedString>& result) {

  // Stop the consumer thread.
  thread_.Stop();

  // Pass the serialized events.
  callback.Run(result);
}

bool EtwSystemEventConsumer::StartKernelSessionTracing() {
  // Enabled flags (tracing facilities).
  uint32 enabled_flags = EVENT_TRACE_FLAG_IMAGE_LOAD |
                         EVENT_TRACE_FLAG_PROCESS |
                         EVENT_TRACE_FLAG_THREAD |
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
  if (FAILED(hr)) {
    VLOG(1) << "StartRealtimeSession() failed with " << hr << ".";
    return false;
  }

  return true;
}

bool EtwSystemEventConsumer::StopKernelSessionTracing() {
  HRESULT hr = base::win::EtwTraceController::Stop(
      KERNEL_LOGGER_NAME, &properties_);
  return SUCCEEDED(hr);
}

// static
EtwSystemEventConsumer* EtwSystemEventConsumer::GetInstance() {
  return Singleton<EtwSystemEventConsumer>::get();
}

// static
void EtwSystemEventConsumer::ProcessEvent(EVENT_TRACE* event) {
  EtwSystemEventConsumer::GetInstance()->AppendEventToBuffer(event);
}

void EtwSystemEventConsumer::AddSyncEventToBuffer() {
  // Sync the clocks.
  base::Time walltime = base::Time::NowFromSystemTime();
  base::TimeTicks now = base::TimeTicks::NowFromSystemTraceTime();

  LARGE_INTEGER walltime_in_us;
  walltime_in_us.QuadPart = walltime.ToInternalValue();
  LARGE_INTEGER now_in_us;
  now_in_us.QuadPart = now.ToInternalValue();

  // Add fields to the event.
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
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

void EtwSystemEventConsumer::AppendEventToBuffer(EVENT_TRACE* event) {
  using base::FundamentalValue;

  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());

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

void EtwSystemEventConsumer::TraceAndConsumeOnThread() {
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

void EtwSystemEventConsumer::FlushOnThread(const OutputCallback& callback) {
  // Add the header information to the stream.
  scoped_ptr<base::DictionaryValue> header(new base::DictionaryValue());
  header->Set("name", base::Value::CreateStringValue("ETW"));

  // Release and pass the events buffer.
  header->Set("content", events_.release());

  // Serialize the results as a JSon string.
  std::string output;
  JSONStringValueSerializer serializer(&output);
  serializer.Serialize(*header.get());

  // Pass the result to the UI Thread.
  scoped_refptr<base::RefCountedString> result =
      base::RefCountedString::TakeString(&output);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, result));
}

}  // namespace content
