// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SCOPED_HISTOGRAM_TIMER_H_
#define MEDIA_BASE_SCOPED_HISTOGRAM_TIMER_H_

#include "base/metrics/histogram.h"
#include "base/time/time.h"

// Scoped class which logs its time on this earth as a UMA statistic.  Must be
// a #define macro since UMA macros prevent variables as names.  The nested
// macro is necessary to expand __COUNTER__ to an actual value.
#define SCOPED_UMA_HISTOGRAM_TIMER(name) \
  SCOPED_UMA_HISTOGRAM_TIMER_EXPANDER(name, __COUNTER__)

#define SCOPED_UMA_HISTOGRAM_TIMER_EXPANDER(name, key) \
  SCOPED_UMA_HISTOGRAM_TIMER_UNIQUE(name, key)

#define SCOPED_UMA_HISTOGRAM_TIMER_UNIQUE(name, key) \
  class ScopedHistogramTimer##key { \
   public: \
    ScopedHistogramTimer##key() : constructed_(base::TimeTicks::Now()) {} \
    ~ScopedHistogramTimer##key() { \
      base::TimeDelta elapsed = base::TimeTicks::Now() - constructed_; \
      UMA_HISTOGRAM_TIMES(name, elapsed); \
    } \
   private: \
    base::TimeTicks constructed_; \
  } scoped_histogram_timer_##key

#endif  // MEDIA_BASE_SCOPED_HISTOGRAM_TIMER_H_
