// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_SHARED_MEMORY_LIMITS_H_
#define GPU_COMMAND_BUFFER_CLIENT_SHARED_MEMORY_LIMITS_H_

#include <stddef.h>

namespace gpu {

struct SharedMemoryLimits {
  int32_t command_buffer_size = 1024 * 1024;
  uint32_t start_transfer_buffer_size = 1 * 1024 * 1024;
  uint32_t min_transfer_buffer_size = 1 * 256 * 1024;
  uint32_t max_transfer_buffer_size = 16 * 1024 * 1024;

  static constexpr uint32_t kNoLimit = 0;
  uint32_t mapped_memory_reclaim_limit = kNoLimit;

  // These are limits for contexts only used for creating textures, mailboxing
  // them and dealing with synchronization.
  static SharedMemoryLimits ForMailboxContext() {
    SharedMemoryLimits limits;
    limits.command_buffer_size = 64 * 1024;
    limits.start_transfer_buffer_size = 64 * 1024;
    limits.min_transfer_buffer_size = 64 * 1024;
    return limits;
  }
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_SHARED_MEMORY_LIMITS_H_
