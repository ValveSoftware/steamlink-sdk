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

#ifndef SVGPoint_h
#define SVGPoint_h

#include "core/svg/SVGParsingError.h"
#include "core/svg/properties/SVGPropertyHelper.h"
#include "platform/geometry/FloatPoint.h"

namespace blink {

class AffineTransform;
class SVGPointTearOff;

class SVGPoint final : public SVGPropertyHelper<SVGPoint> {
public:
    typedef SVGPointTearOff TearOffType;

    static SVGPoint* create()
    {
        return new SVGPoint();
    }

    static SVGPoint* create(const FloatPoint& point)
    {
        return new SVGPoint(point);
    }

    SVGPoint* clone() const;

    const FloatPoint& value() const { return m_value; }
    void setValue(const FloatPoint& value) { m_value = value; }

    float x() const { return m_value.x(); }
    float y() const { return m_value.y(); }
    void setX(float f) { m_value.setX(f); }
    void setY(float f) { m_value.setY(f); }

    FloatPoint matrixTransform(const AffineTransform&) const;

    String valueAsString() const override;
    SVGParsingError setValueAsString(const String&);

    void add(SVGPropertyBase*, SVGElement*) override;
    void calculateAnimatedValue(SVGAnimationElement*, float percentage, unsigned repeatCount, SVGPropertyBase* from, SVGPropertyBase* to, SVGPropertyBase* toAtEndOfDurationValue, SVGElement* contextElement) override;
    float calculateDistance(SVGPropertyBase* to, SVGElement* contextElement) override;

    static AnimatedPropertyType classType() { return AnimatedPoint; }

private:
    SVGPoint();
    explicit SVGPoint(const FloatPoint&);

    template<typename CharType>
    SVGParsingError parse(const CharType*& ptr, const CharType* end);

    FloatPoint m_value;
};

DEFINE_SVG_PROPERTY_TYPE_CASTS(SVGPoint);

} // namespace blink

#endif // SVGPoint_h
