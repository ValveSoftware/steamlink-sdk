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
#include "platform/heap/Handle.h"

namespace blink {

template<> const SVGEnumerationStringEntries& getStaticStringEntries<EdgeModeType>();

class SVGFEConvolveMatrixElement final : public SVGFilterPrimitiveStandardAttributes {
    DEFINE_WRAPPERTYPEINFO();
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

    DECLARE_VIRTUAL_TRACE();

private:
    explicit SVGFEConvolveMatrixElement(Document&);

    bool setFilterEffectAttribute(FilterEffect*, const QualifiedName&) override;
    void svgAttributeChanged(const QualifiedName&) override;
    FilterEffect* build(SVGFilterBuilder*, Filter*) override;

    Member<SVGAnimatedNumber> m_bias;
    Member<SVGAnimatedNumber> m_divisor;
    Member<SVGAnimatedString> m_in1;
    Member<SVGAnimatedEnumeration<EdgeModeType>> m_edgeMode;
    Member<SVGAnimatedNumberList> m_kernelMatrix;
    Member<SVGAnimatedNumberOptionalNumber> m_kernelUnitLength;
    Member<SVGAnimatedIntegerOptionalInteger> m_order;
    Member<SVGAnimatedBoolean> m_preserveAlpha;
    Member<SVGAnimatedInteger> m_targetX;
    Member<SVGAnimatedInteger> m_targetY;
};

} // namespace blink

#endif // SVGFEConvolveMatrixElement_h
