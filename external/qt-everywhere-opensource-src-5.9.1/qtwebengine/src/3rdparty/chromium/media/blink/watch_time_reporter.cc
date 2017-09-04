// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/watch_time_reporter.h"

#include "base/power_monitor/power_monitor.h"

namespace media {

// The minimum amount of media playback which can elapse before we'll report
// watch time metrics for a playback.
constexpr base::TimeDelta kMinimumElapsedWatchTime =
    base::TimeDelta::FromSeconds(7);

// The minimum width and height of videos to report watch time metrics for.
constexpr gfx::Size kMinimumVideoSize = gfx::Size(200, 200);

static bool IsOnBatteryPower() {
  if (base::PowerMonitor* pm = base::PowerMonitor::Get())
    return pm->IsOnBatteryPower();
  return false;
}

WatchTimeReporter::WatchTimeReporter(bool has_audio,
                                     bool has_video,
                                     bool is_mse,
                                     bool is_encrypted,
                                     scoped_refptr<MediaLog> media_log,
                                     const gfx::Size& initial_video_size,
                                     const GetMediaTimeCB& get_media_time_cb)
    : has_audio_(has_audio),
      has_video_(has_video),
      is_mse_(is_mse),
      is_encrypted_(is_encrypted),
      media_log_(std::move(media_log)),
      initial_video_size_(initial_video_size),
      get_media_time_cb_(get_media_time_cb) {
  DCHECK(!get_media_time_cb_.is_null());
  DCHECK(has_audio_ || has_video_);

  if (base::PowerMonitor* pm = base::PowerMonitor::Get())
    pm->AddObserver(this);
}

WatchTimeReporter::~WatchTimeReporter() {
  // If the timer is still running, finalize immediately, this is our last
  // chance to capture metrics.
  if (reporting_timer_.IsRunning())
    MaybeFinalizeWatchTime(FinalizeTime::IMMEDIATELY);

  if (base::PowerMonitor* pm = base::PowerMonitor::Get())
    pm->RemoveObserver(this);
}

void WatchTimeReporter::OnPlaying() {
  is_playing_ = true;
  MaybeStartReportingTimer(get_media_time_cb_.Run());
}

void WatchTimeReporter::OnPaused() {
  is_playing_ = false;
  MaybeFinalizeWatchTime(FinalizeTime::ON_NEXT_UPDATE);
}

void WatchTimeReporter::OnSeeking() {
  if (!reporting_timer_.IsRunning())
    return;

  // Seek is a special case that does not have hysteresis, when this is called
  // the seek is imminent, so finalize the previous playback immediately.

  // Don't trample an existing end timestamp.
  if (end_timestamp_ == kNoTimestamp)
    end_timestamp_ = get_media_time_cb_.Run();
  UpdateWatchTime();
}

void WatchTimeReporter::OnVolumeChange(double volume) {
  const double old_volume = volume_;
  volume_ = volume;

  // We're only interesting in transitions in and out of the muted state.
  if (!old_volume && volume)
    MaybeStartReportingTimer(get_media_time_cb_.Run());
  else if (old_volume && !volume_)
    MaybeFinalizeWatchTime(FinalizeTime::ON_NEXT_UPDATE);
}

void WatchTimeReporter::OnShown() {
  if (!has_video_)
    return;

  is_visible_ = true;
  MaybeStartReportingTimer(get_media_time_cb_.Run());
}

void WatchTimeReporter::OnHidden() {
  if (!has_video_)
    return;

  is_visible_ = false;
  MaybeFinalizeWatchTime(FinalizeTime::ON_NEXT_UPDATE);
}

void WatchTimeReporter::OnPowerStateChange(bool on_battery_power) {
  if (!reporting_timer_.IsRunning())
    return;

  // Defer changing |is_on_battery_power_| until the next watch time report to
  // avoid momentary power changes from affecting the results.
  if (is_on_battery_power_ != on_battery_power) {
    end_timestamp_for_power_ = get_media_time_cb_.Run();

    // Restart the reporting timer so the full hysteresis is afforded.
    reporting_timer_.Start(FROM_HERE, reporting_interval_, this,
                           &WatchTimeReporter::UpdateWatchTime);
    return;
  }

  end_timestamp_for_power_ = kNoTimestamp;
}

bool WatchTimeReporter::ShouldReportWatchTime() {
  // Report listen time or watch time only for tracks that are audio-only or
  // have both an audio and video track of sufficient size.
  return (!has_video_ && has_audio_) ||
         (has_video_ && has_audio_ &&
          initial_video_size_.height() >= kMinimumVideoSize.height() &&
          initial_video_size_.width() >= kMinimumVideoSize.width());
}

void WatchTimeReporter::MaybeStartReportingTimer(
    base::TimeDelta start_timestamp) {
  // Don't start the timer if any of our state indicates we shouldn't; this
  // check is important since the various event handlers do not have to care
  // about the state of other events.
  if (!ShouldReportWatchTime() || !is_playing_ || !volume_ || !is_visible_) {
    // If we reach this point the timer should already have been stopped or
    // there is a pending finalize in flight.
    DCHECK(!reporting_timer_.IsRunning() || end_timestamp_ != kNoTimestamp);
    return;
  }

  // If we haven't finalized the last watch time metrics yet, count this
  // playback as a continuation of the previous metrics.
  if (end_timestamp_ != kNoTimestamp) {
    DCHECK(reporting_timer_.IsRunning());
    end_timestamp_ = kNoTimestamp;
    return;
  }

  // Don't restart the timer if it's already running.
  if (reporting_timer_.IsRunning())
    return;

  last_media_timestamp_ = end_timestamp_for_power_ = kNoTimestamp;
  is_on_battery_power_ = IsOnBatteryPower();
  start_timestamp_ = start_timestamp_for_power_ = start_timestamp;
  reporting_timer_.Start(FROM_HERE, reporting_interval_, this,
                         &WatchTimeReporter::UpdateWatchTime);
}

void WatchTimeReporter::MaybeFinalizeWatchTime(FinalizeTime finalize_time) {
  // Don't finalize if the timer is already stopped.
  if (!reporting_timer_.IsRunning())
    return;

  // Don't trample an existing finalize; the first takes precedence.
  if (end_timestamp_ == kNoTimestamp)
    end_timestamp_ = get_media_time_cb_.Run();

  if (finalize_time == FinalizeTime::IMMEDIATELY) {
    UpdateWatchTime();
    return;
  }

  // Always restart the timer when finalizing, so that we allow for the full
  // length of |kReportingInterval| to elapse for hysteresis purposes.
  DCHECK_EQ(finalize_time, FinalizeTime::ON_NEXT_UPDATE);
  reporting_timer_.Start(FROM_HERE, reporting_interval_, this,
                         &WatchTimeReporter::UpdateWatchTime);
}

void WatchTimeReporter::UpdateWatchTime() {
  DCHECK(ShouldReportWatchTime());

  const bool is_finalizing = end_timestamp_ != kNoTimestamp;
  const bool is_power_change_pending = end_timestamp_for_power_ != kNoTimestamp;

  // If we're finalizing the log, use the media time value at the time of
  // finalization.
  const base::TimeDelta current_timestamp =
      is_finalizing ? end_timestamp_ : get_media_time_cb_.Run();
  const base::TimeDelta elapsed = current_timestamp - start_timestamp_;

  // Only report watch time after some minimum amount has elapsed. Don't update
  // watch time if media time hasn't changed since the last run; this may occur
  // if a seek is taking some time to complete or the playback is stalled for
  // some reason.
  if (elapsed >= kMinimumElapsedWatchTime &&
      last_media_timestamp_ != current_timestamp) {
    last_media_timestamp_ = current_timestamp;

    std::unique_ptr<MediaLogEvent> log_event =
        media_log_->CreateEvent(MediaLogEvent::Type::WATCH_TIME_UPDATE);

    log_event->params.SetDoubleWithoutPathExpansion(
        has_video_ ? MediaLog::kWatchTimeAudioVideoAll
                   : MediaLog::kWatchTimeAudioAll,
        elapsed.InSecondsF());
    if (is_mse_) {
      log_event->params.SetDoubleWithoutPathExpansion(
          has_video_ ? MediaLog::kWatchTimeAudioVideoMse
                     : MediaLog::kWatchTimeAudioMse,
          elapsed.InSecondsF());
    } else {
      log_event->params.SetDoubleWithoutPathExpansion(
          has_video_ ? MediaLog::kWatchTimeAudioVideoSrc
                     : MediaLog::kWatchTimeAudioSrc,
          elapsed.InSecondsF());
    }
    if (is_encrypted_) {
      log_event->params.SetDoubleWithoutPathExpansion(
          has_video_ ? MediaLog::kWatchTimeAudioVideoEme
                     : MediaLog::kWatchTimeAudioEme,
          elapsed.InSecondsF());
    }

    // Record watch time using the last known value for |is_on_battery_power_|;
    // if there's a |pending_power_change_| use that to accurately finalize the
    // last bits of time in the previous bucket.
    const base::TimeDelta elapsed_power =
        (is_power_change_pending ? end_timestamp_for_power_
                                 : current_timestamp) -
        start_timestamp_for_power_;

    // Again, only update watch time if enough time has elapsed; we need to
    // recheck the elapsed time here since the power source can change anytime.
    if (elapsed_power >= kMinimumElapsedWatchTime) {
      if (is_on_battery_power_) {
        log_event->params.SetDoubleWithoutPathExpansion(
            has_video_ ? MediaLog::kWatchTimeAudioVideoBattery
                       : MediaLog::kWatchTimeAudioBattery,
            elapsed_power.InSecondsF());
      } else {
        log_event->params.SetDoubleWithoutPathExpansion(
            has_video_ ? MediaLog::kWatchTimeAudioVideoAc
                       : MediaLog::kWatchTimeAudioAc,
            elapsed_power.InSecondsF());
      }
    }

    if (is_finalizing)
      log_event->params.SetBoolean(MediaLog::kWatchTimeFinalize, true);
    else if (is_power_change_pending)
      log_event->params.SetBoolean(MediaLog::kWatchTimeFinalizePower, true);

    DVLOG(2) << "Sending watch time update.";
    media_log_->AddEvent(std::move(log_event));
  }

  if (is_power_change_pending) {
    // Invert battery power status here instead of using the value returned by
    // the PowerObserver since there may be a pending OnPowerStateChange().
    is_on_battery_power_ = !is_on_battery_power_;

    start_timestamp_for_power_ = end_timestamp_for_power_;
    end_timestamp_for_power_ = kNoTimestamp;
  }

  // Stop the timer if this is supposed to be our last tick.
  if (is_finalizing) {
    end_timestamp_ = kNoTimestamp;
    reporting_timer_.Stop();
  }
}

}  // namespace media
