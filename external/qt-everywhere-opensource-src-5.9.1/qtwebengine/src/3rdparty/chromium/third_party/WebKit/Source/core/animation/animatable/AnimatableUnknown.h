/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AnimatableUnknown_h
#define AnimatableUnknown_h

#include "core/CSSValueKeywords.h"
#include "core/animation/animatable/AnimatableValue.h"
#include "core/css/CSSIdentifierValue.h"

namespace blink {

class AnimatableUnknown final : public AnimatableValue {
 public:
  ~AnimatableUnknown() override {}

  static PassRefPtr<AnimatableUnknown> create(CSSValue* value) {
    return adoptRef(new AnimatableUnknown(value));
  }
  static PassRefPtr<AnimatableUnknown> create(CSSValueID value) {
    return adoptRef(new AnimatableUnknown(CSSIdentifierValue::create(value)));
  }

  CSSValue* toCSSValue() const { return m_value; }
  CSSValueID toCSSValueID() const {
    return toCSSIdentifierValue(m_value.get())->getValueID();
  }

 protected:
  PassRefPtr<AnimatableValue> interpolateTo(const AnimatableValue* value,
                                            double fraction) const override {
    return defaultInterpolateTo(this, value, fraction);
  }

  bool usesDefaultInterpolationWith(const AnimatableValue*) const override;

 private:
  explicit AnimatableUnknown(CSSValue* value) : m_value(value) {
    DCHECK(m_value);
  }
  AnimatableType type() const override { return TypeUnknown; }
  bool equalTo(const AnimatableValue*) const override;

  const Persistent<CSSValue> m_value;
};

DEFINE_ANIMATABLE_VALUE_TYPE_CASTS(AnimatableUnknown, isUnknown());

inline bool AnimatableUnknown::equalTo(const AnimatableValue* value) const {
  const AnimatableUnknown* unknown = toAnimatableUnknown(value);
  return m_value == unknown->m_value || m_value->equals(*unknown->m_value);
}

inline bool AnimatableUnknown::usesDefaultInterpolationWith(
    const AnimatableValue* value) const {
  const AnimatableUnknown& unknown = toAnimatableUnknown(*value);
  return !m_value->equals(*unknown.m_value);
}

}  // namespace blink

#endif  // AnimatableUnknown_h
