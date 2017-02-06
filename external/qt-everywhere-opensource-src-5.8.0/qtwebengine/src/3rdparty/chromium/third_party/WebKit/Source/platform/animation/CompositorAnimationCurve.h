// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorAnimationCurve_h
#define CompositorAnimationCurve_h

#include "platform/PlatformExport.h"

#include <memory>

namespace cc {
class AnimationCurve;
}

namespace blink {

class PLATFORM_EXPORT CompositorAnimationCurve {
public:
    virtual ~CompositorAnimationCurve() {}
    virtual std::unique_ptr<cc::AnimationCurve> cloneToAnimationCurve() const = 0;
};

} // namespace blink

#endif // CompositorAnimationCurve_h
