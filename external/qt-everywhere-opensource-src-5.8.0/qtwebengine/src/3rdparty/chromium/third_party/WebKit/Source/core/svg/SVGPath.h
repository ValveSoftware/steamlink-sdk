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

#ifndef SVGPath_h
#define SVGPath_h

#include "core/css/CSSPathValue.h"
#include "core/svg/SVGParsingError.h"
#include "core/svg/SVGPathByteStream.h"
#include "core/svg/properties/SVGProperty.h"

namespace blink {

class SVGPath final : public SVGPropertyBase {
public:
    typedef void TearOffType;

    static SVGPath* create()
    {
        return new SVGPath();
    }
    static SVGPath* create(CSSPathValue* pathValue)
    {
        return new SVGPath(pathValue);
    }

    ~SVGPath() override;

    const SVGPathByteStream& byteStream() const { return m_pathValue->byteStream(); }
    StylePath* stylePath() const { return m_pathValue->stylePath(); }
    CSSPathValue* pathValue() const { return m_pathValue.get(); }

    // SVGPropertyBase:
    SVGPath* clone() const;
    SVGPropertyBase* cloneForAnimation(const String&) const override;
    String valueAsString() const override;
    SVGParsingError setValueAsString(const String&);

    void add(SVGPropertyBase*, SVGElement*) override;
    void calculateAnimatedValue(SVGAnimationElement*, float percentage, unsigned repeatCount, SVGPropertyBase* fromValue, SVGPropertyBase* toValue, SVGPropertyBase* toAtEndOfDurationValue, SVGElement*) override;
    float calculateDistance(SVGPropertyBase* to, SVGElement*) override;

    static AnimatedPropertyType classType() { return AnimatedPath; }
    AnimatedPropertyType type() const override { return classType(); }

    DECLARE_VIRTUAL_TRACE();

private:
    SVGPath();
    explicit SVGPath(CSSPathValue*);

    Member<CSSPathValue> m_pathValue;
};

DEFINE_SVG_PROPERTY_TYPE_CASTS(SVGPath);

} // namespace blink

#endif
