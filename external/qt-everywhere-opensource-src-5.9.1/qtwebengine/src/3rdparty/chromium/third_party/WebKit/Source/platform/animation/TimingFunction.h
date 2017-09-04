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
#include "platform/PlatformExport.h"
#include "wtf/Assertions.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/WTFString.h"

namespace blink {

class PLATFORM_EXPORT TimingFunction : public RefCounted<TimingFunction> {
 public:
  using Type = cc::TimingFunction::Type;

  virtual ~TimingFunction() {}

  Type getType() const { return m_type; }

  virtual String toString() const = 0;

  // Evaluates the timing function at the given fraction. The accuracy parameter
  // provides a hint as to the required accuracy and is not guaranteed.
  virtual double evaluate(double fraction, double accuracy) const = 0;

  // This function returns the minimum and maximum values obtainable when
  // calling evaluate();
  virtual void range(double* minValue, double* maxValue) const = 0;

  // Create CC instance.
  virtual std::unique_ptr<cc::TimingFunction> cloneToCC() const = 0;

 protected:
  TimingFunction(Type type) : m_type(type) {}

 private:
  Type m_type;
};

class PLATFORM_EXPORT LinearTimingFunction final : public TimingFunction {
 public:
  static LinearTimingFunction* shared() {
    DEFINE_STATIC_REF(LinearTimingFunction, linear,
                      (adoptRef(new LinearTimingFunction())));
    return linear;
  }

  ~LinearTimingFunction() override {}

  // TimingFunction implementation.
  String toString() const override;
  double evaluate(double fraction, double) const override;
  void range(double* minValue, double* maxValue) const override;
  std::unique_ptr<cc::TimingFunction> cloneToCC() const override;

 private:
  LinearTimingFunction() : TimingFunction(Type::LINEAR) {}
};

class PLATFORM_EXPORT CubicBezierTimingFunction final : public TimingFunction {
 public:
  using EaseType = cc::CubicBezierTimingFunction::EaseType;

  static PassRefPtr<CubicBezierTimingFunction> create(double x1,
                                                      double y1,
                                                      double x2,
                                                      double y2) {
    return adoptRef(new CubicBezierTimingFunction(x1, y1, x2, y2));
  }

  static CubicBezierTimingFunction* preset(EaseType easeType) {
    DEFINE_STATIC_REF(
        CubicBezierTimingFunction, ease,
        (adoptRef(new CubicBezierTimingFunction(EaseType::EASE))));
    DEFINE_STATIC_REF(
        CubicBezierTimingFunction, easeIn,
        (adoptRef(new CubicBezierTimingFunction(EaseType::EASE_IN))));
    DEFINE_STATIC_REF(
        CubicBezierTimingFunction, easeOut,
        (adoptRef(new CubicBezierTimingFunction(EaseType::EASE_OUT))));
    DEFINE_STATIC_REF(
        CubicBezierTimingFunction, easeInOut,
        (adoptRef(new CubicBezierTimingFunction(EaseType::EASE_IN_OUT))));
    DEFINE_STATIC_REF(
        CubicBezierTimingFunction, easeOutNatural,
        (adoptRef(new CubicBezierTimingFunction(EaseType::EASE_OUT_NATURAL))));

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

  ~CubicBezierTimingFunction() override {}

  // TimingFunction implementation.
  String toString() const override;
  double evaluate(double fraction, double accuracy) const override;
  void range(double* minValue, double* maxValue) const override;
  std::unique_ptr<cc::TimingFunction> cloneToCC() const override;

  double x1() const {
    DCHECK_EQ(getEaseType(), EaseType::CUSTOM);
    return m_x1;
  }
  double y1() const {
    DCHECK_EQ(getEaseType(), EaseType::CUSTOM);
    return m_y1;
  }
  double x2() const {
    DCHECK_EQ(getEaseType(), EaseType::CUSTOM);
    return m_x2;
  }
  double y2() const {
    DCHECK_EQ(getEaseType(), EaseType::CUSTOM);
    return m_y2;
  }
  EaseType getEaseType() const { return m_bezier->ease_type(); }

 private:
  explicit CubicBezierTimingFunction(EaseType easeType)
      : TimingFunction(Type::CUBIC_BEZIER),
        m_bezier(cc::CubicBezierTimingFunction::CreatePreset(easeType)),
        m_x1(),
        m_y1(),
        m_x2(),
        m_y2() {}

  CubicBezierTimingFunction(double x1, double y1, double x2, double y2)
      : TimingFunction(Type::CUBIC_BEZIER),
        m_bezier(cc::CubicBezierTimingFunction::Create(x1, y1, x2, y2)),
        m_x1(x1),
        m_y1(y1),
        m_x2(x2),
        m_y2(y2) {}

  std::unique_ptr<cc::CubicBezierTimingFunction> m_bezier;

  // TODO(loyso): Get these values from m_bezier->bezier_ (gfx::CubicBezier)
  const double m_x1;
  const double m_y1;
  const double m_x2;
  const double m_y2;
};

class PLATFORM_EXPORT StepsTimingFunction final : public TimingFunction {
 public:
  using StepPosition = cc::StepsTimingFunction::StepPosition;

  static PassRefPtr<StepsTimingFunction> create(int steps,
                                                StepPosition stepPosition) {
    return adoptRef(new StepsTimingFunction(steps, stepPosition));
  }

  static StepsTimingFunction* preset(StepPosition position) {
    DEFINE_STATIC_REF(StepsTimingFunction, start,
                      create(1, StepPosition::START));
    DEFINE_STATIC_REF(StepsTimingFunction, middle,
                      create(1, StepPosition::MIDDLE));
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

  ~StepsTimingFunction() override {}

  // TimingFunction implementation.
  String toString() const override;
  double evaluate(double fraction, double) const override;
  void range(double* minValue, double* maxValue) const override;
  std::unique_ptr<cc::TimingFunction> cloneToCC() const override;

  int numberOfSteps() const { return m_steps->steps(); }
  StepPosition getStepPosition() const { return m_steps->step_position(); }

 private:
  StepsTimingFunction(int steps, StepPosition stepPosition)
      : TimingFunction(Type::STEPS),
        m_steps(cc::StepsTimingFunction::Create(steps, stepPosition)) {}

  std::unique_ptr<cc::StepsTimingFunction> m_steps;
};

PLATFORM_EXPORT PassRefPtr<TimingFunction> createCompositorTimingFunctionFromCC(
    const cc::TimingFunction*);

PLATFORM_EXPORT bool operator==(const LinearTimingFunction&,
                                const TimingFunction&);
PLATFORM_EXPORT bool operator==(const CubicBezierTimingFunction&,
                                const TimingFunction&);
PLATFORM_EXPORT bool operator==(const StepsTimingFunction&,
                                const TimingFunction&);

PLATFORM_EXPORT bool operator==(const TimingFunction&, const TimingFunction&);
PLATFORM_EXPORT bool operator!=(const TimingFunction&, const TimingFunction&);

#define DEFINE_TIMING_FUNCTION_TYPE_CASTS(typeName, enumName)           \
  DEFINE_TYPE_CASTS(typeName##TimingFunction, TimingFunction, value,    \
                    value->getType() == TimingFunction::Type::enumName, \
                    value.getType() == TimingFunction::Type::enumName)

DEFINE_TIMING_FUNCTION_TYPE_CASTS(Linear, LINEAR);
DEFINE_TIMING_FUNCTION_TYPE_CASTS(CubicBezier, CUBIC_BEZIER);
DEFINE_TIMING_FUNCTION_TYPE_CASTS(Steps, STEPS);

}  // namespace blink

#endif  // TimingFunction_h
