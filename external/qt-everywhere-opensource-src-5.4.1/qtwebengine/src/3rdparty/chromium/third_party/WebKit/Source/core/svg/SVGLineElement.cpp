/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGLineElement.h"

#include "core/rendering/svg/RenderSVGResource.h"
#include "core/svg/SVGLength.h"

namespace WebCore {

inline SVGLineElement::SVGLineElement(Document& document)
    : SVGGeometryElement(SVGNames::lineTag, document)
    , m_x1(SVGAnimatedLength::create(this, SVGNames::x1Attr, SVGLength::create(LengthModeWidth), AllowNegativeLengths))
    , m_y1(SVGAnimatedLength::create(this, SVGNames::y1Attr, SVGLength::create(LengthModeHeight), AllowNegativeLengths))
    , m_x2(SVGAnimatedLength::create(this, SVGNames::x2Attr, SVGLength::create(LengthModeWidth), AllowNegativeLengths))
    , m_y2(SVGAnimatedLength::create(this, SVGNames::y2Attr, SVGLength::create(LengthModeHeight), AllowNegativeLengths))
{
    ScriptWrappable::init(this);

    addToPropertyMap(m_x1);
    addToPropertyMap(m_y1);
    addToPropertyMap(m_x2);
    addToPropertyMap(m_y2);
}

DEFINE_NODE_FACTORY(SVGLineElement)

bool SVGLineElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        supportedAttributes.add(SVGNames::x1Attr);
        supportedAttributes.add(SVGNames::x2Attr);
        supportedAttributes.add(SVGNames::y1Attr);
        supportedAttributes.add(SVGNames::y2Attr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGLineElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    SVGParsingError parseError = NoError;

    if (!isSupportedAttribute(name))
        SVGGeometryElement::parseAttribute(name, value);
    else if (name == SVGNames::x1Attr)
        m_x1->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::y1Attr)
        m_y1->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::x2Attr)
        m_x2->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::y2Attr)
        m_y2->setBaseValueAsString(value, parseError);
    else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, name, value);
}

void SVGLineElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGGeometryElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElement::InvalidationGuard invalidationGuard(this);

    bool isLengthAttribute = attrName == SVGNames::x1Attr
                          || attrName == SVGNames::y1Attr
                          || attrName == SVGNames::x2Attr
                          || attrName == SVGNames::y2Attr;

    if (isLengthAttribute)
        updateRelativeLengthsInformation();

    RenderSVGShape* renderer = toRenderSVGShape(this->renderer());
    if (!renderer)
        return;

    if (isLengthAttribute) {
        renderer->setNeedsShapeUpdate();
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer);
        return;
    }

    ASSERT_NOT_REACHED();
}

bool SVGLineElement::selfHasRelativeLengths() const
{
    return m_x1->currentValue()->isRelative()
        || m_y1->currentValue()->isRelative()
        || m_x2->currentValue()->isRelative()
        || m_y2->currentValue()->isRelative();
}

}
