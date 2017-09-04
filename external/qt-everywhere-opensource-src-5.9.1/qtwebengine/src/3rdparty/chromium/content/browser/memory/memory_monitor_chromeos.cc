// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/memory_monitor_chromeos.h"

#include "base/memory/ptr_util.h"
#include "base/process/process_metrics.h"

namespace content {

namespace {

// The number of bits to shift to convert KiB to MiB.
const int kShiftKiBtoMiB = 10;

}  // namespace

MemoryMonitorChromeOS::MemoryMonitorChromeOS(MemoryMonitorDelegate* delegate)
    : delegate_(delegate) {}

MemoryMonitorChromeOS::~MemoryMonitorChromeOS() {}

int MemoryMonitorChromeOS::GetFreeMemoryUntilCriticalMB() {
  base::SystemMemoryInfoKB mem_info = {};
  delegate_->GetSystemMemoryInfo(&mem_info);

  // The available memory consists of "real" and virtual (z)ram memory.
  // Since swappable memory uses a non pre-deterministic compression and
  // the compression creates its own "dynamic" in the system, it gets
  // de-emphasized by the |kSwapWeight| factor.
  const int kSwapWeight = 4;

  // The kernel internally uses 50MB.
  const int kMinFileMemory = 50 * 1024;

  // Most file memory can be easily reclaimed.
  int file_memory = mem_info.active_file + mem_info.inactive_file;
  // unless it is dirty or it's a minimal portion which is required.
  file_memory -= mem_info.dirty + kMinFileMemory;

  // Available memory is the sum of free, swap and easy reclaimable memory.
  return (mem_info.free + mem_info.swap_free / kSwapWeight + file_memory) >>
         kShiftKiBtoMiB;
}

// static
std::unique_ptr<MemoryMonitorChromeOS> MemoryMonitorChromeOS::Create(
    MemoryMonitorDelegate* delegate) {
  return base::MakeUnique<MemoryMonitorChromeOS>(delegate);
}

// Implementation of factory function defined in memory_monitor.h.
std::unique_ptr<MemoryMonitor> CreateMemoryMonitor() {
  return MemoryMonitorChromeOS::Create(MemoryMonitorDelegate::GetInstance());
}

}  // namespace content
