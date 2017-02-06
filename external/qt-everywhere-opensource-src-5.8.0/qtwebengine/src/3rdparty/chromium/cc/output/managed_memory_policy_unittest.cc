// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/managed_memory_policy.h"

#include <stddef.h>

#include "cc/proto/managed_memory_policy.pb.h"
#include "cc/proto/memory_allocation.pb.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

void VerifySerializeAndDeserialize(
    size_t bytes_limit_when_visible,
    gpu::MemoryAllocation::PriorityCutoff priority_cutoff_when_visible,
    size_t num_resources_limit) {
  ManagedMemoryPolicy policy1(bytes_limit_when_visible,
                              gpu::MemoryAllocation::CUTOFF_ALLOW_REQUIRED_ONLY,
                              num_resources_limit);
  proto::ManagedMemoryPolicy proto;
  policy1.ToProtobuf(&proto);
  ManagedMemoryPolicy policy2(1);
  policy2.FromProtobuf(proto);
  EXPECT_EQ(policy1, policy2);
}

TEST(ManagedMemoryPolicyTest, SerializeDeserialize) {
  VerifySerializeAndDeserialize(
      42, gpu::MemoryAllocation::CUTOFF_ALLOW_REQUIRED_ONLY, 24);
  VerifySerializeAndDeserialize(0, gpu::MemoryAllocation::CUTOFF_ALLOW_NOTHING,
                                1);
  VerifySerializeAndDeserialize(
      1, gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE, 0);
  VerifySerializeAndDeserialize(
      1024, gpu::MemoryAllocation::CUTOFF_ALLOW_EVERYTHING, 4096);
}

}  // namespace
}  // namespace cc
