/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "core/svg/SVGFilterElement.h"

#include "core/XLinkNames.h"
#include "core/rendering/svg/RenderSVGResourceFilter.h"
#include "core/svg/SVGParserUtilities.h"

namespace WebCore {

inline SVGFilterElement::SVGFilterElement(Document& document)
    : SVGElement(SVGNames::filterTag, document)
    , SVGURIReference(this)
    , m_x(SVGAnimatedLength::create(this, SVGNames::xAttr, SVGLength::create(LengthModeWidth), AllowNegativeLengths))
    , m_y(SVGAnimatedLength::create(this, SVGNames::yAttr, SVGLength::create(LengthModeHeight), AllowNegativeLengths))
    , m_width(SVGAnimatedLength::create(this, SVGNames::widthAttr, SVGLength::create(LengthModeWidth), ForbidNegativeLengths))
    , m_height(SVGAnimatedLength::create(this, SVGNames::heightAttr, SVGLength::create(LengthModeHeight), ForbidNegativeLengths))
    , m_filterUnits(SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>::create(this, SVGNames::filterUnitsAttr, SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX))
    , m_primitiveUnits(SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>::create(this, SVGNames::primitiveUnitsAttr, SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE))
    , m_filterRes(SVGAnimatedIntegerOptionalInteger::create(this, SVGNames::filterResAttr))
{
    ScriptWrappable::init(this);

    // Spec: If the x/y attribute is not specified, the effect is as if a value of "-10%" were specified.
    // Spec: If the width/height attribute is not specified, the effect is as if a value of "120%" were specified.
    m_x->setDefaultValueAsString("-10%");
    m_y->setDefaultValueAsString("-10%");
    m_width->setDefaultValueAsString("120%");
    m_height->setDefaultValueAsString("120%");

    addToPropertyMap(m_x);
    addToPropertyMap(m_y);
    addToPropertyMap(m_width);
    addToPropertyMap(m_height);
    addToPropertyMap(m_filterUnits);
    addToPropertyMap(m_primitiveUnits);
    addToPropertyMap(m_filterRes);
}

DEFINE_NODE_FACTORY(SVGFilterElement)

void SVGFilterElement::trace(Visitor* visitor)
{
    visitor->trace(m_clientsToAdd);
    SVGElement::trace(visitor);
}

void SVGFilterElement::setFilterRes(unsigned x, unsigned y)
{
    filterResX()->baseValue()->setValue(x);
    filterResY()->baseValue()->setValue(y);

    invalidateSVGAttributes();
    svgAttributeChanged(SVGNames::filterResAttr);
}

bool SVGFilterElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGURIReference::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::filterUnitsAttr);
        supportedAttributes.add(SVGNames::primitiveUnitsAttr);
        supportedAttributes.add(SVGNames::xAttr);
        supportedAttributes.add(SVGNames::yAttr);
        supportedAttributes.add(SVGNames::widthAttr);
        supportedAttributes.add(SVGNames::heightAttr);
        supportedAttributes.add(SVGNames::filterResAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGFilterElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    SVGParsingError parseError = NoError;

    if (!isSupportedAttribute(name))
        SVGElement::parseAttribute(name, value);
    else if (name == SVGNames::filterUnitsAttr)
        m_filterUnits->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::primitiveUnitsAttr)
        m_primitiveUnits->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::xAttr)
        m_x->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::yAttr)
        m_y->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::widthAttr)
        m_width->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::heightAttr)
        m_height->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::filterResAttr)
        m_filterRes->setBaseValueAsString(value, parseError);
    else if (SVGURIReference::parseAttribute(name, value, parseError)) {
    } else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, name, value);
}

void SVGFilterElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElement::InvalidationGuard invalidationGuard(this);

    if (attrName == SVGNames::xAttr
        || attrName == SVGNames::yAttr
        || attrName == SVGNames::widthAttr
        || attrName == SVGNames::heightAttr)
        updateRelativeLengthsInformation();

    RenderSVGResourceContainer* renderer = toRenderSVGResourceContainer(this->renderer());
    if (renderer)
        renderer->invalidateCacheAndMarkForLayout();
}

void SVGFilterElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);

    if (changedByParser)
        return;

    if (RenderObject* object = renderer())
        object->setNeedsLayoutAndFullPaintInvalidation();
}

RenderObject* SVGFilterElement::createRenderer(RenderStyle*)
{
    RenderSVGResourceFilter* renderer = new RenderSVGResourceFilter(this);

    WillBeHeapHashSet<RefPtrWillBeMember<Node> >::iterator layerEnd = m_clientsToAdd.end();
    for (WillBeHeapHashSet<RefPtrWillBeMember<Node> >::iterator it = m_clientsToAdd.begin(); it != layerEnd; ++it)
        renderer->addClientRenderLayer(it->get());
    m_clientsToAdd.clear();

    return renderer;
}

bool SVGFilterElement::selfHasRelativeLengths() const
{
    return m_x->currentValue()->isRelative()
        || m_y->currentValue()->isRelative()
        || m_width->currentValue()->isRelative()
        || m_height->currentValue()->isRelative();
}

void SVGFilterElement::addClient(Node* client)
{
    ASSERT(client);
    m_clientsToAdd.add(client);
}

void SVGFilterElement::removeClient(Node* client)
{
    ASSERT(client);
    m_clientsToAdd.remove(client);
}

}
