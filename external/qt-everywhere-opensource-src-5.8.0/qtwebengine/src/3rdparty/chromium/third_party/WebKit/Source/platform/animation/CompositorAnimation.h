// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorAnimation_h
#define CompositorAnimation_h

#include "cc/animation/animation.h"
#include "platform/PlatformExport.h"
#include "platform/animation/CompositorTargetProperty.h"
#include "wtf/Noncopyable.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace cc {
class Animation;
}

namespace blink {

class CompositorAnimationCurve;
class CompositorFloatAnimationCurve;

// A compositor driven animation.
class PLATFORM_EXPORT CompositorAnimation {
    WTF_MAKE_NONCOPYABLE(CompositorAnimation);
public:
    using Direction = cc::Animation::Direction;
    using FillMode = cc::Animation::FillMode;

    static std::unique_ptr<CompositorAnimation> create(const blink::CompositorAnimationCurve& curve, CompositorTargetProperty::Type target, int groupId, int animationId)
    {
        return wrapUnique(new CompositorAnimation(curve, target, animationId, groupId));
    }

    ~CompositorAnimation();

    // An id must be unique.
    int id() const;
    int group() const;

    CompositorTargetProperty::Type targetProperty() const;

    // This is the number of times that the animation will play. If this
    // value is zero the animation will not play. If it is negative, then
    // the animation will loop indefinitely.
    double iterations() const;
    void setIterations(double);

    double startTime() const;
    void setStartTime(double monotonicTime);

    double timeOffset() const;
    void setTimeOffset(double monotonicTime);

    Direction getDirection() const;
    void setDirection(Direction);

    double playbackRate() const;
    void setPlaybackRate(double);

    FillMode getFillMode() const;
    void setFillMode(FillMode);

    double iterationStart() const;
    void setIterationStart(double);

    std::unique_ptr<cc::Animation> passAnimation();

    std::unique_ptr<CompositorFloatAnimationCurve> floatCurveForTesting() const;

private:
    CompositorAnimation(const CompositorAnimationCurve&, CompositorTargetProperty::Type, int animationId, int groupId);

    std::unique_ptr<cc::Animation> m_animation;
};

} // namespace blink

#endif // CompositorAnimation_h
