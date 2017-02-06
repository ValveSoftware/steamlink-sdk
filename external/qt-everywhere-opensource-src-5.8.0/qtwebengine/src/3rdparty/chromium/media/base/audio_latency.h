// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_LATENCY_H_
#define MEDIA_BASE_AUDIO_LATENCY_H_

#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT AudioLatency {
 public:
  // Categories of expected latencies for input/output audio. Do not change
  // existing values, they are used for UMA histogram reporting.
  enum LatencyType {
    // Specific latency in milliseconds.
    LATENCY_EXACT_MS = 0,
    // Lowest possible latency which does not cause glitches.
    LATENCY_INTERACTIVE = 1,
    // Latency optimized for real time communication.
    LATENCY_RTC = 2,
    // Latency optimized for continuous playback and power saving.
    LATENCY_PLAYBACK = 3,
    // For validation only.
    LATENCY_LAST = LATENCY_PLAYBACK,
    LATENCY_COUNT = LATENCY_LAST + 1
  };

  // |preferred_buffer_size| should be set to 0 if a client has no preference.
  static int GetHighLatencyBufferSize(int sample_rate,
                                      int preferred_buffer_size);

  // |hardware_buffer_size| should be set to 0 if unknown/invalid/not preferred.
  static int GetRtcBufferSize(int sample_rate, int hardware_buffer_size);

  static int GetInteractiveBufferSize(int hardware_buffer_size);
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_LATENCY_H_
