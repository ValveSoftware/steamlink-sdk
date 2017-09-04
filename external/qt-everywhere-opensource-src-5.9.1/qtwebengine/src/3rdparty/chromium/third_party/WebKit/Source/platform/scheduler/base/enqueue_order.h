// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_ENQUEUE_ORDER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_ENQUEUE_ORDER_H_

#include <stdint.h>

#include "base/synchronization/lock.h"

namespace blink {
namespace scheduler {
namespace internal {

using EnqueueOrder = uint64_t;

// TODO(scheduler-dev): Remove explicit casts when c++17 comes.
enum class EnqueueOrderValues : EnqueueOrder {
  // Invalid EnqueueOrder.
  NONE = 0,

  // Earliest possible EnqueueOrder, to be used for fence blocking.
  BLOCKING_FENCE = 1,
  FIRST = 2,
};

// A 64bit integer used to provide ordering of tasks. NOTE The scheduler assumes
// these values will not overflow.
class EnqueueOrderGenerator {
 public:
  EnqueueOrderGenerator();
  ~EnqueueOrderGenerator();

  // Returns a monotonically increasing integer, starting from one. Can be
  // called from any thread.
  EnqueueOrder GenerateNext();

  static bool IsValidEnqueueOrder(EnqueueOrder enqueue_order) {
    return enqueue_order != 0ull;
  }

 private:
  base::Lock lock_;
  EnqueueOrder enqueue_order_;
};

}  // namespace internal
}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_ENQUEUE_ORDER_H_
