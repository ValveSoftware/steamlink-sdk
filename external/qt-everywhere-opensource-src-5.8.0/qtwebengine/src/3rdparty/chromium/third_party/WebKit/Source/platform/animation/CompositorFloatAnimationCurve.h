// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorFloatAnimationCurve_h
#define CompositorFloatAnimationCurve_h

#include "platform/PlatformExport.h"
#include "platform/animation/CompositorAnimationCurve.h"
#include "platform/animation/CompositorFloatKeyframe.h"
#include "platform/animation/TimingFunction.h"
#include "wtf/Noncopyable.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"
#include <memory>

namespace cc {
class KeyframedFloatAnimationCurve;
}

namespace blink {
struct CompositorFloatKeyframe;
}

namespace blink {

// A keyframed float animation curve.
class PLATFORM_EXPORT CompositorFloatAnimationCurve : public CompositorAnimationCurve {
    WTF_MAKE_NONCOPYABLE(CompositorFloatAnimationCurve);
public:
    static std::unique_ptr<CompositorFloatAnimationCurve> create()
    {
        return wrapUnique(new CompositorFloatAnimationCurve());
    }

    ~CompositorFloatAnimationCurve() override;

    static std::unique_ptr<CompositorFloatAnimationCurve> CreateForTesting(std::unique_ptr<cc::KeyframedFloatAnimationCurve>);
    Vector<CompositorFloatKeyframe> keyframesForTesting() const;

    // TODO(loyso): Erase these methods once blink/cc timing functions unified.
    CubicBezierTimingFunction::EaseType getCurveEaseTypeForTesting() const;
    bool curveHasLinearTimingFunctionForTesting() const;
    CubicBezierTimingFunction::EaseType getKeyframeEaseTypeForTesting(unsigned long index) const;
    bool keyframeHasLinearTimingFunctionForTesting(unsigned long index) const;

    void addLinearKeyframe(const CompositorFloatKeyframe&);
    void addCubicBezierKeyframe(const CompositorFloatKeyframe&, CubicBezierTimingFunction::EaseType);
    // Adds the keyframe with a custom, bezier timing function. Note, it is
    // assumed that x0 = y0 , and x3 = y3 = 1.
    void addCubicBezierKeyframe(const CompositorFloatKeyframe&, double x1, double y1, double x2, double y2);
    void addStepsKeyframe(const CompositorFloatKeyframe&, int steps, StepsTimingFunction::StepPosition);

    void setLinearTimingFunction();
    void setCubicBezierTimingFunction(CubicBezierTimingFunction::EaseType);
    void setCubicBezierTimingFunction(double x1, double y1, double x2, double y2);
    void setStepsTimingFunction(int numberOfSteps, StepsTimingFunction::StepPosition);

    float getValue(double time) const;

    // CompositorAnimationCurve implementation.
    std::unique_ptr<cc::AnimationCurve> cloneToAnimationCurve() const override;

private:
    CompositorFloatAnimationCurve();
    CompositorFloatAnimationCurve(std::unique_ptr<cc::KeyframedFloatAnimationCurve>);

    std::unique_ptr<cc::KeyframedFloatAnimationCurve> m_curve;
};

} // namespace blink

#endif // CompositorFloatAnimationCurve_h
