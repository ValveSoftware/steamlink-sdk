// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_statistics.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "media/base/android/demuxer_stream_player_params.h"

namespace media {

// Minimum playback interval to report.
const int kMinDurationInSeconds = 2;

// Maximum playback interval to report.
const int kMaxDurationInSeconds = 3600;

// Number of slots in the histogram for playback interval.
const int kNumDurationSlots = 50;

// For easier reading.
const int kOneMillion = 1000000;

void FrameStatistics::IncrementLateFrameCount() {
  // Do not collect the late frame if this is the first frame after the start.
  // Right now we do not want to consider the late video frame which is the
  // first after preroll, preroll may be inacurate in this respect.
  // The first audio frame cannot be late by definition and by not considering
  // it we can simplify audio decoder code.
  if (total == 1)
    return;

  ++late;
}

MediaStatistics::MediaStatistics() {}

MediaStatistics::~MediaStatistics() {}

void MediaStatistics::Start(base::TimeDelta current_playback_time) {
  DVLOG(1) << __FUNCTION__;

  if (start_time_ == kNoTimestamp()) {
    Clear();
    start_time_ = current_playback_time;
  }
}

void MediaStatistics::StopAndReport(base::TimeDelta current_playback_time) {
  DVLOG(1) << __FUNCTION__;

  if (start_time_ == kNoTimestamp())
    return;  // skip if there was no prior Start().

  if (current_playback_time == kNoTimestamp()) {
    // Cancel the start event and skip if current time is unknown.
    start_time_ = kNoTimestamp();
    return;
  }

  base::TimeDelta duration = current_playback_time - start_time_;

  // Reset start time.
  start_time_ = kNoTimestamp();

  if (duration < base::TimeDelta::FromSeconds(kMinDurationInSeconds))
    return;  // duration is too short.

  if (duration > base::TimeDelta::FromSeconds(kMaxDurationInSeconds))
    return;  // duration is too long.

  Report(duration);
}

void MediaStatistics::Clear() {
  start_time_ = kNoTimestamp();
  audio_frame_stats_.Clear();
  video_frame_stats_.Clear();
  num_starvations_ = 0;
}

void MediaStatistics::Report(base::TimeDelta duration) {
  DVLOG(1) << __FUNCTION__ << " duration:" << duration
           << " audio frames:"
           << audio_frame_stats_.late << "/" << audio_frame_stats_.total
           << " video frames:"
           << video_frame_stats_.late << "/" << video_frame_stats_.total
           << " starvations:" << num_starvations_;

  // Playback duration is the time interval between the moment playback starts
  // and the moment it is interrupted either by stopping or by seeking, changing
  // to full screen, minimizing the browser etc. The interval is measured by
  // media time.

  UMA_HISTOGRAM_CUSTOM_TIMES(
      "Media.MSE.PlaybackDuration", duration,
      base::TimeDelta::FromSeconds(kMinDurationInSeconds),
      base::TimeDelta::FromSeconds(kMaxDurationInSeconds), kNumDurationSlots);

  // Number of late frames per one million frames.

  if (audio_frame_stats_.total) {
    UMA_HISTOGRAM_COUNTS(
        "Media.MSE.LateAudioFrames",
        kOneMillion * audio_frame_stats_.late / audio_frame_stats_.total);
  }

  if (video_frame_stats_.total) {
    UMA_HISTOGRAM_COUNTS(
        "Media.MSE.LateVideoFrames",
        kOneMillion * video_frame_stats_.late / video_frame_stats_.total);
  }

  // Number of starvations per one million frames.

  uint32_t total_frames = audio_frame_stats_.total ? audio_frame_stats_.total
                                                   : video_frame_stats_.total;
  if (total_frames) {
    UMA_HISTOGRAM_COUNTS("Media.MSE.Starvations",
                         kOneMillion * num_starvations_ / total_frames);
  }
}

}  // namespace media
