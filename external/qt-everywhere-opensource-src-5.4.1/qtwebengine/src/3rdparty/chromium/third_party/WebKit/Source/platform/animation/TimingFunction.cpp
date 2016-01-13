// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/animation/TimingFunction.h"

#include "wtf/MathExtras.h"

namespace WebCore {

String LinearTimingFunction::toString() const
{
    return "linear";
}

double LinearTimingFunction::evaluate(double fraction, double) const
{
    return fraction;
}

String CubicBezierTimingFunction::toString() const
{
    switch (this->subType()) {
    case CubicBezierTimingFunction::Ease:
        return "ease";
    case CubicBezierTimingFunction::EaseIn:
        return "ease-in";
    case CubicBezierTimingFunction::EaseOut:
        return "ease-out";
    case CubicBezierTimingFunction::EaseInOut:
        return "ease-in-out";
    case CubicBezierTimingFunction::Custom:
        return "cubic-bezier(" + String::numberToStringECMAScript(this->x1()) + ", " +
            String::numberToStringECMAScript(this->y1()) + ", " + String::numberToStringECMAScript(this->x2()) +
            ", " + String::numberToStringECMAScript(this->y2()) + ")";
    default:
        ASSERT_NOT_REACHED();
    }
    return "";
}

double CubicBezierTimingFunction::evaluate(double fraction, double accuracy) const
{
    if (!m_bezier)
        m_bezier = adoptPtr(new UnitBezier(m_x1, m_y1, m_x2, m_y2));
    return m_bezier->solve(fraction, accuracy);
}

String StepsTimingFunction::toString() const
{
    StringBuilder builder;
    switch (this->subType()) {
    case StepsTimingFunction::Start:
        return "step-start";
    case StepsTimingFunction::Middle:
        return "step-middle";
    case StepsTimingFunction::End:
        return "step-end";
    case StepsTimingFunction::Custom:
        builder.append("steps(" + String::numberToStringECMAScript(this->numberOfSteps()) + ", ");

        if (this->stepAtPosition() == StepsTimingFunction::StepAtStart)
            builder.append("start");
        else if (this->stepAtPosition() == StepsTimingFunction::StepAtMiddle)
            builder.append("middle");
        else if (this->stepAtPosition() == StepsTimingFunction::StepAtEnd)
            builder.append("end");
        else
            ASSERT_NOT_REACHED();

        builder.append(")");
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return builder.toString();
}

double StepsTimingFunction::evaluate(double fraction, double) const
{
    double startOffset = 0;
    switch (m_stepAtPosition) {
    case StepAtStart:
        startOffset = 1;
        break;
    case StepAtMiddle:
        startOffset = 0.5;
        break;
    case StepAtEnd:
        startOffset = 0;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return clampTo(floor((m_steps * fraction) + startOffset) / m_steps, 0.0, 1.0);
}

// Equals operators
bool operator==(const LinearTimingFunction& lhs, const TimingFunction& rhs)
{
    return rhs.type() == TimingFunction::LinearFunction;
}

bool operator==(const CubicBezierTimingFunction& lhs, const TimingFunction& rhs)
{
    if (rhs.type() != TimingFunction::CubicBezierFunction)
        return false;

    const CubicBezierTimingFunction& ctf = toCubicBezierTimingFunction(rhs);
    if ((lhs.subType() == CubicBezierTimingFunction::Custom) && (ctf.subType() == CubicBezierTimingFunction::Custom))
        return (lhs.x1() == ctf.x1()) && (lhs.y1() == ctf.y1()) && (lhs.x2() == ctf.x2()) && (lhs.y2() == ctf.y2());

    return lhs.subType() == ctf.subType();
}

bool operator==(const StepsTimingFunction& lhs, const TimingFunction& rhs)
{
    if (rhs.type() != TimingFunction::StepsFunction)
        return false;

    const StepsTimingFunction& stf = toStepsTimingFunction(rhs);
    if ((lhs.subType() == StepsTimingFunction::Custom) && (stf.subType() == StepsTimingFunction::Custom))
        return (lhs.numberOfSteps() == stf.numberOfSteps()) && (lhs.stepAtPosition() == stf.stepAtPosition());

    return lhs.subType() == stf.subType();
}

// The generic operator== *must* come after the
// non-generic operator== otherwise it will end up calling itself.
bool operator==(const TimingFunction& lhs, const TimingFunction& rhs)
{
    switch (lhs.type()) {
    case TimingFunction::LinearFunction: {
        const LinearTimingFunction& linear = toLinearTimingFunction(lhs);
        return (linear == rhs);
    }
    case TimingFunction::CubicBezierFunction: {
        const CubicBezierTimingFunction& cubic = toCubicBezierTimingFunction(lhs);
        return (cubic == rhs);
    }
    case TimingFunction::StepsFunction: {
        const StepsTimingFunction& step = toStepsTimingFunction(lhs);
        return (step == rhs);
    }
    default:
        ASSERT_NOT_REACHED();
    }
    return false;
}

// No need to define specific operator!= as they can all come via this function.
bool operator!=(const TimingFunction& lhs, const TimingFunction& rhs)
{
    return !(lhs == rhs);
}

} // namespace WebCore
