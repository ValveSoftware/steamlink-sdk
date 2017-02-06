// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_BASE_ENQUEUE_ORDER_H_
#define COMPONENTS_SCHEDULER_BASE_ENQUEUE_ORDER_H_

#include <stdint.h>

#include "base/synchronization/lock.h"

namespace scheduler {
namespace internal {

using EnqueueOrder = uint64_t;

class EnqueueOrderGenerator {
 public:
  EnqueueOrderGenerator();
  ~EnqueueOrderGenerator();

  EnqueueOrder GenerateNext();

 private:
  base::Lock lock_;
  EnqueueOrder enqueue_order_;
};

}  // namespace internal
}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_BASE_ENQUEUE_ORDER_H_
