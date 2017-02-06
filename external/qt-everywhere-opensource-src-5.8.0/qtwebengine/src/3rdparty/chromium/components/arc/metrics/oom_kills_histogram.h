// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_METRICS_OOM_KILLS_HISTOGRAM_H_
#define COMPONENTS_ARC_METRICS_OOM_KILLS_HISTOGRAM_H_

#include "base/metrics/histogram.h"

namespace arc {

const int kMaxOomMemoryKillTimeDeltaSecs = 30;

}  // namespace arc

// Use this macro to report elapsed time since last OOM kill event.
// Must be a macro as the underlying HISTOGRAM macro creates static variables.
#define UMA_HISTOGRAM_OOM_KILL_TIME_INTERVAL(name, sample)  \
  UMA_HISTOGRAM_CUSTOM_TIMES(                               \
      name, sample, base::TimeDelta::FromMilliseconds(1),   \
      base::TimeDelta::FromSeconds(::arc::kMaxOomMemoryKillTimeDeltaSecs), 50)

#endif  // COMPONENTS_ARC_METRICS_OOM_KILLS_HISTOGRAM_H_
