// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_WATCH_TIME_REPORTER_H_
#define MEDIA_BLINK_WATCH_TIME_REPORTER_H_

#include "base/callback.h"
#include "base/power_monitor/power_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/base/media_log.h"
#include "media/base/timestamp_constants.h"
#include "media/blink/media_blink_export.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// Class for monitoring and reporting watch time in response to various state
// changes during the playback of media. We record metrics for audio only
// playbacks as well as audio+video playbacks of sufficient size.
//
// Watch time for our purposes is defined as the amount of elapsed media time
// for audio only or audio+video media. A minimum of 7 seconds of unmuted,
// foreground (where this is video) media must be watched to start watch time
// monitoring. Watch time is checked every 5 seconds from then on and reported
// to multiple buckets: All, MSE, SRC, EME, AC, and battery.
//
// Any one of paused, hidden (where this is video), or muted is sufficient to
// stop watch time metric reports. Each of these has a hysteresis where if the
// state change is undone within 5 seconds, the watch time will be counted as
// uninterrupted.
//
// Power events (on/off battery power) have a similar hysteresis, but unlike
// the aforementioned properties, will not stop metric collection.
//
// Each seek event will result in a new watch time metric being started and the
// old metric finalized as accurately as possible.
class MEDIA_BLINK_EXPORT WatchTimeReporter : base::PowerObserver {
 public:
  using GetMediaTimeCB = base::Callback<base::TimeDelta(void)>;

  // Constructor for the reporter; all requested metadata should be fully known
  // before attempting construction as incorrect values will result in the wrong
  // watch time metrics being reported.
  //
  // |media_log| is used to continuously log the watch time values for eventual
  // recording to a histogram upon finalization.
  //
  // |initial_video_size| required to ensure that the video track has sufficient
  // size for watch time reporting.
  //
  // |get_media_time_cb| must return the current playback time in terms of media
  // time, not wall clock time! Using media time instead of wall clock time
  // allows us to avoid a whole class of issues around clock changes during
  // suspend and resume.
  // TODO(dalecurtis): Should we only report when rate == 1.0? Should we scale
  // the elapsed media time instead?
  WatchTimeReporter(bool has_audio,
                    bool has_video,
                    bool is_mse,
                    bool is_encrypted,
                    scoped_refptr<MediaLog> media_log,
                    const gfx::Size& initial_video_size,
                    const GetMediaTimeCB& get_media_time_cb);
  ~WatchTimeReporter() override;

  // These methods are used to ensure that watch time is only reported for
  // media that is actually playing. They should be called whenever the media
  // starts or stops playing for any reason.
  void OnPlaying();
  void OnPaused();

  // This will immediately finalize any outstanding watch time reports and stop
  // the reporting timer. Clients should call OnPlaying() upon seek completion
  // to restart the reporting timer.
  void OnSeeking();

  // This method is used to ensure that watch time is only reported for media
  // that is actually audible to the user. It should be called whenever the
  // volume changes.
  //
  // Note: This does not catch all cases. E.g., headphones that are being
  // listened too, or even OS level volume state.
  void OnVolumeChange(double volume);

  // These methods are used to ensure that watch time is only reported for
  // videos that are actually visible to the user. They should be called when
  // the video is shown or hidden respectively.
  //
  // TODO(dalecurtis): At present, this is only called when the entire content
  // window goes into the foreground or background respectively; i.e. it does
  // not catch cases where the video is in the foreground but out of the view
  // port. We need a method for rejecting out of view port videos.
  void OnShown();
  void OnHidden();

 private:
  friend class WatchTimeReporterTest;

  // base::PowerObserver implementation.
  //
  // We only observe power source changes. We don't need to observe suspend and
  // resume events because we report watch time in terms of elapsed media time
  // and not in terms of elapsed real time.
  void OnPowerStateChange(bool on_battery_power) override;

  bool ShouldReportWatchTime();
  void MaybeStartReportingTimer(base::TimeDelta start_timestamp);
  enum class FinalizeTime { IMMEDIATELY, ON_NEXT_UPDATE };
  void MaybeFinalizeWatchTime(FinalizeTime finalize_time);
  void UpdateWatchTime();

  // Initialized during construction.
  const bool has_audio_;
  const bool has_video_;
  const bool is_mse_;
  const bool is_encrypted_;
  scoped_refptr<MediaLog> media_log_;
  const gfx::Size initial_video_size_;
  const GetMediaTimeCB get_media_time_cb_;

  // The amount of time between each UpdateWatchTime(); this is the frequency by
  // which the watch times are updated. In the event of a process crash or kill
  // this is also the most amount of watch time that we might lose.
  base::TimeDelta reporting_interval_ = base::TimeDelta::FromSeconds(5);

  base::RepeatingTimer reporting_timer_;

  // Updated by the OnXXX() methods above.
  bool is_on_battery_power_ = false;
  bool is_playing_ = false;
  bool is_visible_ = true;
  double volume_ = 1.0;

  // The last media timestamp seen by UpdateWatchTime().
  base::TimeDelta last_media_timestamp_ = kNoTimestamp;

  // The starting and ending timestamps used for reporting watch time.
  base::TimeDelta start_timestamp_;
  base::TimeDelta end_timestamp_ = kNoTimestamp;

  // Similar to the above but tracks watch time relative to whether or not
  // battery or AC power is being used.
  base::TimeDelta start_timestamp_for_power_;
  base::TimeDelta end_timestamp_for_power_ = kNoTimestamp;

  DISALLOW_COPY_AND_ASSIGN(WatchTimeReporter);
};

}  // namespace media

#endif  // MEDIA_BLINK_WATCH_TIME_REPORTER_H_
