// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_PROCESS_MEMORY_METRICS_DUMP_PROVIDER_H_
#define COMPONENTS_TRACING_COMMON_PROCESS_MEMORY_METRICS_DUMP_PROVIDER_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/trace_event/memory_dump_provider.h"
#include "build/build_config.h"
#include "components/tracing/tracing_export.h"

namespace base {
class ProcessMetrics;
}

namespace tracing {

// Dump provider which collects process-wide memory stats.
class TRACING_EXPORT ProcessMetricsMemoryDumpProvider
    : public base::trace_event::MemoryDumpProvider {
 public:
  // Pass base::kNullProcessId to register for current process.
  static void RegisterForProcess(base::ProcessId process);
  static void UnregisterForProcess(base::ProcessId process);

  ~ProcessMetricsMemoryDumpProvider() override;

  // MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ProcessMetricsMemoryDumpProviderTest,
                           ParseProcSmaps);
  FRIEND_TEST_ALL_PREFIXES(ProcessMetricsMemoryDumpProviderTest, DumpRSS);

  ProcessMetricsMemoryDumpProvider(base::ProcessId process);

  bool DumpProcessTotals(const base::trace_event::MemoryDumpArgs& args,
                         base::trace_event::ProcessMemoryDump* pmd);
  bool DumpProcessMemoryMaps(const base::trace_event::MemoryDumpArgs& args,
                             base::trace_event::ProcessMemoryDump* pmd);

  static uint64_t rss_bytes_for_testing;

#if defined(OS_LINUX) || defined(OS_ANDROID)
  static FILE* proc_smaps_for_testing;
#endif

  base::ProcessId process_;
  std::unique_ptr<base::ProcessMetrics> process_metrics_;

  // The peak may not be resettable on all the processes if the linux kernel is
  // older than http://bit.ly/reset_rss or only on child processes if yama LSM
  // sandbox is enabled.
  bool is_rss_peak_resettable_;

  DISALLOW_COPY_AND_ASSIGN(ProcessMetricsMemoryDumpProvider);
};

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_PROCESS_MEMORY_METRICS_DUMP_PROVIDER_H_
