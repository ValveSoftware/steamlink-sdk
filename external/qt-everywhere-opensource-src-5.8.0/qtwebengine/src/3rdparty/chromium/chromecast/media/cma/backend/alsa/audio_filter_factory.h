// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ALSA_AUDIO_FILTER_FACTORY_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ALSA_AUDIO_FILTER_FACTORY_H_

#include <memory>

#include "chromecast/media/cma/backend/alsa/audio_filter_interface.h"

namespace chromecast {
namespace media {

class AudioFilterFactory {
 public:
  // FilterType specifies the usage of the created filter.
  enum FilterType { PRE_LOOPBACK_FILTER, POST_LOOPBACK_FILTER };

  // Creates a new AudioFilterInterface.
  static std::unique_ptr<AudioFilterInterface> MakeAudioFilter(
      FilterType filter_type);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ALSA_FILTER_AUDIO_FILTER_FACTORY_H_
