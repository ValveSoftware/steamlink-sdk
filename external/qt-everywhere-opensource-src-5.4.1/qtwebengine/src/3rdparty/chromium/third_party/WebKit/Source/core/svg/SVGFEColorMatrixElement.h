/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#ifndef SVGFEColorMatrixElement_h
#define SVGFEColorMatrixElement_h

#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedNumberList.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"
#include "platform/graphics/filters/FEColorMatrix.h"

namespace WebCore {

template<> const SVGEnumerationStringEntries& getStaticStringEntries<ColorMatrixType>();

class SVGFEColorMatrixElement FINAL : public SVGFilterPrimitiveStandardAttributes {
public:
    DECLARE_NODE_FACTORY(SVGFEColorMatrixElement);

    SVGAnimatedNumberList* values() { return m_values.get(); }
    SVGAnimatedString* in1() { return m_in1.get(); }
    SVGAnimatedEnumeration<ColorMatrixType>* type() { return m_type.get(); }

private:
    explicit SVGFEColorMatrixElement(Document&);

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual bool setFilterEffectAttribute(FilterEffect*, const QualifiedName&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE;
    virtual PassRefPtr<FilterEffect> build(SVGFilterBuilder*, Filter*) OVERRIDE;

    RefPtr<SVGAnimatedNumberList> m_values;
    RefPtr<SVGAnimatedString> m_in1;
    RefPtr<SVGAnimatedEnumeration<ColorMatrixType> > m_type;
};

} // namespace WebCore

#endif
