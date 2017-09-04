// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/test_time_source.h"

namespace blink {
namespace scheduler {

TestTimeSource::TestTimeSource(base::SimpleTestTickClock* time_source)
    : time_source_(time_source) {
  DCHECK(time_source_);
}

TestTimeSource::~TestTimeSource() {}

base::TimeTicks TestTimeSource::NowTicks() {
  return time_source_->NowTicks();
}

}  // namespace scheduler
}  // namespace blink
