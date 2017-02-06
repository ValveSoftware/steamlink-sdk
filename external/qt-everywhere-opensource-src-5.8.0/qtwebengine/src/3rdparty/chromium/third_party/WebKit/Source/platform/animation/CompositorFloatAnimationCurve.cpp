// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorFloatAnimationCurve.h"

#include "cc/animation/animation_curve.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/timing_function.h"
#include "wtf/PtrUtil.h"
#include <memory>

using blink::CompositorFloatKeyframe;

namespace blink {

CompositorFloatAnimationCurve::CompositorFloatAnimationCurve()
    : m_curve(cc::KeyframedFloatAnimationCurve::Create())
{
}

CompositorFloatAnimationCurve::CompositorFloatAnimationCurve(std::unique_ptr<cc::KeyframedFloatAnimationCurve> curve)
    : m_curve(std::move(curve))
{
}

CompositorFloatAnimationCurve::~CompositorFloatAnimationCurve()
{
}

std::unique_ptr<CompositorFloatAnimationCurve> CompositorFloatAnimationCurve::CreateForTesting(std::unique_ptr<cc::KeyframedFloatAnimationCurve> curve)
{
    return wrapUnique(new CompositorFloatAnimationCurve(std::move(curve)));
}

Vector<CompositorFloatKeyframe> CompositorFloatAnimationCurve::keyframesForTesting() const
{
    Vector<CompositorFloatKeyframe> keyframes;
    for (const auto& ccKeyframe : m_curve->keyframes_for_testing()) {
        CompositorFloatKeyframe keyframe(ccKeyframe->Time().InSecondsF(), ccKeyframe->Value());
        keyframes.append(keyframe);
    }
    return keyframes;
}

CubicBezierTimingFunction::EaseType CompositorFloatAnimationCurve::getCurveEaseTypeForTesting() const
{
    const cc::TimingFunction* timingFunction = m_curve->timing_function_for_testing();
    DCHECK(timingFunction);
    DCHECK_EQ(timingFunction->GetType(), cc::TimingFunction::Type::CUBIC_BEZIER);
    auto cubicTimingFunction = static_cast<const cc::CubicBezierTimingFunction*>(timingFunction);
    return cubicTimingFunction->ease_type();
}

bool CompositorFloatAnimationCurve::curveHasLinearTimingFunctionForTesting() const
{
    return !m_curve->timing_function_for_testing();
}

CubicBezierTimingFunction::EaseType CompositorFloatAnimationCurve::getKeyframeEaseTypeForTesting(unsigned long index) const
{
    DCHECK_LT(index, m_curve->keyframes_for_testing().size());
    const cc::TimingFunction* timingFunction = m_curve->keyframes_for_testing()[index]->timing_function();
    DCHECK(timingFunction);
    DCHECK_EQ(timingFunction->GetType(), cc::TimingFunction::Type::CUBIC_BEZIER);
    auto cubicTimingFunction = static_cast<const cc::CubicBezierTimingFunction*>(timingFunction);
    return cubicTimingFunction->ease_type();
}

bool CompositorFloatAnimationCurve::keyframeHasLinearTimingFunctionForTesting(unsigned long index) const
{
    DCHECK_LT(index, m_curve->keyframes_for_testing().size());
    return !m_curve->keyframes_for_testing()[index]->timing_function();
}

void CompositorFloatAnimationCurve::addLinearKeyframe(const CompositorFloatKeyframe& keyframe)
{
    m_curve->AddKeyframe(
        cc::FloatKeyframe::Create(base::TimeDelta::FromSecondsD(keyframe.time),
            keyframe.value, nullptr));
}

void CompositorFloatAnimationCurve::addCubicBezierKeyframe(const CompositorFloatKeyframe& keyframe,
    CubicBezierTimingFunction::EaseType easeType)
{
    m_curve->AddKeyframe(
        cc::FloatKeyframe::Create(base::TimeDelta::FromSecondsD(keyframe.time),
            keyframe.value, cc::CubicBezierTimingFunction::CreatePreset(easeType)));
}

void CompositorFloatAnimationCurve::addCubicBezierKeyframe(const CompositorFloatKeyframe& keyframe, double x1, double y1, double x2, double y2)
{
    m_curve->AddKeyframe(cc::FloatKeyframe::Create(
        base::TimeDelta::FromSecondsD(keyframe.time), keyframe.value,
        cc::CubicBezierTimingFunction::Create(x1, y1, x2, y2)));
}

void CompositorFloatAnimationCurve::addStepsKeyframe(const CompositorFloatKeyframe& keyframe, int steps, StepsTimingFunction::StepPosition stepPosition)
{
    m_curve->AddKeyframe(cc::FloatKeyframe::Create(
        base::TimeDelta::FromSecondsD(keyframe.time), keyframe.value,
        cc::StepsTimingFunction::Create(steps, stepPosition)));
}

void CompositorFloatAnimationCurve::setLinearTimingFunction()
{
    m_curve->SetTimingFunction(nullptr);
}

void CompositorFloatAnimationCurve::setCubicBezierTimingFunction(CubicBezierTimingFunction::EaseType easeType)
{
    m_curve->SetTimingFunction(cc::CubicBezierTimingFunction::CreatePreset(easeType));
}

void CompositorFloatAnimationCurve::setCubicBezierTimingFunction(double x1, double y1, double x2, double y2)
{
    m_curve->SetTimingFunction(cc::CubicBezierTimingFunction::Create(x1, y1, x2, y2));
}

void CompositorFloatAnimationCurve::setStepsTimingFunction(int numberOfSteps, StepsTimingFunction::StepPosition stepPosition)
{
    m_curve->SetTimingFunction(cc::StepsTimingFunction::Create(numberOfSteps, stepPosition));
}

float CompositorFloatAnimationCurve::getValue(double time) const
{
    return m_curve->GetValue(base::TimeDelta::FromSecondsD(time));
}

std::unique_ptr<cc::AnimationCurve> CompositorFloatAnimationCurve::cloneToAnimationCurve() const
{
    return m_curve->Clone();
}

} // namespace blink
