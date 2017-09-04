// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/TimingFunction.h"

#include "wtf/text/StringBuilder.h"

namespace blink {

String LinearTimingFunction::toString() const {
  return "linear";
}

double LinearTimingFunction::evaluate(double fraction, double) const {
  return fraction;
}

void LinearTimingFunction::range(double* minValue, double* maxValue) const {}

std::unique_ptr<cc::TimingFunction> LinearTimingFunction::cloneToCC() const {
  return nullptr;
}

String CubicBezierTimingFunction::toString() const {
  switch (this->getEaseType()) {
    case CubicBezierTimingFunction::EaseType::EASE:
      return "ease";
    case CubicBezierTimingFunction::EaseType::EASE_IN:
      return "ease-in";
    case CubicBezierTimingFunction::EaseType::EASE_OUT:
      return "ease-out";
    case CubicBezierTimingFunction::EaseType::EASE_IN_OUT:
      return "ease-in-out";
    case CubicBezierTimingFunction::EaseType::CUSTOM:
      return "cubic-bezier(" + String::numberToStringECMAScript(this->x1()) +
             ", " + String::numberToStringECMAScript(this->y1()) + ", " +
             String::numberToStringECMAScript(this->x2()) + ", " +
             String::numberToStringECMAScript(this->y2()) + ")";
    default:
      NOTREACHED();
      return "";
  }
}

double CubicBezierTimingFunction::evaluate(double fraction,
                                           double accuracy) const {
  return m_bezier->bezier().SolveWithEpsilon(fraction, accuracy);
}

void CubicBezierTimingFunction::range(double* minValue,
                                      double* maxValue) const {
  const double solution1 = m_bezier->bezier().range_min();
  const double solution2 = m_bezier->bezier().range_max();

  // Since our input values can be out of the range 0->1 so we must also
  // consider the minimum and maximum points.
  double solutionMin = m_bezier->bezier().SolveWithEpsilon(
      *minValue, std::numeric_limits<double>::epsilon());
  double solutionMax = m_bezier->bezier().SolveWithEpsilon(
      *maxValue, std::numeric_limits<double>::epsilon());
  *minValue = std::min(std::min(solutionMin, solutionMax), 0.0);
  *maxValue = std::max(std::max(solutionMin, solutionMax), 1.0);
  *minValue = std::min(std::min(*minValue, solution1), solution2);
  *maxValue = std::max(std::max(*maxValue, solution1), solution2);
}

std::unique_ptr<cc::TimingFunction> CubicBezierTimingFunction::cloneToCC()
    const {
  return m_bezier->Clone();
}

String StepsTimingFunction::toString() const {
  const char* positionString = nullptr;
  switch (getStepPosition()) {
    case StepPosition::START:
      positionString = "start";
      break;
    case StepPosition::MIDDLE:
      positionString = "middle";
      break;
    case StepPosition::END:
      // do not specify step position in output
      break;
  }

  StringBuilder builder;
  builder.append("steps(");
  builder.append(String::numberToStringECMAScript(this->numberOfSteps()));
  if (positionString) {
    builder.append(", ");
    builder.append(positionString);
  }
  builder.append(')');
  return builder.toString();
}

void StepsTimingFunction::range(double* minValue, double* maxValue) const {
  *minValue = 0;
  *maxValue = 1;
}

double StepsTimingFunction::evaluate(double fraction, double) const {
  return m_steps->GetPreciseValue(fraction);
}

std::unique_ptr<cc::TimingFunction> StepsTimingFunction::cloneToCC() const {
  return m_steps->Clone();
}

PassRefPtr<TimingFunction> createCompositorTimingFunctionFromCC(
    const cc::TimingFunction* timingFunction) {
  if (!timingFunction)
    return LinearTimingFunction::shared();

  switch (timingFunction->GetType()) {
    case cc::TimingFunction::Type::CUBIC_BEZIER: {
      auto cubicTimingFunction =
          static_cast<const cc::CubicBezierTimingFunction*>(timingFunction);
      if (cubicTimingFunction->ease_type() !=
          cc::CubicBezierTimingFunction::EaseType::CUSTOM)
        return CubicBezierTimingFunction::preset(
            cubicTimingFunction->ease_type());

      const auto& bezier = cubicTimingFunction->bezier();
      return CubicBezierTimingFunction::create(bezier.GetX1(), bezier.GetY1(),
                                               bezier.GetX2(), bezier.GetY2());
    }

    case cc::TimingFunction::Type::STEPS: {
      auto stepsTimingFunction =
          static_cast<const cc::StepsTimingFunction*>(timingFunction);
      return StepsTimingFunction::create(stepsTimingFunction->steps(),
                                         stepsTimingFunction->step_position());
    }

    default:
      NOTREACHED();
      return nullptr;
  }
}

// Equals operators
bool operator==(const LinearTimingFunction& lhs, const TimingFunction& rhs) {
  return rhs.getType() == TimingFunction::Type::LINEAR;
}

bool operator==(const CubicBezierTimingFunction& lhs,
                const TimingFunction& rhs) {
  if (rhs.getType() != TimingFunction::Type::CUBIC_BEZIER)
    return false;

  const CubicBezierTimingFunction& ctf = toCubicBezierTimingFunction(rhs);
  if ((lhs.getEaseType() == CubicBezierTimingFunction::EaseType::CUSTOM) &&
      (ctf.getEaseType() == CubicBezierTimingFunction::EaseType::CUSTOM))
    return (lhs.x1() == ctf.x1()) && (lhs.y1() == ctf.y1()) &&
           (lhs.x2() == ctf.x2()) && (lhs.y2() == ctf.y2());

  return lhs.getEaseType() == ctf.getEaseType();
}

bool operator==(const StepsTimingFunction& lhs, const TimingFunction& rhs) {
  if (rhs.getType() != TimingFunction::Type::STEPS)
    return false;

  const StepsTimingFunction& stf = toStepsTimingFunction(rhs);
  return (lhs.numberOfSteps() == stf.numberOfSteps()) &&
         (lhs.getStepPosition() == stf.getStepPosition());
}

// The generic operator== *must* come after the
// non-generic operator== otherwise it will end up calling itself.
bool operator==(const TimingFunction& lhs, const TimingFunction& rhs) {
  switch (lhs.getType()) {
    case TimingFunction::Type::LINEAR: {
      const LinearTimingFunction& linear = toLinearTimingFunction(lhs);
      return (linear == rhs);
    }
    case TimingFunction::Type::CUBIC_BEZIER: {
      const CubicBezierTimingFunction& cubic = toCubicBezierTimingFunction(lhs);
      return (cubic == rhs);
    }
    case TimingFunction::Type::STEPS: {
      const StepsTimingFunction& step = toStepsTimingFunction(lhs);
      return (step == rhs);
    }
    default:
      ASSERT_NOT_REACHED();
  }
  return false;
}

// No need to define specific operator!= as they can all come via this function.
bool operator!=(const TimingFunction& lhs, const TimingFunction& rhs) {
  return !(lhs == rhs);
}

}  // namespace blink
