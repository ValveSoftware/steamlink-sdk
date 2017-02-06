// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_STATISTICS_H_
#define MEDIA_BASE_ANDROID_MEDIA_STATISTICS_H_

#include <stdint.h>
#include "base/time/time.h"
#include "media/base/demuxer_stream.h"
#include "media/base/timestamp_constants.h"

namespace media {

// FrameStatistics struct deals with frames of one stream, i.e. either
// audio or video.
struct FrameStatistics {
  // Audio: total number of frames that have been rendered.
  // Video: total number of frames that were supposed to be rendered. Late video
  //        frames might be skipped, but are counted here.
  uint32_t total = 0;

  // A number of late frames. Late frames are a subset of the total frames.
  // Audio: The frame is late if it might cause an underrun, i.e. comes from
  //        decoder when audio buffer is already depleted.
  // Video: The frame is late if it missed its presentation time as determined
  //        by PTS when it comes from decoder.  The rendering policy (i.e.
  //        render or skip) does not affect this number.
  uint32_t late = 0;

  void Clear() { total = late = 0; }

  // Increments |total| frame count.
  void IncrementFrameCount() { ++total; }

  // Increments |late| frame count except it is the first frame after start.
  // For each IncrementLateFrameCount() call there should be preceding
  // IncrementFrameCount() call.
  void IncrementLateFrameCount();
};

// MediaStatistics class gathers and reports playback quality statistics to UMA.
//
// This class is not thread safe. The caller should guarantee that operations
// on FrameStatistics objects does not happen during Start() and
// StopAndReport(). The Start() and StopAndReport() methods need to be called
// sequentially.

class MediaStatistics {
 public:
  MediaStatistics();
  ~MediaStatistics();

  // Returns the frame statistics for audio frames.
  FrameStatistics& audio_frame_stats() { return audio_frame_stats_; }

  // Returns the frame statistics for video frames.
  FrameStatistics& video_frame_stats() { return video_frame_stats_; }

  // Starts gathering statistics. When called in a row only the first call will
  // take effect.
  void Start(base::TimeDelta current_playback_time);

  // Stops gathering statistics, calculate and report results. When called
  // in a row only the first call will take effect.
  void StopAndReport(base::TimeDelta current_playback_time);

  // Adds starvation event. Starvation happens when the player interrupts
  // the regular playback and asks for more data.
  void AddStarvation() { ++num_starvations_; }

 private:
  // Resets the data to the initial state.
  void Clear();

  // Calculates relative data based on total frame numbers and reports it and
  // the duration to UMA.
  void Report(base::TimeDelta duration);

  base::TimeDelta start_time_ = kNoTimestamp();
  FrameStatistics audio_frame_stats_;
  FrameStatistics video_frame_stats_;
  uint32_t num_starvations_ = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_STATISTICS_H_
