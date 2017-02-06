/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Rob Buis <buis@kde.org>
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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

#include "core/svg/SVGFitToViewBox.h"

#include "core/dom/Attribute.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGParserUtilities.h"
#include "platform/geometry/FloatRect.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/text/StringImpl.h"

namespace blink {

class SVGAnimatedViewBoxRect : public SVGAnimatedRect {
public:
    static SVGAnimatedRect* create(SVGElement* contextElement)
    {
        return new SVGAnimatedViewBoxRect(contextElement);
    }

    SVGParsingError setBaseValueAsString(const String&) override;

protected:
    SVGAnimatedViewBoxRect(SVGElement* contextElement)
        : SVGAnimatedRect(contextElement, SVGNames::viewBoxAttr)
    {
    }
};

SVGParsingError SVGAnimatedViewBoxRect::setBaseValueAsString(const String& value)
{
    SVGParsingError parseStatus = SVGAnimatedRect::setBaseValueAsString(value);

    if (parseStatus == SVGParseStatus::NoError && (baseValue()->width() < 0 || baseValue()->height() < 0)) {
        parseStatus = SVGParseStatus::NegativeValue;
        baseValue()->setInvalid();
    }
    return parseStatus;
}

SVGFitToViewBox::SVGFitToViewBox(SVGElement* element, PropertyMapPolicy propertyMapPolicy)
    : m_viewBox(SVGAnimatedViewBoxRect::create(element))
    , m_preserveAspectRatio(SVGAnimatedPreserveAspectRatio::create(element, SVGNames::preserveAspectRatioAttr, SVGPreserveAspectRatio::create()))
{
    ASSERT(element);
    if (propertyMapPolicy == PropertyMapPolicyAdd) {
        element->addToPropertyMap(m_viewBox);
        element->addToPropertyMap(m_preserveAspectRatio);
    }
}

DEFINE_TRACE(SVGFitToViewBox)
{
    visitor->trace(m_viewBox);
    visitor->trace(m_preserveAspectRatio);
}

AffineTransform SVGFitToViewBox::viewBoxToViewTransform(const FloatRect& viewBoxRect, SVGPreserveAspectRatio* preserveAspectRatio, float viewWidth, float viewHeight)
{
    if (!viewBoxRect.width() || !viewBoxRect.height() || !viewWidth || !viewHeight)
        return AffineTransform();

    return preserveAspectRatio->getCTM(viewBoxRect.x(), viewBoxRect.y(), viewBoxRect.width(), viewBoxRect.height(), viewWidth, viewHeight);
}

bool SVGFitToViewBox::isKnownAttribute(const QualifiedName& attrName)
{
    return attrName == SVGNames::viewBoxAttr || attrName == SVGNames::preserveAspectRatioAttr;
}

void SVGFitToViewBox::updateViewBox(const FloatRect& rect)
{
    ASSERT(m_viewBox);
    m_viewBox->baseValue()->setValue(rect);
}

} // namespace blink
