// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/InterpolableValue.h"

#include <memory>

namespace blink {

bool InterpolableNumber::equals(const InterpolableValue& other) const {
  return m_value == toInterpolableNumber(other).m_value;
}

bool InterpolableList::equals(const InterpolableValue& other) const {
  const InterpolableList& otherList = toInterpolableList(other);
  if (length() != otherList.length())
    return false;
  for (size_t i = 0; i < length(); i++) {
    if (!m_values[i]->equals(*otherList.m_values[i]))
      return false;
  }
  return true;
}

void InterpolableNumber::interpolate(const InterpolableValue& to,
                                     const double progress,
                                     InterpolableValue& result) const {
  const InterpolableNumber& toNumber = toInterpolableNumber(to);
  InterpolableNumber& resultNumber = toInterpolableNumber(result);

  if (progress == 0 || m_value == toNumber.m_value)
    resultNumber.m_value = m_value;
  else if (progress == 1)
    resultNumber.m_value = toNumber.m_value;
  else
    resultNumber.m_value =
        m_value * (1 - progress) + toNumber.m_value * progress;
}

void InterpolableList::interpolate(const InterpolableValue& to,
                                   const double progress,
                                   InterpolableValue& result) const {
  const InterpolableList& toList = toInterpolableList(to);
  InterpolableList& resultList = toInterpolableList(result);

  DCHECK_EQ(toList.length(), length());
  DCHECK_EQ(resultList.length(), length());

  for (size_t i = 0; i < length(); i++) {
    DCHECK(m_values[i]);
    DCHECK(toList.m_values[i]);
    m_values[i]->interpolate(*(toList.m_values[i]), progress,
                             *(resultList.m_values[i]));
  }
}

std::unique_ptr<InterpolableValue> InterpolableList::cloneAndZero() const {
  std::unique_ptr<InterpolableList> result = InterpolableList::create(length());
  for (size_t i = 0; i < length(); i++)
    result->set(i, m_values[i]->cloneAndZero());
  return std::move(result);
}

void InterpolableNumber::scale(double scale) {
  m_value = m_value * scale;
}

void InterpolableList::scale(double scale) {
  for (size_t i = 0; i < length(); i++)
    m_values[i]->scale(scale);
}

void InterpolableNumber::scaleAndAdd(double scale,
                                     const InterpolableValue& other) {
  m_value = m_value * scale + toInterpolableNumber(other).m_value;
}

void InterpolableList::scaleAndAdd(double scale,
                                   const InterpolableValue& other) {
  const InterpolableList& otherList = toInterpolableList(other);
  DCHECK_EQ(otherList.length(), length());
  for (size_t i = 0; i < length(); i++)
    m_values[i]->scaleAndAdd(scale, *otherList.m_values[i]);
}

void InterpolableAnimatableValue::interpolate(const InterpolableValue& to,
                                              const double progress,
                                              InterpolableValue& result) const {
  const InterpolableAnimatableValue& toValue =
      toInterpolableAnimatableValue(to);
  InterpolableAnimatableValue& resultValue =
      toInterpolableAnimatableValue(result);
  if (progress == 0)
    resultValue.m_value = m_value;
  if (progress == 1)
    resultValue.m_value = toValue.m_value;
  resultValue.m_value = AnimatableValue::interpolate(
      m_value.get(), toValue.m_value.get(), progress);
}

}  // namespace blink
