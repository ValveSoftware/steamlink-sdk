/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#ifndef SVGFEConvolveMatrixElement_h
#define SVGFEConvolveMatrixElement_h

#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedInteger.h"
#include "core/svg/SVGAnimatedIntegerOptionalInteger.h"
#include "core/svg/SVGAnimatedNumber.h"
#include "core/svg/SVGAnimatedNumberList.h"
#include "core/svg/SVGAnimatedNumberOptionalNumber.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"
#include "platform/graphics/filters/FEConvolveMatrix.h"

namespace WebCore {

template<> const SVGEnumerationStringEntries& getStaticStringEntries<EdgeModeType>();

class SVGFEConvolveMatrixElement FINAL : public SVGFilterPrimitiveStandardAttributes {
public:
    DECLARE_NODE_FACTORY(SVGFEConvolveMatrixElement);

    SVGAnimatedBoolean* preserveAlpha() { return m_preserveAlpha.get(); }
    SVGAnimatedNumber* divisor() { return m_divisor.get(); }
    SVGAnimatedNumber* bias() { return m_bias.get(); }
    SVGAnimatedNumber* kernelUnitLengthX() { return m_kernelUnitLength->firstNumber(); }
    SVGAnimatedNumber* kernelUnitLengthY() { return m_kernelUnitLength->secondNumber(); }
    SVGAnimatedNumberList* kernelMatrix() { return m_kernelMatrix.get(); }
    SVGAnimatedString* in1() { return m_in1.get(); }
    SVGAnimatedEnumeration<EdgeModeType>* edgeMode() { return m_edgeMode.get(); }
    SVGAnimatedInteger* orderX() { return m_order->firstInteger(); }
    SVGAnimatedInteger* orderY() { return m_order->secondInteger(); }
    SVGAnimatedInteger* targetX() { return m_targetX.get(); }
    SVGAnimatedInteger* targetY() { return m_targetY.get(); }

private:
    explicit SVGFEConvolveMatrixElement(Document&);

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual bool setFilterEffectAttribute(FilterEffect*, const QualifiedName&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE;
    virtual PassRefPtr<FilterEffect> build(SVGFilterBuilder*, Filter*) OVERRIDE;

    RefPtr<SVGAnimatedNumber> m_bias;
    RefPtr<SVGAnimatedNumber> m_divisor;
    RefPtr<SVGAnimatedString> m_in1;
    RefPtr<SVGAnimatedEnumeration<EdgeModeType> > m_edgeMode;
    RefPtr<SVGAnimatedNumberList> m_kernelMatrix;
    RefPtr<SVGAnimatedNumberOptionalNumber> m_kernelUnitLength;
    RefPtr<SVGAnimatedIntegerOptionalInteger> m_order;
    RefPtr<SVGAnimatedBoolean> m_preserveAlpha;
    RefPtr<SVGAnimatedInteger> m_targetX;
    RefPtr<SVGAnimatedInteger> m_targetY;
};

} // namespace WebCore

#endif
