/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Oliver Hunt <oliver@nerget.com>
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

#ifndef SVGFESpecularLightingElement_h
#define SVGFESpecularLightingElement_h

#include "core/SVGNames.h"
#include "core/svg/SVGAnimatedNumber.h"
#include "core/svg/SVGAnimatedNumberOptionalNumber.h"
#include "core/svg/SVGFELightElement.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"
#include "platform/graphics/filters/FESpecularLighting.h"

namespace WebCore {

class SVGFESpecularLightingElement FINAL : public SVGFilterPrimitiveStandardAttributes {
public:
    DECLARE_NODE_FACTORY(SVGFESpecularLightingElement);
    void lightElementAttributeChanged(const SVGFELightElement*, const QualifiedName&);

    SVGAnimatedNumber* specularConstant() { return m_specularConstant.get(); }
    SVGAnimatedNumber* specularExponent() { return m_specularExponent.get(); }
    SVGAnimatedNumber* surfaceScale() { return m_surfaceScale.get(); }
    SVGAnimatedNumber* kernelUnitLengthX() { return m_kernelUnitLength->firstNumber(); }
    SVGAnimatedNumber* kernelUnitLengthY() { return m_kernelUnitLength->secondNumber(); }
    SVGAnimatedString* in1() { return m_in1.get(); }
private:
    explicit SVGFESpecularLightingElement(Document&);

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual bool setFilterEffectAttribute(FilterEffect*, const QualifiedName&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE;
    virtual PassRefPtr<FilterEffect> build(SVGFilterBuilder*, Filter*) OVERRIDE;

    static const AtomicString& kernelUnitLengthXIdentifier();
    static const AtomicString& kernelUnitLengthYIdentifier();

    RefPtr<SVGAnimatedNumber> m_specularConstant;
    RefPtr<SVGAnimatedNumber> m_specularExponent;
    RefPtr<SVGAnimatedNumber> m_surfaceScale;
    RefPtr<SVGAnimatedNumberOptionalNumber> m_kernelUnitLength;
    RefPtr<SVGAnimatedString> m_in1;
};

} // namespace WebCore

#endif
