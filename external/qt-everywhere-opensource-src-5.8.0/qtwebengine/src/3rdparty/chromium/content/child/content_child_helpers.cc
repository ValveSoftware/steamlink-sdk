// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/content_child_helpers.h"

#include <stdint.h>

#include <memory>

#include "base/logging.h"
#include "base/process/process_metrics.h"
#include "build/build_config.h"
#include "v8/include/v8.h"

#if defined(OS_LINUX)
#include <malloc.h>
#endif

namespace content {

// TODO(primiano): get rid of this file together with --memory-metrics.
// This function is both misplaced and misnamed. If useful, this should
// be moved to base/process/process_metrics.h. Regardless the name,
// though, this provides only a partial and misleading value.
// Unfortunately some telemetry benchmark rely on it and these need to
// be refactored before getting rid of this. See crbug.com/581365 .
#if defined(OS_LINUX) || defined(OS_ANDROID)
size_t GetMemoryUsageKB() {
  struct mallinfo minfo = mallinfo();
  uint64_t mem_usage =
#if defined(USE_TCMALLOC)
      minfo.uordblks
#else
      (minfo.hblkhd + minfo.arena)
#endif
      >> 10;

  v8::HeapStatistics stat;
  // TODO(svenpanne) The call below doesn't take web workers into account, this
  // has to be done manually by iterating over all Isolates involved.
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  if (isolate)
    isolate->GetHeapStatistics(&stat);
  return mem_usage + (static_cast<uint64_t>(stat.total_heap_size()) >> 10);
}
#elif defined(OS_MACOSX)
size_t GetMemoryUsageKB() {
  std::unique_ptr<base::ProcessMetrics> process_metrics(
      // The default port provider is sufficient to get data for the current
      // process.
      base::ProcessMetrics::CreateProcessMetrics(
          base::GetCurrentProcessHandle(), NULL));
  return process_metrics->GetWorkingSetSize() >> 10;
}
#else
size_t GetMemoryUsageKB() {
  std::unique_ptr<base::ProcessMetrics> process_metrics(
      base::ProcessMetrics::CreateProcessMetrics(
          base::GetCurrentProcessHandle()));
  return process_metrics->GetPagefileUsage() >> 10;
}
#endif

}  // namespace content
