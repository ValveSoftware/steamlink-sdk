// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/Timing.h"

namespace blink {

String Timing::fillModeString(FillMode fillMode) {
  switch (fillMode) {
    case FillMode::NONE:
      return "none";
    case FillMode::FORWARDS:
      return "forwards";
    case FillMode::BACKWARDS:
      return "backwards";
    case FillMode::BOTH:
      return "both";
    case FillMode::AUTO:
      return "auto";
  }
  NOTREACHED();
  return "none";
}

String Timing::playbackDirectionString(PlaybackDirection playbackDirection) {
  switch (playbackDirection) {
    case PlaybackDirection::NORMAL:
      return "normal";
    case PlaybackDirection::REVERSE:
      return "reverse";
    case PlaybackDirection::ALTERNATE_NORMAL:
      return "alternate";
    case PlaybackDirection::ALTERNATE_REVERSE:
      return "alternate-reverse";
  }
  NOTREACHED();
  return "normal";
}

}  // namespace blink
