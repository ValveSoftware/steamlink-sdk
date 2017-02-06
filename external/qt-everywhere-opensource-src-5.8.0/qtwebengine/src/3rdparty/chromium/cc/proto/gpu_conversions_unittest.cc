// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/proto/gpu_conversions.h"

#include <stddef.h>

#include "cc/proto/memory_allocation.pb.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(GpuProtoConversionTest,
     SerializeDeserializeMemoryAllocationPriorityCutoff) {
  for (size_t i = 0; i < gpu::MemoryAllocation::PriorityCutoff::CUTOFF_LAST;
       ++i) {
    gpu::MemoryAllocation::PriorityCutoff priority_cutoff =
        static_cast<gpu::MemoryAllocation::PriorityCutoff>(i);
    EXPECT_EQ(priority_cutoff,
              MemoryAllocationPriorityCutoffFromProto(
                  MemoryAllocationPriorityCutoffToProto(priority_cutoff)));
  }
}

}  // namespace
}  // namespace cc
