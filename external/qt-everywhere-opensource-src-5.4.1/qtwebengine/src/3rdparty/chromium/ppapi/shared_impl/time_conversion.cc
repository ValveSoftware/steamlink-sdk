// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/time_conversion.h"

namespace ppapi {

namespace {

// Since WebKit doesn't use ticks for event times, we have to compute what
// the time ticks would be assuming the wall clock time doesn't change.
//
// This should only be used for WebKit times which we can't change the
// definition of.
double GetTimeToTimeTicksDeltaInSeconds() {
  static double time_to_ticks_delta_seconds = 0.0;
  if (time_to_ticks_delta_seconds == 0.0) {
    double wall_clock = TimeToPPTime(base::Time::Now());
    double ticks = TimeTicksToPPTimeTicks(base::TimeTicks::Now());
    time_to_ticks_delta_seconds = ticks - wall_clock;
  }
  return time_to_ticks_delta_seconds;
}

}  // namespace

PP_Time TimeToPPTime(base::Time t) { return t.ToDoubleT(); }

base::Time PPTimeToTime(PP_Time t) {
  // The time code handles exact "0" values as special, and produces
  // a "null" Time object. But calling code would expect t==0 to represent the
  // epoch (according to the description of PP_Time). Hence we just return the
  // epoch in this case.
  if (t == 0.0)
    return base::Time::UnixEpoch();
  return base::Time::FromDoubleT(t);
}

PP_TimeTicks TimeTicksToPPTimeTicks(base::TimeTicks t) {
  return static_cast<double>(t.ToInternalValue()) /
         base::Time::kMicrosecondsPerSecond;
}

PP_TimeTicks EventTimeToPPTimeTicks(double event_time) {
  return event_time + GetTimeToTimeTicksDeltaInSeconds();
}

double PPTimeTicksToEventTime(PP_TimeTicks t) {
  return t - GetTimeToTimeTicksDeltaInSeconds();
}

double PPGetLocalTimeZoneOffset(const base::Time& time) {
  // Explode it to local time and then unexplode it as if it were UTC. Also
  // explode it to UTC and unexplode it (this avoids mismatching rounding or
  // lack thereof). The time zone offset is their difference.
  base::Time::Exploded exploded = {0};
  base::Time::Exploded utc_exploded = {0};
  time.LocalExplode(&exploded);
  time.UTCExplode(&utc_exploded);
  if (exploded.HasValidValues() && utc_exploded.HasValidValues()) {
    base::Time adj_time = base::Time::FromUTCExploded(exploded);
    base::Time cur = base::Time::FromUTCExploded(utc_exploded);
    return (adj_time - cur).InSecondsF();
  }
  return 0.0;
}

}  // namespace ppapi
