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

#include "config.h"

#include "core/svg/SVGFitToViewBox.h"

#include "core/dom/Attribute.h"
#include "core/dom/Document.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGParserUtilities.h"
#include "platform/geometry/FloatRect.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/text/StringImpl.h"

namespace WebCore {

SVGFitToViewBox::SVGFitToViewBox(SVGElement* element, PropertyMapPolicy propertyMapPolicy)
    : m_viewBox(SVGAnimatedRect::create(element, SVGNames::viewBoxAttr))
    , m_preserveAspectRatio(SVGAnimatedPreserveAspectRatio::create(element, SVGNames::preserveAspectRatioAttr, SVGPreserveAspectRatio::create()))
{
    ASSERT(element);
    if (propertyMapPolicy == PropertyMapPolicyAdd) {
        element->addToPropertyMap(m_viewBox);
        element->addToPropertyMap(m_preserveAspectRatio);
    }
}

AffineTransform SVGFitToViewBox::viewBoxToViewTransform(const FloatRect& viewBoxRect, PassRefPtr<SVGPreserveAspectRatio> preserveAspectRatio, float viewWidth, float viewHeight)
{
    if (!viewBoxRect.width() || !viewBoxRect.height())
        return AffineTransform();

    return preserveAspectRatio->getCTM(viewBoxRect.x(), viewBoxRect.y(), viewBoxRect.width(), viewBoxRect.height(), viewWidth, viewHeight);
}

bool SVGFitToViewBox::isKnownAttribute(const QualifiedName& attrName)
{
    return attrName == SVGNames::viewBoxAttr || attrName == SVGNames::preserveAspectRatioAttr;
}

void SVGFitToViewBox::addSupportedAttributes(HashSet<QualifiedName>& supportedAttributes)
{
    supportedAttributes.add(SVGNames::viewBoxAttr);
    supportedAttributes.add(SVGNames::preserveAspectRatioAttr);
}

void SVGFitToViewBox::updateViewBox(const FloatRect& rect)
{
    ASSERT(m_viewBox);
    m_viewBox->baseValue()->setValue(rect);
}

}
