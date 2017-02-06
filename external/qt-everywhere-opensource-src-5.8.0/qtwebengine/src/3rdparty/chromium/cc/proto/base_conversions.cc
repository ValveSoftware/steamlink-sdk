// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/proto/base_conversions.h"

namespace base {
class TimeDelta;
}

namespace cc {

int64_t TimeTicksToProto(base::TimeTicks ticks) {
  base::TimeDelta diff = ticks - base::TimeTicks::UnixEpoch();
  return diff.InMicroseconds();
}

CC_EXPORT base::TimeTicks ProtoToTimeTicks(int64_t ticks) {
  base::TimeDelta diff = base::TimeDelta::FromMicroseconds(ticks);
  return base::TimeTicks::UnixEpoch() + diff;
}

}  // namespace cc
