// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/sync_control_vsync_provider.h"

#include <math.h>

#include "base/logging.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"

#if defined(OS_LINUX) || defined(OS_WIN)
// These constants define a reasonable range for a calculated refresh interval.
// Calculating refreshes out of this range will be considered a fatal error.
const int64_t kMinVsyncIntervalUs = base::Time::kMicrosecondsPerSecond / 400;
const int64_t kMaxVsyncIntervalUs = base::Time::kMicrosecondsPerSecond / 10;

// How much noise we'll tolerate between successive computed intervals before
// we think the latest computed interval is invalid (noisey due to
// monitor configuration change, moving a window between monitors, etc.).
const double kRelativeIntervalDifferenceThreshold = 0.05;
#endif  // defined(OS_LINUX) || defined(OS_WIN)

namespace gl {

SyncControlVSyncProvider::SyncControlVSyncProvider() : gfx::VSyncProvider() {
#if defined(OS_LINUX) || defined(OS_WIN)
  // On platforms where we can't get an accurate reading on the refresh
  // rate we fall back to the assumption that we're displaying 60 frames
  // per second.
  last_good_interval_ = base::TimeDelta::FromSeconds(1) / 60;
#endif  // defined(OS_LINUX) || defined(OS_WIN)
}

SyncControlVSyncProvider::~SyncControlVSyncProvider() {}

void SyncControlVSyncProvider::GetVSyncParameters(
    const UpdateVSyncCallback& callback) {
  TRACE_EVENT0("gpu", "SyncControlVSyncProvider::GetVSyncParameters");
#if defined(OS_LINUX) || defined(OS_WIN)
  base::TimeTicks timebase;

  int64_t system_time;
  int64_t media_stream_counter;
  int64_t swap_buffer_counter;
  if (!GetSyncValues(&system_time, &media_stream_counter, &swap_buffer_counter))
    return;

  if (media_stream_counter == last_media_stream_counter_) {
    // SyncValues haven't updated, there is no reason to invoke the callback.
    return;
  }

  // Perform platform specific adjustment of |system_time| and
  // |media_stream_counter|.
  if (!AdjustSyncValues(&system_time, &media_stream_counter))
    return;

  timebase = base::TimeTicks::FromInternalValue(system_time);

  // Only need the previous calculated interval for our filtering.
  while (last_computed_intervals_.size() > 1)
    last_computed_intervals_.pop();

  base::TimeDelta timebase_diff;
  int64_t counter_diff = 0;

  int32_t numerator, denominator;
  if (GetMscRate(&numerator, &denominator) && numerator) {
    timebase_diff = base::TimeDelta::FromSeconds(denominator);
    counter_diff = numerator;
  } else if (!last_timebase_.is_null()) {
    timebase_diff = timebase - last_timebase_;
    counter_diff = media_stream_counter - last_media_stream_counter_;
  }

  if (counter_diff > 0 && timebase_diff > base::TimeDelta()) {
    last_computed_intervals_.push(timebase_diff / counter_diff);

    if (last_computed_intervals_.size() == 2) {
      const base::TimeDelta& old_interval = last_computed_intervals_.front();
      const base::TimeDelta& new_interval = last_computed_intervals_.back();

      double relative_change = fabs(old_interval.InMillisecondsF() -
                                    new_interval.InMillisecondsF()) /
                               new_interval.InMillisecondsF();
      if (relative_change < kRelativeIntervalDifferenceThreshold) {
        if (new_interval.InMicroseconds() < kMinVsyncIntervalUs ||
            new_interval.InMicroseconds() > kMaxVsyncIntervalUs) {
#if defined(OS_WIN) || defined(USE_ASH)
          // On ash platforms (ChromeOS essentially), the real refresh interval
          // is queried from XRandR, regardless of the value calculated here,
          // and this value is overriden by ui::CompositorVSyncManager.  The log
          // should not be fatal in this case. Reconsider all this when XRandR
          // support is added to non-ash platforms.
          // http://crbug.com/340851
          // On Windows |system_time| is based on QPC and it seems it may
          // produce invalid value after a suspend/resume cycle.
          // http://crbug.com/656469
          LOG(ERROR)
#else
          LOG(FATAL)
#endif  // OS_WIN || USE_ASH
              << "Calculated bogus refresh interval="
              << new_interval.InMicroseconds()
              << " us, old_interval=" << old_interval.InMicroseconds()
              << " us, last_timebase_=" << last_timebase_.ToInternalValue()
              << " us, timebase=" << timebase.ToInternalValue()
              << " us, timebase_diff=" << timebase_diff.ToInternalValue()
              << " us, last_timebase_diff_="
              << last_timebase_diff_.ToInternalValue()
              << " us, last_media_stream_counter_="
              << last_media_stream_counter_
              << ", media_stream_counter=" << media_stream_counter
              << ", counter_diff=" << counter_diff
              << ", last_counter_diff_=" << last_counter_diff_;
        } else {
          last_good_interval_ = new_interval;
        }
      }
    }

    last_timebase_diff_ = timebase_diff;
    last_counter_diff_ = counter_diff;
  }

  last_timebase_ = timebase;
  last_media_stream_counter_ = media_stream_counter;
  callback.Run(timebase, last_good_interval_);
#endif  // defined(OS_LINUX) || defined(OS_WIN)
}

#if defined(OS_LINUX)
bool SyncControlVSyncProvider::AdjustSyncValues(int64_t* system_time,
                                                int64_t* media_stream_counter) {
  // Both Intel and Mali drivers will return TRUE for GetSyncValues
  // but a value of 0 for MSC if they cannot access the CRTC data structure
  // associated with the surface. crbug.com/231945
  bool prev_invalid_msc = invalid_msc_;
  invalid_msc_ = (*media_stream_counter == 0);
  if (invalid_msc_) {
    LOG_IF(ERROR, !prev_invalid_msc)
        << "glXGetSyncValuesOML "
           "should not return TRUE with a media stream counter of 0.";
    return false;
  }

  // The actual clock used for the system time returned by glXGetSyncValuesOML
  // is unspecified. In practice, the clock used is likely to be either
  // CLOCK_REALTIME or CLOCK_MONOTONIC, so we compare the returned time to the
  // current time according to both clocks, and assume that the returned time
  // was produced by the clock whose current time is closest to it, subject
  // to the restriction that the returned time must not be in the future
  // (since it is the time of a vblank that has already occurred).
  struct timespec real_time;
  struct timespec monotonic_time;
  clock_gettime(CLOCK_REALTIME, &real_time);
  clock_gettime(CLOCK_MONOTONIC, &monotonic_time);

  int64_t real_time_in_microseconds =
      real_time.tv_sec * base::Time::kMicrosecondsPerSecond +
      real_time.tv_nsec / base::Time::kNanosecondsPerMicrosecond;
  int64_t monotonic_time_in_microseconds =
      monotonic_time.tv_sec * base::Time::kMicrosecondsPerSecond +
      monotonic_time.tv_nsec / base::Time::kNanosecondsPerMicrosecond;

  // We need the time according to CLOCK_MONOTONIC, so if we've been given
  // a time from CLOCK_REALTIME, we need to convert.
  bool time_conversion_needed =
      llabs(*system_time - real_time_in_microseconds) <
      llabs(*system_time - monotonic_time_in_microseconds);

  if (time_conversion_needed)
    *system_time += monotonic_time_in_microseconds - real_time_in_microseconds;

  // Return if |*system_time| is more than 1 frames in the future.
  int64_t interval_in_microseconds = last_good_interval_.InMicroseconds();
  if (*system_time > monotonic_time_in_microseconds + interval_in_microseconds)
    return false;

  // If |system_time| is slightly in the future, adjust it to the previous
  // frame and use the last frame counter to prevent issues in the callback.
  if (*system_time > monotonic_time_in_microseconds) {
    *system_time -= interval_in_microseconds;
    (*media_stream_counter)--;
  }
  if (monotonic_time_in_microseconds - *system_time >
      base::Time::kMicrosecondsPerSecond)
    return false;

  return true;
}
#endif  // defined(OS_LINUX)

#if defined(OS_WIN)
bool SyncControlVSyncProvider::AdjustSyncValues(int64_t* system_time,
                                                int64_t* media_stream_counter) {
  // Zero MSC is returned once when switching between windowed and full screen
  // modes.
  if (*media_stream_counter == 0)
    return false;

  // The actual clock used for the system time returned by glXGetSyncValuesEGL
  // is unspecified. In practice, the clock comes from QueryPerformanceCounter.
  LARGE_INTEGER perf_counter_now = {};
  ::QueryPerformanceCounter(&perf_counter_now);
  int64_t qpc_now =
      base::TimeDelta::FromQPCValue(perf_counter_now.QuadPart).InMicroseconds();

  // Return if |system_time| is more than 1 frames in the future.
  int64_t interval_in_microseconds = last_good_interval_.InMicroseconds();
  if (*system_time > qpc_now + interval_in_microseconds)
    return false;

  // If |system_time| is slightly in the future, adjust it to the previous
  // frame and use the last frame counter to prevent issues in the callback.
  if (*system_time > qpc_now) {
    *system_time -= interval_in_microseconds;
    (*media_stream_counter)--;
  }
  if (qpc_now - *system_time > base::Time::kMicrosecondsPerSecond)
    return false;

  return true;
}
#endif  // defined(OS_WIN)

}  // namespace gl
