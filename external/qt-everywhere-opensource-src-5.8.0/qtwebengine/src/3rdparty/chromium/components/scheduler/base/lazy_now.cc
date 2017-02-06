// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/lazy_now.h"

#include "base/time/tick_clock.h"
#include "components/scheduler/base/task_queue_manager.h"

namespace scheduler {
base::TimeTicks LazyNow::Now() {
  if (now_.is_null())
    now_ = tick_clock_->NowTicks();
  return now_;
}

}  // namespace scheduler
