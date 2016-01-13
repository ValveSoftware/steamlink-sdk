// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CLOCK_H_
#define MEDIA_BASE_CLOCK_H_

#include "base/basictypes.h"
#include "base/time/time.h"
#include "media/base/media_export.h"

namespace base {
class TickClock;
}  // namespace base

namespace media {

// A clock represents a single source of time to allow audio and video streams
// to synchronize with each other.  Clock essentially tracks the media time with
// respect to some other source of time, whether that may be the monotonic
// system clock or updates via SetTime(). Clock uses linear interpolation to
// calculate the current media time since the last time SetTime() was called.
//
// Clocks start off paused with a playback rate of 1.0f and a media time of 0.
//
// Clock is not thread-safe and must be externally locked.
//
// TODO(scherkus): Clock will some day be responsible for executing callbacks
// given a media time.  This will be used primarily by video renderers.  For now
// we'll keep using a poll-and-sleep solution.
//
// TODO(miu): Rename media::Clock to avoid confusion (and tripping up the media
// PRESUBMIT script on future changes).
class MEDIA_EXPORT Clock {
 public:
  explicit Clock(base::TickClock* clock);
  ~Clock();

  // Returns true if the clock is running.
  bool IsPlaying() const;

  // Starts the clock and returns the current media time, which will increase
  // with respect to the current playback rate.
  base::TimeDelta Play();

  // Stops the clock and returns the current media time, which will remain
  // constant until Play() is called.
  base::TimeDelta Pause();

  // Sets a new playback rate.  The rate at which the media time will increase
  // will now change.
  void SetPlaybackRate(float playback_rate);

  // Forcefully sets the media time to |current_time|. The second parameter is
  // the |max_time| that the clock should progress after a call to Play(). This
  // value is often the time of the end of the last frame buffered and decoded.
  //
  // These values are clamped to the duration of the video, which is initially
  // set to 0 (before SetDuration() is called).
  void SetTime(base::TimeDelta current_time, base::TimeDelta max_time);

  // Sets the |max_time| to be returned by a call to Elapsed().
  void SetMaxTime(base::TimeDelta max_time);

  // Returns the current elapsed media time. Returns 0 if SetDuration() has
  // never been called.
  base::TimeDelta Elapsed();

  // Sets the duration of the video. Clock expects the duration will be set
  // exactly once.
  void SetDuration(base::TimeDelta duration);

  // Returns the duration of the clock, or 0 if not set.
  base::TimeDelta Duration() const;

 private:
  // Updates the reference points based on the current calculated time.
  void UpdateReferencePoints();

  // Updates the reference points based on the given |current_time|.
  void UpdateReferencePoints(base::TimeDelta current_time);

  // Returns the time elapsed based on the current reference points, ignoring
  // the |max_time_| cap.
  base::TimeDelta EstimatedElapsedTime();

  // Translates |time| into the current media time, based on the perspective of
  // the monotonically-increasing system clock.
  base::TimeDelta ElapsedViaProvidedTime(const base::TimeTicks& time) const;

  base::TimeDelta ClampToValidTimeRange(base::TimeDelta time) const;

  base::TickClock* const clock_;

  // Whether the clock is running.
  bool playing_;

  // Whether the clock is stalled because it has reached the |max_time_|
  // allowed.
  bool underflow_;

  // The monotonic system clock time when this Clock last started playing or had
  // its time set via SetTime().
  base::TimeTicks reference_;

  // Current accumulated amount of media time.  The remaining portion must be
  // calculated by comparing the system time to the reference time.
  base::TimeDelta media_time_;

  // Current playback rate.
  float playback_rate_;

  // The maximum time that can be returned by calls to Elapsed().
  base::TimeDelta max_time_;

  // Duration of the media.
  base::TimeDelta duration_;

  DISALLOW_COPY_AND_ASSIGN(Clock);
};

}  // namespace media

#endif  // MEDIA_BASE_CLOCK_H_
