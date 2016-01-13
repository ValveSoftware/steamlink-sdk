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

#ifndef SVGNumber_h
#define SVGNumber_h

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/svg/properties/SVGProperty.h"

namespace WebCore {

class SVGNumberTearOff;

class SVGNumber : public SVGPropertyBase {
public:
    // SVGNumber has a tear-off type, but SVGAnimatedNumber uses primitive type.
    typedef SVGNumberTearOff TearOffType;
    typedef float PrimitiveType;

    static PassRefPtr<SVGNumber> create(float value = 0.0f)
    {
        return adoptRef(new SVGNumber(value));
    }

    virtual PassRefPtr<SVGNumber> clone() const;
    virtual PassRefPtr<SVGPropertyBase> cloneForAnimation(const String&) const OVERRIDE;

    float value() const { return m_value; }
    void setValue(float value) { m_value = value; }

    virtual String valueAsString() const OVERRIDE;
    virtual void setValueAsString(const String&, ExceptionState&);

    virtual void add(PassRefPtrWillBeRawPtr<SVGPropertyBase>, SVGElement*) OVERRIDE;
    virtual void calculateAnimatedValue(SVGAnimationElement*, float percentage, unsigned repeatCount, PassRefPtr<SVGPropertyBase> from, PassRefPtr<SVGPropertyBase> to, PassRefPtr<SVGPropertyBase> toAtEndOfDurationValue, SVGElement* contextElement) OVERRIDE;
    virtual float calculateDistance(PassRefPtr<SVGPropertyBase> to, SVGElement* contextElement) OVERRIDE;

    static AnimatedPropertyType classType() { return AnimatedNumber; }

protected:
    explicit SVGNumber(float);

    template<typename CharType>
    bool parse(const CharType*& ptr, const CharType* end);

    float m_value;
};

inline PassRefPtr<SVGNumber> toSVGNumber(PassRefPtr<SVGPropertyBase> passBase)
{
    RefPtr<SVGPropertyBase> base = passBase;
    ASSERT(base->type() == SVGNumber::classType());
    return static_pointer_cast<SVGNumber>(base.release());
}

// SVGNumber which also accepts percentage as its value.
// This is used for <stop> "offset"
// Spec: http://www.w3.org/TR/SVG11/pservers.html#GradientStops
//   offset = "<number> | <percentage>"
class SVGNumberAcceptPercentage FINAL : public SVGNumber {
public:
    static PassRefPtr<SVGNumberAcceptPercentage> create(float value = 0)
    {
        return adoptRef(new SVGNumberAcceptPercentage(value));
    }

    virtual PassRefPtr<SVGNumber> clone() const OVERRIDE;
    virtual void setValueAsString(const String&, ExceptionState&) OVERRIDE;

private:
    SVGNumberAcceptPercentage(float value);
};

} // namespace WebCore

#endif // SVGNumber_h
