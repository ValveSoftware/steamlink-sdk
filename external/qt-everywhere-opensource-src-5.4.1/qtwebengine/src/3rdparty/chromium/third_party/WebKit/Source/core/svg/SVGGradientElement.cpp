/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGGradientElement.h"

#include "core/XLinkNames.h"
#include "core/dom/Attribute.h"
#include "core/rendering/svg/RenderSVGPath.h"
#include "core/rendering/svg/RenderSVGResourceLinearGradient.h"
#include "core/rendering/svg/RenderSVGResourceRadialGradient.h"
#include "core/svg/SVGStopElement.h"
#include "core/svg/SVGTransformList.h"

namespace WebCore {

template<> const SVGEnumerationStringEntries& getStaticStringEntries<SVGSpreadMethodType>()
{
    DEFINE_STATIC_LOCAL(SVGEnumerationStringEntries, entries, ());
    if (entries.isEmpty()) {
        entries.append(std::make_pair(SVGSpreadMethodPad, "pad"));
        entries.append(std::make_pair(SVGSpreadMethodReflect, "reflect"));
        entries.append(std::make_pair(SVGSpreadMethodRepeat, "repeat"));
    }
    return entries;
}

SVGGradientElement::SVGGradientElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document)
    , SVGURIReference(this)
    , m_gradientTransform(SVGAnimatedTransformList::create(this, SVGNames::gradientTransformAttr, SVGTransformList::create()))
    , m_spreadMethod(SVGAnimatedEnumeration<SVGSpreadMethodType>::create(this, SVGNames::spreadMethodAttr, SVGSpreadMethodPad))
    , m_gradientUnits(SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>::create(this, SVGNames::gradientUnitsAttr, SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX))
{
    addToPropertyMap(m_gradientTransform);
    addToPropertyMap(m_spreadMethod);
    addToPropertyMap(m_gradientUnits);
}

bool SVGGradientElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGURIReference::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::gradientUnitsAttr);
        supportedAttributes.add(SVGNames::gradientTransformAttr);
        supportedAttributes.add(SVGNames::spreadMethodAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGGradientElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!isSupportedAttribute(name)) {
        SVGElement::parseAttribute(name, value);
        return;
    }

    SVGParsingError parseError = NoError;

    if (name == SVGNames::gradientTransformAttr)
        m_gradientTransform->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::gradientUnitsAttr)
        m_gradientUnits->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::spreadMethodAttr)
        m_spreadMethod->setBaseValueAsString(value, parseError);
    else if (SVGURIReference::parseAttribute(name, value, parseError)) {
    } else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, name, value);
}

void SVGGradientElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElement::InvalidationGuard invalidationGuard(this);

    RenderSVGResourceContainer* renderer = toRenderSVGResourceContainer(this->renderer());
    if (renderer)
        renderer->invalidateCacheAndMarkForLayout();
}

void SVGGradientElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);

    if (changedByParser)
        return;

    if (RenderObject* object = renderer())
        object->setNeedsLayoutAndFullPaintInvalidation();
}

Vector<Gradient::ColorStop> SVGGradientElement::buildStops()
{
    Vector<Gradient::ColorStop> stops;

    float previousOffset = 0.0f;
    for (SVGStopElement* stop = Traversal<SVGStopElement>::firstChild(*this); stop; stop = Traversal<SVGStopElement>::nextSibling(*stop)) {
        // Figure out right monotonic offset
        float offset = stop->offset()->currentValue()->value();
        offset = std::min(std::max(previousOffset, offset), 1.0f);
        previousOffset = offset;

        stops.append(Gradient::ColorStop(offset, stop->stopColorIncludingOpacity()));
    }

    return stops;
}

}
