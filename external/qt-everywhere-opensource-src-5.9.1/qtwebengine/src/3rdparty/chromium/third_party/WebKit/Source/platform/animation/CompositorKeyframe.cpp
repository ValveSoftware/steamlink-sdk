// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorKeyframe.h"

#include "platform/animation/TimingFunction.h"

namespace blink {

PassRefPtr<TimingFunction> CompositorKeyframe::getTimingFunctionForTesting()
    const {
  return createCompositorTimingFunctionFromCC(ccTimingFunction());
}

}  // namespace blink
