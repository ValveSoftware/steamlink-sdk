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

#include "config.h"

#include "core/svg/SVGFELightElement.h"

#include "core/SVGNames.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/svg/RenderSVGResource.h"
#include "core/svg/SVGFEDiffuseLightingElement.h"
#include "core/svg/SVGFESpecularLightingElement.h"

namespace WebCore {

SVGFELightElement::SVGFELightElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document)
    , m_azimuth(SVGAnimatedNumber::create(this, SVGNames::azimuthAttr, SVGNumber::create()))
    , m_elevation(SVGAnimatedNumber::create(this, SVGNames::elevationAttr, SVGNumber::create()))
    , m_x(SVGAnimatedNumber::create(this, SVGNames::xAttr, SVGNumber::create()))
    , m_y(SVGAnimatedNumber::create(this, SVGNames::yAttr, SVGNumber::create()))
    , m_z(SVGAnimatedNumber::create(this, SVGNames::zAttr, SVGNumber::create()))
    , m_pointsAtX(SVGAnimatedNumber::create(this, SVGNames::pointsAtXAttr, SVGNumber::create()))
    , m_pointsAtY(SVGAnimatedNumber::create(this, SVGNames::pointsAtYAttr, SVGNumber::create()))
    , m_pointsAtZ(SVGAnimatedNumber::create(this, SVGNames::pointsAtZAttr, SVGNumber::create()))
    , m_specularExponent(SVGAnimatedNumber::create(this, SVGNames::specularExponentAttr, SVGNumber::create(1)))
    , m_limitingConeAngle(SVGAnimatedNumber::create(this, SVGNames::limitingConeAngleAttr, SVGNumber::create()))
{
    addToPropertyMap(m_azimuth);
    addToPropertyMap(m_elevation);
    addToPropertyMap(m_x);
    addToPropertyMap(m_y);
    addToPropertyMap(m_z);
    addToPropertyMap(m_pointsAtX);
    addToPropertyMap(m_pointsAtY);
    addToPropertyMap(m_pointsAtZ);
    addToPropertyMap(m_specularExponent);
    addToPropertyMap(m_limitingConeAngle);
}

SVGFELightElement* SVGFELightElement::findLightElement(const SVGElement& svgElement)
{
    return Traversal<SVGFELightElement>::firstChild(svgElement);
}

bool SVGFELightElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        supportedAttributes.add(SVGNames::azimuthAttr);
        supportedAttributes.add(SVGNames::elevationAttr);
        supportedAttributes.add(SVGNames::xAttr);
        supportedAttributes.add(SVGNames::yAttr);
        supportedAttributes.add(SVGNames::zAttr);
        supportedAttributes.add(SVGNames::pointsAtXAttr);
        supportedAttributes.add(SVGNames::pointsAtYAttr);
        supportedAttributes.add(SVGNames::pointsAtZAttr);
        supportedAttributes.add(SVGNames::specularExponentAttr);
        supportedAttributes.add(SVGNames::limitingConeAngleAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGFELightElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!isSupportedAttribute(name)) {
        SVGElement::parseAttribute(name, value);
        return;
    }

    SVGParsingError parseError = NoError;

    if (name == SVGNames::azimuthAttr)
        m_azimuth->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::elevationAttr)
        m_elevation->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::xAttr)
        m_x->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::yAttr)
        m_y->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::zAttr)
        m_z->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::pointsAtXAttr)
        m_pointsAtX->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::pointsAtYAttr)
        m_pointsAtY->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::pointsAtZAttr)
        m_pointsAtZ->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::specularExponentAttr)
        m_specularExponent->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::limitingConeAngleAttr)
        m_limitingConeAngle->setBaseValueAsString(value, parseError);
    else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, name, value);
}

void SVGFELightElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElement::InvalidationGuard invalidationGuard(this);

    if (attrName == SVGNames::azimuthAttr
        || attrName == SVGNames::elevationAttr
        || attrName == SVGNames::xAttr
        || attrName == SVGNames::yAttr
        || attrName == SVGNames::zAttr
        || attrName == SVGNames::pointsAtXAttr
        || attrName == SVGNames::pointsAtYAttr
        || attrName == SVGNames::pointsAtZAttr
        || attrName == SVGNames::specularExponentAttr
        || attrName == SVGNames::limitingConeAngleAttr) {
        ContainerNode* parent = parentNode();
        if (!parent)
            return;

        RenderObject* renderer = parent->renderer();
        if (!renderer || !renderer->isSVGResourceFilterPrimitive())
            return;

        if (isSVGFEDiffuseLightingElement(*parent)) {
            toSVGFEDiffuseLightingElement(*parent).lightElementAttributeChanged(this, attrName);
            return;
        }
        if (isSVGFESpecularLightingElement(*parent)) {
            toSVGFESpecularLightingElement(*parent).lightElementAttributeChanged(this, attrName);
            return;
        }
    }

    ASSERT_NOT_REACHED();
}

void SVGFELightElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);

    if (!changedByParser) {
        if (ContainerNode* parent = parentNode()) {
            RenderObject* renderer = parent->renderer();
            if (renderer && renderer->isSVGResourceFilterPrimitive())
                RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer);
        }
    }
}

}
