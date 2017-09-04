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

#ifndef SVGPointList_h
#define SVGPointList_h

#include "core/svg/SVGParsingError.h"
#include "core/svg/SVGPoint.h"
#include "core/svg/properties/SVGListPropertyHelper.h"

namespace blink {

class SVGPointListTearOff;

class SVGPointList final
    : public SVGListPropertyHelper<SVGPointList, SVGPoint> {
 public:
  typedef SVGPointListTearOff TearOffType;

  static SVGPointList* create() { return new SVGPointList(); }

  ~SVGPointList() override;

  SVGParsingError setValueAsString(const String&);

  // SVGPropertyBase:
  String valueAsString() const override;

  void add(SVGPropertyBase*, SVGElement*) override;
  void calculateAnimatedValue(SVGAnimationElement*,
                              float percentage,
                              unsigned repeatCount,
                              SVGPropertyBase* fromValue,
                              SVGPropertyBase* toValue,
                              SVGPropertyBase* toAtEndOfDurationValue,
                              SVGElement*) override;
  float calculateDistance(SVGPropertyBase* to, SVGElement*) override;

  static AnimatedPropertyType classType() { return AnimatedPoints; }
  AnimatedPropertyType type() const override { return classType(); }

 private:
  SVGPointList();

  template <typename CharType>
  SVGParsingError parse(const CharType*& ptr, const CharType* end);
};

DEFINE_SVG_PROPERTY_TYPE_CASTS(SVGPointList);

}  // namespace blink

#endif  // SVGPointList_h
