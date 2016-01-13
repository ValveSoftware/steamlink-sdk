// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_POWER_PROFILER_POWER_EVENT_H_
#define CONTENT_BROWSER_POWER_PROFILER_POWER_EVENT_H_

#include "base/time/time.h"

namespace content {

struct PowerEvent {
  enum Type {
    // Total power of SoC. including CPU, GT and others on the chip,
    // modules which aren't part of the SoC such as the screen are not included.
    SOC_PACKAGE,

    // Whole device power.
    DEVICE,

    // Keep this at the end.
    ID_COUNT
  };

  Type type;

  base::TimeTicks time;  // Time that power data was read.

  // Power value between last event and this one, in watts.
  // E.g, event1 {t1, v1}; event2 {t2, v2}; event3 {t3, v3}.
  // Suppose event1 is the first event the observer received, then event2,
  // event3. Then v2 is the average power from t1 to t2, v3 is the average
  // power from t2 to t3. v1 should be ignored since event1 only means the
  // start point of power profiling.
  double value;
};

extern const char* kPowerTypeNames[];

}  // namespace content

#endif  // CONTENT_BROWSER_POWER_PROFILER_POWER_EVENT_H_
