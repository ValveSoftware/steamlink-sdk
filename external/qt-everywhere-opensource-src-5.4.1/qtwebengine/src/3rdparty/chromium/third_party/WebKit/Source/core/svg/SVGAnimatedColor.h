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

#ifndef SVGAnimatedColor_h
#define SVGAnimatedColor_h

#include "core/css/StyleColor.h"
#include "core/svg/properties/SVGAnimatedProperty.h"

namespace WebCore {

class SVGAnimationElement;

// StyleColor adaptor to SVGPropertyBase. This is only used for SMIL animations.
// FIXME: WebAnimations: Replacable with AnimatableColor once SMIL animations are implemented in WebAnimations.
class SVGColorProperty FINAL : public SVGPropertyBase {
public:
    static PassRefPtr<SVGColorProperty> create(StyleColor styleColor)
    {
        return adoptRef(new SVGColorProperty(styleColor));
    }

    virtual PassRefPtr<SVGPropertyBase> cloneForAnimation(const String&) const OVERRIDE;
    virtual String valueAsString() const OVERRIDE;

    virtual void add(PassRefPtrWillBeRawPtr<SVGPropertyBase>, SVGElement*) OVERRIDE;
    virtual void calculateAnimatedValue(SVGAnimationElement*, float percentage, unsigned repeatCount, PassRefPtr<SVGPropertyBase> from, PassRefPtr<SVGPropertyBase> to, PassRefPtr<SVGPropertyBase> toAtEndOfDurationValue, SVGElement*) OVERRIDE;
    virtual float calculateDistance(PassRefPtr<SVGPropertyBase> to, SVGElement*) OVERRIDE;

    static AnimatedPropertyType classType() { return AnimatedColor; }

private:
    explicit SVGColorProperty(StyleColor styleColor)
        : SVGPropertyBase(classType())
        , m_styleColor(styleColor)
    {
    }

    StyleColor m_styleColor;
};

inline PassRefPtr<SVGColorProperty> toSVGColorProperty(PassRefPtr<SVGPropertyBase> passBase)
{
    RefPtr<SVGPropertyBase> base = passBase;
    ASSERT(base->type() == SVGColorProperty::classType());
    return static_pointer_cast<SVGColorProperty>(base.release());
}

} // namespace WebCore

#endif
