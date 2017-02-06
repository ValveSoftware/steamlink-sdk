// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/audio_filter_factory.h"

// An AudioFilterFactory that just returns nullptrs

namespace chromecast {
namespace media {

std::unique_ptr<AudioFilterInterface> AudioFilterFactory::MakeAudioFilter(
    AudioFilterFactory::FilterType filter_type) {
  return nullptr;
}

}  // namespace media
}  // namespace chromecast
