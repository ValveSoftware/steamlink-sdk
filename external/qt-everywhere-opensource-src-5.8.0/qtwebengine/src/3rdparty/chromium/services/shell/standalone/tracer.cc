// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/standalone/tracer.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <utility>

#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/trace_event.h"

namespace shell {

Tracer::Tracer()
    : tracing_(false), first_chunk_written_(false), trace_file_(nullptr) {}

Tracer::~Tracer() {
  StopAndFlushToFile();
}

void Tracer::Start(const std::string& categories,
                   const std::string& duration_seconds_str,
                   const std::string& filename) {
  tracing_ = true;
  trace_filename_ = filename;
  categories_ = categories;
  base::trace_event::TraceConfig config(categories,
                                        base::trace_event::RECORD_UNTIL_FULL);
  base::trace_event::TraceLog::GetInstance()->SetEnabled(
      config, base::trace_event::TraceLog::RECORDING_MODE);

  int trace_duration_secs = 5;
  if (!duration_seconds_str.empty()) {
    CHECK(base::StringToInt(duration_seconds_str, &trace_duration_secs))
        << "Could not parse --trace-startup-duration value "
        << duration_seconds_str;
  }
  base::MessageLoop::current()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&Tracer::StopAndFlushToFile, base::Unretained(this)),
      base::TimeDelta::FromSeconds(trace_duration_secs));
}

void Tracer::StartCollectingFromTracingService(
    tracing::TraceCollectorPtr coordinator) {
  coordinator_ = std::move(coordinator);
  mojo::DataPipe data_pipe;
  coordinator_->Start(std::move(data_pipe.producer_handle), categories_);
  drainer_.reset(new mojo::common::DataPipeDrainer(
      this, std::move(data_pipe.consumer_handle)));
}

void Tracer::StopAndFlushToFile() {
  if (tracing_)
    StopTracingAndFlushToDisk();
}

void Tracer::ConnectToProvider(
    mojo::InterfaceRequest<tracing::TraceProvider> request) {
  trace_provider_impl_.Bind(std::move(request));
}

void Tracer::StopTracingAndFlushToDisk() {
  tracing_ = false;
  trace_file_ = fopen(trace_filename_.c_str(), "w+");
  PCHECK(trace_file_);
  static const char kStart[] = "{\"traceEvents\":[";
  PCHECK(fwrite(kStart, 1, strlen(kStart), trace_file_) == strlen(kStart));

  // At this point we might be connected to the tracing service, in which case
  // we want to tell it to stop tracing and we will send the data we've
  // collected in process to it.
  if (coordinator_) {
    coordinator_->StopAndFlush();
  } else {
    // Or we might not be connected. If we aren't connected to the tracing
    // service we want to collect the tracing data gathered ourselves and flush
    // it to disk. We do this in a blocking fashion (for this thread) so we can
    // gather as much data as possible on shutdown.
    base::trace_event::TraceLog::GetInstance()->SetDisabled();
    {
      base::WaitableEvent flush_complete_event(
          base::WaitableEvent::ResetPolicy::AUTOMATIC,
          base::WaitableEvent::InitialState::NOT_SIGNALED);
      // TraceLog::Flush requires a message loop but we've already shut ours
      // down.
      // Spin up a new thread to flush things out.
      base::Thread flush_thread("mojo_runner_trace_event_flush");
      flush_thread.Start();
      flush_thread.task_runner()->PostTask(
          FROM_HERE,
          base::Bind(&Tracer::EndTraceAndFlush, base::Unretained(this),
                     trace_filename_,
                     base::Bind(&base::WaitableEvent::Signal,
                                base::Unretained(&flush_complete_event))));
      base::trace_event::TraceLog::GetInstance()
          ->SetCurrentThreadBlocksMessageLoop();
      flush_complete_event.Wait();
    }
  }
}

void Tracer::WriteFooterAndClose() {
  static const char kEnd[] = "]}";
  PCHECK(fwrite(kEnd, 1, strlen(kEnd), trace_file_) == strlen(kEnd));
  PCHECK(fclose(trace_file_) == 0);
  trace_file_ = nullptr;
  LOG(ERROR) << "Wrote trace data to " << trace_filename_;
}

void Tracer::EndTraceAndFlush(const std::string& filename,
                              const base::Closure& done_callback) {
  base::trace_event::TraceLog::GetInstance()->SetDisabled();
  base::trace_event::TraceLog::GetInstance()->Flush(base::Bind(
      &Tracer::WriteTraceDataCollected, base::Unretained(this), done_callback));
}

void Tracer::WriteTraceDataCollected(
    const base::Closure& done_callback,
    const scoped_refptr<base::RefCountedString>& events_str,
    bool has_more_events) {
  if (events_str->size()) {
    WriteCommaIfNeeded();

    PCHECK(fwrite(events_str->data().c_str(), 1, events_str->data().length(),
                  trace_file_) == events_str->data().length());
  }

  if (!has_more_events && !done_callback.is_null())
    done_callback.Run();
}

void Tracer::OnDataAvailable(const void* data, size_t num_bytes) {
  const char* chars = static_cast<const char*>(data);
  trace_service_data_.append(chars, num_bytes);
}

void Tracer::OnDataComplete() {
  if (!trace_service_data_.empty()) {
    WriteCommaIfNeeded();
    const char* const chars = trace_service_data_.data();
    size_t num_bytes = trace_service_data_.length();
    PCHECK(fwrite(chars, 1, num_bytes, trace_file_) == num_bytes);
    trace_service_data_ = std::string();
  }
  drainer_.reset();
  coordinator_.reset();
  WriteFooterAndClose();
}

void Tracer::WriteCommaIfNeeded() {
  if (first_chunk_written_)
    PCHECK(fwrite(",", 1, 1, trace_file_) == 1);
  first_chunk_written_ = true;
}

}  // namespace shell
