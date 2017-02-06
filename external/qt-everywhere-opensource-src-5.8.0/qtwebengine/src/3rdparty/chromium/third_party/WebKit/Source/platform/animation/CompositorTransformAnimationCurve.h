// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorTransformAnimationCurve_h
#define CompositorTransformAnimationCurve_h

#include "platform/PlatformExport.h"
#include "platform/animation/CompositorAnimationCurve.h"
#include "platform/animation/CompositorTransformKeyframe.h"
#include "platform/animation/TimingFunction.h"
#include "wtf/Noncopyable.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace cc {
class KeyframedTransformAnimationCurve;
}

namespace blink {
class CompositorTransformKeyframe;
}

namespace blink {

// A keyframed transform animation curve.
class PLATFORM_EXPORT CompositorTransformAnimationCurve : public CompositorAnimationCurve {
    WTF_MAKE_NONCOPYABLE(CompositorTransformAnimationCurve);
public:
    static std::unique_ptr<CompositorTransformAnimationCurve> create()
    {
        return wrapUnique(new CompositorTransformAnimationCurve());
    }

    ~CompositorTransformAnimationCurve() override;

    void addLinearKeyframe(const CompositorTransformKeyframe&);
    void addCubicBezierKeyframe(const CompositorTransformKeyframe&, CubicBezierTimingFunction::EaseType);
    // Adds the keyframe with a custom, bezier timing function. Note, it is
    // assumed that x0 = y0, and x3 = y3 = 1.
    void addCubicBezierKeyframe(const CompositorTransformKeyframe&, double x1, double y1, double x2, double y2);
    void addStepsKeyframe(const CompositorTransformKeyframe&, int steps, StepsTimingFunction::StepPosition);

    void setLinearTimingFunction();
    void setCubicBezierTimingFunction(CubicBezierTimingFunction::EaseType);
    void setCubicBezierTimingFunction(double x1, double y1, double x2, double y2);
    void setStepsTimingFunction(int numberOfSteps, StepsTimingFunction::StepPosition);

    // CompositorAnimationCurve implementation.
    std::unique_ptr<cc::AnimationCurve> cloneToAnimationCurve() const override;

private:
    CompositorTransformAnimationCurve();

    std::unique_ptr<cc::KeyframedTransformAnimationCurve> m_curve;
};

} // namespace blink

#endif // CompositorTransformAnimationCurve_h
