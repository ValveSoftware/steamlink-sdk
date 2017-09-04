/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef SVGString_h
#define SVGString_h

#include "core/svg/SVGParsingError.h"
#include "core/svg/properties/SVGProperty.h"

namespace blink {

class SVGString final : public SVGPropertyBase {
 public:
  // SVGString does not have a tear-off type.
  typedef void TearOffType;
  typedef String PrimitiveType;

  static SVGString* create() { return new SVGString(); }

  static SVGString* create(const String& value) { return new SVGString(value); }

  SVGString* clone() const { return create(m_value); }
  SVGPropertyBase* cloneForAnimation(const String& value) const override {
    return create(value);
  }

  String valueAsString() const override { return m_value; }
  SVGParsingError setValueAsString(const String& value) {
    m_value = value;
    return SVGParseStatus::NoError;
  }

  void add(SVGPropertyBase*, SVGElement*) override;
  void calculateAnimatedValue(SVGAnimationElement*,
                              float percentage,
                              unsigned repeatCount,
                              SVGPropertyBase* from,
                              SVGPropertyBase* to,
                              SVGPropertyBase* toAtEndOfDurationValue,
                              SVGElement*) override;
  float calculateDistance(SVGPropertyBase* to, SVGElement*) override;

  const String& value() const { return m_value; }
  void setValue(const String& value) { m_value = value; }

  static AnimatedPropertyType classType() { return AnimatedString; }
  AnimatedPropertyType type() const override { return classType(); }

 private:
  SVGString() {}
  explicit SVGString(const String& value) : m_value(value) {}

  String m_value;
};

DEFINE_SVG_PROPERTY_TYPE_CASTS(SVGString);

}  // namespace blink

#endif  // SVGString_h
