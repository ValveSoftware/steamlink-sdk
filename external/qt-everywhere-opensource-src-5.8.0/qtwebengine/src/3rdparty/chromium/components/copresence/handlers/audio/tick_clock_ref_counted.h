// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_HANDLERS_AUDIO_TICK_CLOCK_REF_COUNTED_H_
#define COMPONENTS_COPRESENCE_HANDLERS_AUDIO_TICK_CLOCK_REF_COUNTED_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace base {
class TickClock;
class TimeTicks;
}

namespace copresence {

class TickClockRefCounted
    : public base::RefCountedThreadSafe<TickClockRefCounted> {
 public:
  explicit TickClockRefCounted(std::unique_ptr<base::TickClock> clock);

  // Takes ownership of the clock.
  explicit TickClockRefCounted(base::TickClock* clock);

  base::TimeTicks NowTicks() const;

 private:
  friend class base::RefCountedThreadSafe<TickClockRefCounted>;
  virtual ~TickClockRefCounted();

  std::unique_ptr<base::TickClock> clock_;

  DISALLOW_COPY_AND_ASSIGN(TickClockRefCounted);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_HANDLERS_AUDIO_TICK_CLOCK_REF_COUNTED_H_
