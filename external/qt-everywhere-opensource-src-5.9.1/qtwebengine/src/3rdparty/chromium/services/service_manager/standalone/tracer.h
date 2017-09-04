// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_STANDALONE_TRACER_H_
#define SERVICES_SERVICE_MANAGER_STANDALONE_TRACER_H_

#include <stddef.h>
#include <stdio.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "mojo/common/data_pipe_drainer.h"
#include "services/tracing/public/interfaces/tracing.mojom.h"

namespace service_manager {

// Tracer collects tracing data from base/trace_event and from externally
// configured sources, aggregates it into a single stream, and writes it out to
// a file. It should be constructed very early in a process' lifetime before any
// initialization that may be interesting to trace has occured and be shut down
// as late as possible to capture as much initialization/shutdown code as
// possible.
class Tracer : public mojo::common::DataPipeDrainer::Client {
 public:
  Tracer();
  ~Tracer() override;

  // Starts tracing the current process with the given set of categories. The
  // tracing results will be saved into the specified filename when
  // StopAndFlushToFile() is called.
  void Start(const std::string& categories,
             const std::string& duration_seconds_str,
             const std::string& filename);

  // Starts collecting data from the tracing service with the given set of
  // categories.
  void StartCollectingFromTracingService(
      tracing::mojom::CollectorPtr coordinator);

  // Stops tracing and flushes all collected trace data to the file specified in
  // Start(). Blocks until the file write is complete. May be called after the
  // message loop is shut down.
  void StopAndFlushToFile();

  void ConnectToProvider(tracing::mojom::ProviderRequest request);

 private:
  void StopTracingAndFlushToDisk();

  // Called from the flush thread. When all data is collected this runs
  // |done_callback| on the flush thread.
  void EndTraceAndFlush(const std::string& filename,
                        const base::Closure& done_callback);

  // Called from the flush thread.
  void WriteTraceDataCollected(
      const base::Closure& done_callback,
      const scoped_refptr<base::RefCountedString>& events_str,
      bool has_more_events);

  // mojo::common::DataPipeDrainer::Client implementation.
  void OnDataAvailable(const void* data, size_t num_bytes) override;
  void OnDataComplete() override;

  // Emits a comma if needed.
  void WriteCommaIfNeeded();

  // Writes trace file footer and closes out the file.
  void WriteFooterAndClose();

  // Set when connected to the tracing service.
  tracing::mojom::CollectorPtr coordinator_;
  std::unique_ptr<mojo::common::DataPipeDrainer> drainer_;

  // Whether we're currently tracing.
  bool tracing_;
  // Categories to trace.
  std::string categories_;

  // Whether we've written the first chunk.
  bool first_chunk_written_;
  std::string trace_service_data_;

  // Trace file, if open.
  FILE* trace_file_;
  std::string trace_filename_;

  DISALLOW_COPY_AND_ASSIGN(Tracer);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_STANDALONE_TRACER_H_
