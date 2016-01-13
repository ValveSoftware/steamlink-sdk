// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_buffers_state.h"

namespace media {

AudioBuffersState::AudioBuffersState()
    : pending_bytes(0),
      hardware_delay_bytes(0) {
}

AudioBuffersState::AudioBuffersState(int pending_bytes,
                                     int hardware_delay_bytes)
    : pending_bytes(pending_bytes),
      hardware_delay_bytes(hardware_delay_bytes) {
}

}  // namespace media
