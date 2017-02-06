// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/enqueue_order.h"

namespace scheduler {
namespace internal {

EnqueueOrderGenerator::EnqueueOrderGenerator() : enqueue_order_(0) {}

EnqueueOrderGenerator::~EnqueueOrderGenerator() {}

EnqueueOrder EnqueueOrderGenerator::GenerateNext() {
  base::AutoLock lock(lock_);
  return enqueue_order_++;
}

}  // namespace internal
}  // namespace scheduler
