// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ALSA_AUDIO_FILTER_INTERFACE_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ALSA_AUDIO_FILTER_INTERFACE_H_

#include <stdint.h>

#include "media/base/sample_format.h"

namespace chromecast {
namespace media {

class AudioFilterInterface {
 public:
  virtual ~AudioFilterInterface() = default;
  virtual bool SetSampleRateAndFormat(int sample_rate,
                                      ::media::SampleFormat sample_format) = 0;

  // Process data frames. Must be interleaved. |data| will be overwritten.
  virtual bool ProcessInterleaved(uint8_t* data, int frames) = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ALSA_AUDIO_FILTER_INTERFACE_H_
