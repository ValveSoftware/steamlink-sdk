// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEMORY_COORDINATOR_COMMON_MEMORY_COORDINATOR_CLIENT_H_
#define MEMORY_COORDINATOR_COMMON_MEMORY_COORDINATOR_CLIENT_H_

#include "components/memory_coordinator/public/interfaces/child_memory_coordinator.mojom.h"

namespace memory_coordinator {

// This is an interface for components which can respond to memory status
// changes.
class MemoryCoordinatorClient {
 public:
  virtual ~MemoryCoordinatorClient() {}

  // Called when memory state has changed.
  virtual void OnMemoryStateChange(mojom::MemoryState state) = 0;
};

}  // namespace memory_coordinator

#endif  // MEMORY_COORDINATOR_COMMON_MEMORY_COORDINATOR_CLIENT_H_
