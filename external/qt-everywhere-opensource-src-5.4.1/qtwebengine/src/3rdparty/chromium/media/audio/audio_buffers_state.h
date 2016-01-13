// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_BUFFERS_STATE_H_
#define MEDIA_AUDIO_AUDIO_BUFFERS_STATE_H_

#include "media/base/media_export.h"

namespace media {

// AudioBuffersState struct stores current state of audio buffers.
// It is used for audio synchronization.
struct MEDIA_EXPORT AudioBuffersState {
  AudioBuffersState();
  AudioBuffersState(int pending_bytes, int hardware_delay_bytes);

  int total_bytes() {
    return pending_bytes + hardware_delay_bytes;
  }

  // Number of bytes we currently have in our software buffer.
  int pending_bytes;

  // Number of bytes that have been written to the device, but haven't
  // been played yet.
  int hardware_delay_bytes;
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_BUFFERS_STATE_H_
