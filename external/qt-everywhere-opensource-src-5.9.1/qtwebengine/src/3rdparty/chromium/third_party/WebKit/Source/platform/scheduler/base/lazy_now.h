// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_LAZY_NOW_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_LAZY_NOW_H_

#include "base/time/time.h"
#include "public/platform/WebCommon.h"

namespace base {
class TickClock;
}

namespace blink {
namespace scheduler {

// Now() is somewhat expensive so it makes sense not to call Now() unless we
// really need to.
class BLINK_PLATFORM_EXPORT LazyNow {
 public:
  explicit LazyNow(base::TimeTicks now) : tick_clock_(nullptr), now_(now) {
    DCHECK(!now.is_null());
  }

  explicit LazyNow(base::TickClock* tick_clock) : tick_clock_(tick_clock) {}

  base::TimeTicks Now();

 private:
  base::TickClock* tick_clock_;  // NOT OWNED
  base::TimeTicks now_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_LAZY_NOW_H_
