// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/media_stream_audio_processor_options.h"
#include "content/renderer/media/mock_constraint_factory.h"
#include "third_party/webrtc/api/mediaconstraintsinterface.h"

namespace content {

MockConstraintFactory::MockConstraintFactory() {}

MockConstraintFactory::~MockConstraintFactory() {}

blink::WebMediaTrackConstraintSet& MockConstraintFactory::AddAdvanced() {
  advanced_.emplace_back();
  return advanced_.back();
}

blink::WebMediaConstraints MockConstraintFactory::CreateWebMediaConstraints()
    const {
  blink::WebMediaConstraints constraints;
  constraints.initialize(basic_, advanced_);
  return constraints;
}

void MockConstraintFactory::DisableDefaultAudioConstraints() {
  basic_.googEchoCancellation.setExact(false);
  basic_.googExperimentalEchoCancellation.setExact(false);
  basic_.googAutoGainControl.setExact(false);
  basic_.googExperimentalAutoGainControl.setExact(false);
  basic_.googNoiseSuppression.setExact(false);
  basic_.googNoiseSuppression.setExact(false);
  basic_.googHighpassFilter.setExact(false);
  basic_.googTypingNoiseDetection.setExact(false);
  basic_.googExperimentalNoiseSuppression.setExact(false);
  basic_.googBeamforming.setExact(false);
}

void MockConstraintFactory::Reset() {
  basic_ = blink::WebMediaTrackConstraintSet();
  advanced_.clear();
}

}  // namespace content
