/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef TimingFunction_h
#define TimingFunction_h

#include "cc/animation/timing_function.h"
#include "platform/animation/AnimationUtilities.h" // For blend()
#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"
#include "ui/gfx/geometry/cubic_bezier.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"

namespace blink {

class PLATFORM_EXPORT TimingFunction : public RefCounted<TimingFunction> {
public:

    enum FunctionType {
        kLinearFunction, kCubicBezierFunction, kStepsFunction
    };

    virtual ~TimingFunction() { }

    FunctionType type() const { return m_type; }

    virtual String toString() const = 0;

    // Evaluates the timing function at the given fraction. The accuracy parameter provides a hint as to the required
    // accuracy and is not guaranteed.
    virtual double evaluate(double fraction, double accuracy) const = 0;

    // This function returns the minimum and maximum values obtainable when
    // calling evaluate();
    virtual void range(double* minValue, double* maxValue) const = 0;

protected:
    TimingFunction(FunctionType type)
        : m_type(type)
    {
    }

private:
    FunctionType m_type;
};

class PLATFORM_EXPORT LinearTimingFunction final : public TimingFunction {
public:
    static LinearTimingFunction* shared()
    {
        DEFINE_STATIC_REF(LinearTimingFunction, linear, (adoptRef(new LinearTimingFunction())));
        return linear;
    }

    ~LinearTimingFunction() override { }

    String toString() const override;

    double evaluate(double fraction, double) const override;
    void range(double* minValue, double* maxValue) const override;
private:
    LinearTimingFunction()
        : TimingFunction(kLinearFunction)
    {
    }
};

class PLATFORM_EXPORT CubicBezierTimingFunction final : public TimingFunction {
public:
    using EaseType = cc::CubicBezierTimingFunction::EaseType;

    static PassRefPtr<CubicBezierTimingFunction> create(double x1, double y1, double x2, double y2)
    {
        return adoptRef(new CubicBezierTimingFunction(EaseType::CUSTOM, x1, y1, x2, y2));
    }

    static CubicBezierTimingFunction* preset(EaseType easeType)
    {
        DEFINE_STATIC_REF(CubicBezierTimingFunction, ease, (adoptRef(new CubicBezierTimingFunction(EaseType::EASE, 0.25, 0.1, 0.25, 1.0))));
        DEFINE_STATIC_REF(CubicBezierTimingFunction, easeIn, (adoptRef(new CubicBezierTimingFunction(EaseType::EASE_IN, 0.42, 0.0, 1.0, 1.0))));
        DEFINE_STATIC_REF(CubicBezierTimingFunction, easeOut, (adoptRef(new CubicBezierTimingFunction(EaseType::EASE_OUT, 0.0, 0.0, 0.58, 1.0))));
        DEFINE_STATIC_REF(CubicBezierTimingFunction, easeInOut, (adoptRef(new CubicBezierTimingFunction(EaseType::EASE_IN_OUT, 0.42, 0.0, 0.58, 1.0))));
        DEFINE_STATIC_REF(CubicBezierTimingFunction, easeOutNatural, (adoptRef(new CubicBezierTimingFunction(EaseType::EASE_OUT_NATURAL, 0.26, 0.46, 0.45, 0.94))));

        switch (easeType) {
        case EaseType::EASE:
            return ease;
        case EaseType::EASE_IN:
            return easeIn;
        case EaseType::EASE_OUT:
            return easeOut;
        case EaseType::EASE_IN_OUT:
            return easeInOut;
        case EaseType::EASE_OUT_NATURAL:
            return easeOutNatural;
        default:
            NOTREACHED();
            return nullptr;
        }
    }

    ~CubicBezierTimingFunction() override { }

    String toString() const override;

    double evaluate(double fraction, double accuracy) const override;
    void range(double* minValue, double* maxValue) const override;

    double x1() const { return m_x1; }
    double y1() const { return m_y1; }
    double x2() const { return m_x2; }
    double y2() const { return m_y2; }

    EaseType getEaseType() const { return m_easeType; }

private:
    explicit CubicBezierTimingFunction(EaseType easeType, double x1, double y1, double x2, double y2)
        : TimingFunction(kCubicBezierFunction)
        , m_bezier(x1, y1, x2, y2)
        , m_x1(x1)
        , m_y1(y1)
        , m_x2(x2)
        , m_y2(y2)
        , m_easeType(easeType)
    {
    }

    // Finds points on the cubic bezier that cross the given horizontal
    // line, storing their x values in solution1-3 and returning the
    // number of solutions found.
    size_t findIntersections(double intersectionY, double& solution1, double& solution2, double& solution3) const;

    gfx::CubicBezier m_bezier;
    const double m_x1;
    const double m_y1;
    const double m_x2;
    const double m_y2;
    const EaseType m_easeType;
};

class PLATFORM_EXPORT StepsTimingFunction final : public TimingFunction {
public:
    using StepPosition = cc::StepsTimingFunction::StepPosition;

    static PassRefPtr<StepsTimingFunction> create(int steps, StepPosition stepPosition)
    {
        return adoptRef(new StepsTimingFunction(steps, stepPosition));
    }

    static StepsTimingFunction* preset(StepPosition position)
    {
        DEFINE_STATIC_REF(StepsTimingFunction, start, create(1, StepPosition::START));
        DEFINE_STATIC_REF(StepsTimingFunction, middle, create(1, StepPosition::MIDDLE));
        DEFINE_STATIC_REF(StepsTimingFunction, end, create(1, StepPosition::END));
        switch (position) {
        case StepPosition::START:
            return start;
        case StepPosition::MIDDLE:
            return middle;
        case StepPosition::END:
            return end;
        default:
            NOTREACHED();
            return end;
        }
    }

    ~StepsTimingFunction() override { }

    String toString() const override;

    double evaluate(double fraction, double) const override;
    void range(double* minValue, double* maxValue) const override;

    int numberOfSteps() const { return m_steps; }
    StepPosition getStepPosition() const { return m_stepPosition; }

private:
    StepsTimingFunction(int steps, StepPosition stepPosition)
        : TimingFunction(kStepsFunction)
        , m_steps(steps)
        , m_stepPosition(stepPosition)
    {
    }

    int m_steps;
    StepPosition m_stepPosition;
};

PLATFORM_EXPORT bool operator==(const LinearTimingFunction&, const TimingFunction&);
PLATFORM_EXPORT bool operator==(const CubicBezierTimingFunction&, const TimingFunction&);
PLATFORM_EXPORT bool operator==(const StepsTimingFunction&, const TimingFunction&);

PLATFORM_EXPORT bool operator==(const TimingFunction&, const TimingFunction&);
PLATFORM_EXPORT bool operator!=(const TimingFunction&, const TimingFunction&);

#define DEFINE_TIMING_FUNCTION_TYPE_CASTS(typeName) \
    DEFINE_TYPE_CASTS( \
        typeName##TimingFunction, TimingFunction, value, \
        value->type() == TimingFunction::k##typeName##Function, \
        value.type() == TimingFunction::k##typeName##Function)

DEFINE_TIMING_FUNCTION_TYPE_CASTS(Linear);
DEFINE_TIMING_FUNCTION_TYPE_CASTS(CubicBezier);
DEFINE_TIMING_FUNCTION_TYPE_CASTS(Steps);

} // namespace blink

#endif // TimingFunction_h
