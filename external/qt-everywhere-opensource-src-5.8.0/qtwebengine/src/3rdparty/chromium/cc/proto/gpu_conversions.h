// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PROTO_GPU_CONVERSIONS_H_
#define CC_PROTO_GPU_CONVERSIONS_H_

#include "cc/base/cc_export.h"
#include "cc/proto/memory_allocation.pb.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"

namespace cc {

// TODO(dtrainor): Move these to a class and make them static
// (crbug.com/548432).
CC_EXPORT proto::MemoryAllocation::PriorityCutoff
MemoryAllocationPriorityCutoffToProto(
    const gpu::MemoryAllocation::PriorityCutoff& priority_cutoff);
CC_EXPORT gpu::MemoryAllocation::PriorityCutoff
MemoryAllocationPriorityCutoffFromProto(
    const proto::MemoryAllocation::PriorityCutoff& proto);

}  // namespace cc

#endif  // CC_PROTO_GPU_CONVERSIONS_H_
