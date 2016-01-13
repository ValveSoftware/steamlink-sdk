// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_COMMON_TIME_HELPER_H_
#define MOJO_COMMON_TIME_HELPER_H_

#include "base/time/time.h"
#include "mojo/common/mojo_common_export.h"

namespace base {
class TickClock;
}

namespace mojo {
namespace common {
namespace test {

// Sets the TickClock used for getting TimeTicks::Now(). This is currently used
// by both HandleWatcher and MessagePumpMojo.
MOJO_COMMON_EXPORT void SetTickClockForTest(base::TickClock* clock);

}  // namespace test

namespace internal {

// Returns now. Used internally; generally not useful.
MOJO_COMMON_EXPORT base::TimeTicks NowTicks();

}  // namespace internal
}  // namespace common
}  // namespace mojo

#endif  // MOJO_COMMON_TIME_HELPER_H_
