/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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
 */

#ifndef SVGFEDropShadowElement_h
#define SVGFEDropShadowElement_h

#include "core/svg/SVGAnimatedNumber.h"
#include "core/svg/SVGAnimatedNumberOptionalNumber.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"
#include "platform/graphics/filters/FEDropShadow.h"

namespace WebCore {

class SVGFEDropShadowElement FINAL : public SVGFilterPrimitiveStandardAttributes {
public:
    DECLARE_NODE_FACTORY(SVGFEDropShadowElement);

    void setStdDeviation(float stdDeviationX, float stdDeviationY);

    SVGAnimatedNumber* dx() { return m_dx.get(); }
    SVGAnimatedNumber* dy() { return m_dy.get(); }
    SVGAnimatedNumber* stdDeviationX() { return m_stdDeviation->firstNumber(); }
    SVGAnimatedNumber* stdDeviationY() { return m_stdDeviation->secondNumber(); }
    SVGAnimatedString* in1() { return m_in1.get(); }

private:
    explicit SVGFEDropShadowElement(Document&);

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE;
    virtual PassRefPtr<FilterEffect> build(SVGFilterBuilder*, Filter*) OVERRIDE;

    static const AtomicString& stdDeviationXIdentifier();
    static const AtomicString& stdDeviationYIdentifier();

    RefPtr<SVGAnimatedNumber> m_dx;
    RefPtr<SVGAnimatedNumber> m_dy;
    RefPtr<SVGAnimatedNumberOptionalNumber> m_stdDeviation;
    RefPtr<SVGAnimatedString> m_in1;
};

} // namespace WebCore

#endif
