// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/process_metrics_memory_dump_provider.h"

#include <fcntl.h>
#include <stdint.h>

#include <map>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/format_macros.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/process_memory_maps.h"
#include "base/trace_event/process_memory_totals.h"
#include "build/build_config.h"

namespace tracing {

namespace {

base::LazyInstance<
    std::map<base::ProcessId,
             std::unique_ptr<ProcessMetricsMemoryDumpProvider>>>::Leaky
    g_dump_providers_map = LAZY_INSTANCE_INITIALIZER;

#if defined(OS_LINUX) || defined(OS_ANDROID)
const char kClearPeakRssCommand[] = "5";

const uint32_t kMaxLineSize = 4096;

bool ParseSmapsHeader(const char* header_line,
                      base::trace_event::ProcessMemoryMaps::VMRegion* region) {
  // e.g., "00400000-00421000 r-xp 00000000 fc:01 1234  /foo.so\n"
  bool res = true;  // Whether this region should be appended or skipped.
  uint64_t end_addr = 0;
  char protection_flags[5] = {0};
  char mapped_file[kMaxLineSize];

  if (sscanf(header_line, "%" SCNx64 "-%" SCNx64 " %4c %*s %*s %*s%4095[^\n]\n",
             &region->start_address, &end_addr, protection_flags,
             mapped_file) != 4)
    return false;

  if (end_addr > region->start_address) {
    region->size_in_bytes = end_addr - region->start_address;
  } else {
    // This is not just paranoia, it can actually happen (See crbug.com/461237).
    region->size_in_bytes = 0;
    res = false;
  }

  region->protection_flags = 0;
  if (protection_flags[0] == 'r') {
    region->protection_flags |=
      base::trace_event::ProcessMemoryMaps::VMRegion::kProtectionFlagsRead;
  }
  if (protection_flags[1] == 'w') {
    region->protection_flags |=
      base::trace_event::ProcessMemoryMaps::VMRegion::kProtectionFlagsWrite;
  }
  if (protection_flags[2] == 'x') {
    region->protection_flags |=
      base::trace_event::ProcessMemoryMaps::VMRegion::kProtectionFlagsExec;
  }
  if (protection_flags[3] == 's') {
    region->protection_flags |=
      base::trace_event::ProcessMemoryMaps::VMRegion::kProtectionFlagsMayshare;
  }

  region->mapped_file = mapped_file;
  base::TrimWhitespaceASCII(region->mapped_file, base::TRIM_ALL,
                            &region->mapped_file);

  return res;
}

uint64_t ReadCounterBytes(char* counter_line) {
  uint64_t counter_value = 0;
  int res = sscanf(counter_line, "%*s %" SCNu64 " kB", &counter_value);
  return res == 1 ? counter_value * 1024 : 0;
}

uint32_t ParseSmapsCounter(
    char* counter_line,
    base::trace_event::ProcessMemoryMaps::VMRegion* region) {
  // A smaps counter lines looks as follows: "RSS:  0 Kb\n"
  uint32_t res = 1;
  char counter_name[20];
  int did_read = sscanf(counter_line, "%19[^\n ]", counter_name);
  if (did_read != 1)
    return 0;

  if (strcmp(counter_name, "Pss:") == 0) {
    region->byte_stats_proportional_resident = ReadCounterBytes(counter_line);
  } else if (strcmp(counter_name, "Private_Dirty:") == 0) {
    region->byte_stats_private_dirty_resident = ReadCounterBytes(counter_line);
  } else if (strcmp(counter_name, "Private_Clean:") == 0) {
    region->byte_stats_private_clean_resident = ReadCounterBytes(counter_line);
  } else if (strcmp(counter_name, "Shared_Dirty:") == 0) {
    region->byte_stats_shared_dirty_resident = ReadCounterBytes(counter_line);
  } else if (strcmp(counter_name, "Shared_Clean:") == 0) {
    region->byte_stats_shared_clean_resident = ReadCounterBytes(counter_line);
  } else if (strcmp(counter_name, "Swap:") == 0) {
    region->byte_stats_swapped = ReadCounterBytes(counter_line);
  } else {
    res = 0;
  }

  return res;
}

uint32_t ReadLinuxProcSmapsFile(FILE* smaps_file,
                                base::trace_event::ProcessMemoryMaps* pmm) {
  if (!smaps_file)
    return 0;

  fseek(smaps_file, 0, SEEK_SET);

  char line[kMaxLineSize];
  const uint32_t kNumExpectedCountersPerRegion = 6;
  uint32_t counters_parsed_for_current_region = 0;
  uint32_t num_valid_regions = 0;
  base::trace_event::ProcessMemoryMaps::VMRegion region;
  bool should_add_current_region = false;
  for (;;) {
    line[0] = '\0';
    if (fgets(line, kMaxLineSize, smaps_file) == nullptr || !strlen(line))
      break;
    if (isxdigit(line[0]) && !isupper(line[0])) {
      region = base::trace_event::ProcessMemoryMaps::VMRegion();
      counters_parsed_for_current_region = 0;
      should_add_current_region = ParseSmapsHeader(line, &region);
    } else {
      counters_parsed_for_current_region += ParseSmapsCounter(line, &region);
      DCHECK_LE(counters_parsed_for_current_region,
                kNumExpectedCountersPerRegion);
      if (counters_parsed_for_current_region == kNumExpectedCountersPerRegion) {
        if (should_add_current_region) {
          pmm->AddVMRegion(region);
          ++num_valid_regions;
          should_add_current_region = false;
        }
      }
    }
  }
  return num_valid_regions;
}
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

std::unique_ptr<base::ProcessMetrics> CreateProcessMetrics(
    base::ProcessId process) {
  if (process == base::kNullProcessId)
    return base::WrapUnique(
        base::ProcessMetrics::CreateCurrentProcessMetrics());
#if defined(OS_LINUX) || defined(OS_ANDROID)
  // Just pass ProcessId instead of handle since they are the same in linux and
  // android.
  return base::WrapUnique(base::ProcessMetrics::CreateProcessMetrics(process));
#else
  // Creating process metrics for child processes in mac or windows requires
  // additional information like ProcessHandle or port provider. This is a non
  // needed use case.
  NOTREACHED();
  return std::unique_ptr<base::ProcessMetrics>();
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)
}

}  // namespace

// static
uint64_t ProcessMetricsMemoryDumpProvider::rss_bytes_for_testing = 0;

#if defined(OS_LINUX) || defined(OS_ANDROID)

// static
FILE* ProcessMetricsMemoryDumpProvider::proc_smaps_for_testing = nullptr;

bool ProcessMetricsMemoryDumpProvider::DumpProcessMemoryMaps(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  uint32_t res = 0;
  if (proc_smaps_for_testing) {
    res = ReadLinuxProcSmapsFile(proc_smaps_for_testing, pmd->process_mmaps());
  } else {
    std::string file_name = "/proc/" + (process_ == base::kNullProcessId
                                            ? "self"
                                            : base::IntToString(process_)) +
                            "/smaps";
    base::ScopedFILE smaps_file(fopen(file_name.c_str(), "r"));
    res = ReadLinuxProcSmapsFile(smaps_file.get(), pmd->process_mmaps());
  }

  if (res)
    pmd->set_has_process_mmaps();
  return res;
}
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

// static
void ProcessMetricsMemoryDumpProvider::RegisterForProcess(
    base::ProcessId process) {
  std::unique_ptr<ProcessMetricsMemoryDumpProvider> metrics_provider(
      new ProcessMetricsMemoryDumpProvider(process));
  base::trace_event::MemoryDumpProvider::Options options;
  options.target_pid = process;
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      metrics_provider.get(), "ProcessMemoryMetrics", nullptr, options);
  bool did_insert =
      g_dump_providers_map.Get()
          .insert(std::make_pair(process, std::move(metrics_provider)))
          .second;
  if (!did_insert) {
    DLOG(ERROR) << "ProcessMetricsMemoryDumpProvider already registered for "
                << (process == base::kNullProcessId
                        ? "current process"
                        : "process id " + base::IntToString(process));
  }
}

// static
void ProcessMetricsMemoryDumpProvider::UnregisterForProcess(
    base::ProcessId process) {
  auto iter = g_dump_providers_map.Get().find(process);
  if (iter == g_dump_providers_map.Get().end()) {
    return;
  }
  base::trace_event::MemoryDumpManager::GetInstance()
      ->UnregisterAndDeleteDumpProviderSoon(std::move(iter->second));
  g_dump_providers_map.Get().erase(iter);
}

ProcessMetricsMemoryDumpProvider::ProcessMetricsMemoryDumpProvider(
    base::ProcessId process)
    : process_(process),
      process_metrics_(CreateProcessMetrics(process)),
      is_rss_peak_resettable_(true) {}

ProcessMetricsMemoryDumpProvider::~ProcessMetricsMemoryDumpProvider() {}

// Called at trace dump point time. Creates a snapshot of the memory maps for
// the current process.
bool ProcessMetricsMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  bool res = DumpProcessTotals(args, pmd);

#if defined(OS_LINUX) || defined(OS_ANDROID)
  if (args.level_of_detail ==
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED)
    res &= DumpProcessMemoryMaps(args, pmd);
#endif
  return res;
}

bool ProcessMetricsMemoryDumpProvider::DumpProcessTotals(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  const uint64_t rss_bytes = rss_bytes_for_testing
                                 ? rss_bytes_for_testing
                                 : process_metrics_->GetWorkingSetSize();

  // rss_bytes will be 0 if the process ended while dumping.
  if (!rss_bytes)
    return false;

  uint64_t peak_rss_bytes = 0;

#if !defined(OS_IOS)
  peak_rss_bytes = process_metrics_->GetPeakWorkingSetSize();
#if defined(OS_LINUX) || defined(OS_ANDROID)
  if (is_rss_peak_resettable_) {
    std::string clear_refs_file =
        "/proc/" +
        (process_ == base::kNullProcessId ? "self"
                                          : base::IntToString(process_)) +
        "/clear_refs";
    int clear_refs_fd = open(clear_refs_file.c_str(), O_WRONLY);
    if (clear_refs_fd > 0 &&
        base::WriteFileDescriptor(clear_refs_fd, kClearPeakRssCommand,
                                  sizeof(kClearPeakRssCommand))) {
      pmd->process_totals()->set_is_peak_rss_resetable(true);
    } else {
      is_rss_peak_resettable_ = false;
    }
    close(clear_refs_fd);
  }
#elif defined(MACOSX)
  size_t private_bytes;
  bool res = process_metrics_->GetMemoryBytes(&private_bytes,
                                              nullptr /* shared_bytes */);
  if (res)
    pmd->process_totals()->SetExtraFieldInBytes("private_bytes", private_bytes);
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)
#endif  // !defined(OS_IOS)

  pmd->process_totals()->set_resident_set_bytes(rss_bytes);
  pmd->set_has_process_totals();
  pmd->process_totals()->set_peak_resident_set_bytes(peak_rss_bytes);

  // Returns true even if other metrics failed, since rss is reported.
  return true;
}

}  // namespace tracing
