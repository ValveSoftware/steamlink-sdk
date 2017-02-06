// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/proto/gpu_conversions.h"

#include "base/logging.h"
#include "cc/proto/memory_allocation.pb.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"

namespace cc {

proto::MemoryAllocation::PriorityCutoff MemoryAllocationPriorityCutoffToProto(
    const gpu::MemoryAllocation::PriorityCutoff& priority_cutoff) {
  switch (priority_cutoff) {
    case gpu::MemoryAllocation::PriorityCutoff::CUTOFF_ALLOW_NOTHING:
      return proto::MemoryAllocation_PriorityCutoff_ALLOW_NOTHING;
    case gpu::MemoryAllocation::PriorityCutoff::CUTOFF_ALLOW_REQUIRED_ONLY:
      return proto::MemoryAllocation_PriorityCutoff_ALLOW_REQUIRED_ONLY;
    case gpu::MemoryAllocation::PriorityCutoff::CUTOFF_ALLOW_NICE_TO_HAVE:
      return proto::MemoryAllocation_PriorityCutoff_ALLOW_NICE_TO_HAVE;
    case gpu::MemoryAllocation::PriorityCutoff::CUTOFF_ALLOW_EVERYTHING:
      return proto::MemoryAllocation_PriorityCutoff_ALLOW_EVERYTHING;
  }
  return proto::MemoryAllocation_PriorityCutoff_UNKNOWN;
}

gpu::MemoryAllocation::PriorityCutoff MemoryAllocationPriorityCutoffFromProto(
    const proto::MemoryAllocation::PriorityCutoff& priority_cutoff) {
  switch (priority_cutoff) {
    case proto::MemoryAllocation_PriorityCutoff_ALLOW_NOTHING:
      return gpu::MemoryAllocation::PriorityCutoff::CUTOFF_ALLOW_NOTHING;
    case proto::MemoryAllocation_PriorityCutoff_ALLOW_REQUIRED_ONLY:
      return gpu::MemoryAllocation::PriorityCutoff::CUTOFF_ALLOW_REQUIRED_ONLY;
    case proto::MemoryAllocation_PriorityCutoff_ALLOW_NICE_TO_HAVE:
      return gpu::MemoryAllocation::PriorityCutoff::CUTOFF_ALLOW_NICE_TO_HAVE;
    case proto::MemoryAllocation_PriorityCutoff_ALLOW_EVERYTHING:
      return gpu::MemoryAllocation::PriorityCutoff::CUTOFF_ALLOW_EVERYTHING;
    case proto::MemoryAllocation_PriorityCutoff_UNKNOWN:
      NOTREACHED() << "Received MemoryAllocation::PriorityCutoff_UNKNOWN";
      return gpu::MemoryAllocation::PriorityCutoff::CUTOFF_LAST;
  }
  return gpu::MemoryAllocation::PriorityCutoff::CUTOFF_ALLOW_NOTHING;
}

}  // namespace cc
