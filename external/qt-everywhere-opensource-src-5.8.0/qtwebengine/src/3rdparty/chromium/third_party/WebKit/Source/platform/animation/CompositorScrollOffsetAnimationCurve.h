// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorScrollOffsetAnimationCurve_h
#define CompositorScrollOffsetAnimationCurve_h

#include "platform/PlatformExport.h"
#include "platform/animation/CompositorAnimationCurve.h"
#include "platform/geometry/FloatPoint.h"
#include "wtf/Noncopyable.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace cc {
class ScrollOffsetAnimationCurve;
}

namespace blink {

class PLATFORM_EXPORT CompositorScrollOffsetAnimationCurve : public CompositorAnimationCurve {
    WTF_MAKE_NONCOPYABLE(CompositorScrollOffsetAnimationCurve);
public:
    enum ScrollDurationBehavior {
        ScrollDurationDeltaBased = 0,
        ScrollDurationConstant,
        ScrollDurationInverseDelta
    };

    static std::unique_ptr<CompositorScrollOffsetAnimationCurve> create(FloatPoint targetValue, CompositorScrollOffsetAnimationCurve::ScrollDurationBehavior durationBehavior)
    {
        return wrapUnique(new CompositorScrollOffsetAnimationCurve(targetValue, durationBehavior));
    }
    static std::unique_ptr<CompositorScrollOffsetAnimationCurve> create(cc::ScrollOffsetAnimationCurve* curve)
    {
        return wrapUnique(new CompositorScrollOffsetAnimationCurve(curve));
    }

    ~CompositorScrollOffsetAnimationCurve() override;

    void setInitialValue(FloatPoint);
    FloatPoint getValue(double time) const;
    double duration() const;
    FloatPoint targetValue() const;
    void applyAdjustment(IntSize);
    void updateTarget(double time, FloatPoint newTarget);

    // CompositorAnimationCurve implementation.
    std::unique_ptr<cc::AnimationCurve> cloneToAnimationCurve() const override;

private:
    CompositorScrollOffsetAnimationCurve(FloatPoint, ScrollDurationBehavior);
    CompositorScrollOffsetAnimationCurve(cc::ScrollOffsetAnimationCurve*);

    std::unique_ptr<cc::ScrollOffsetAnimationCurve> m_curve;
};

} // namespace blink

#endif // CompositorScrollOffsetAnimationCurve_h
