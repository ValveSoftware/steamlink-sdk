/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
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

#include "core/svg/SVGLinearGradientElement.h"

#include "core/rendering/svg/RenderSVGResourceLinearGradient.h"
#include "core/svg/LinearGradientAttributes.h"
#include "core/svg/SVGLength.h"
#include "core/svg/SVGTransformList.h"

namespace WebCore {

inline SVGLinearGradientElement::SVGLinearGradientElement(Document& document)
    : SVGGradientElement(SVGNames::linearGradientTag, document)
    , m_x1(SVGAnimatedLength::create(this, SVGNames::x1Attr, SVGLength::create(LengthModeWidth), AllowNegativeLengths))
    , m_y1(SVGAnimatedLength::create(this, SVGNames::y1Attr, SVGLength::create(LengthModeHeight), AllowNegativeLengths))
    , m_x2(SVGAnimatedLength::create(this, SVGNames::x2Attr, SVGLength::create(LengthModeWidth), AllowNegativeLengths))
    , m_y2(SVGAnimatedLength::create(this, SVGNames::y2Attr, SVGLength::create(LengthModeHeight), AllowNegativeLengths))
{
    ScriptWrappable::init(this);

    // Spec: If the x2 attribute is not specified, the effect is as if a value of "100%" were specified.
    m_x2->setDefaultValueAsString("100%");

    addToPropertyMap(m_x1);
    addToPropertyMap(m_y1);
    addToPropertyMap(m_x2);
    addToPropertyMap(m_y2);
}

DEFINE_NODE_FACTORY(SVGLinearGradientElement)

bool SVGLinearGradientElement::isSupportedAttribute(const QualifiedName& attrName)
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

void SVGLinearGradientElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    SVGParsingError parseError = NoError;

    if (!isSupportedAttribute(name))
        SVGGradientElement::parseAttribute(name, value);
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

void SVGLinearGradientElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGGradientElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElement::InvalidationGuard invalidationGuard(this);

    updateRelativeLengthsInformation();

    RenderSVGResourceContainer* renderer = toRenderSVGResourceContainer(this->renderer());
    if (renderer)
        renderer->invalidateCacheAndMarkForLayout();
}

RenderObject* SVGLinearGradientElement::createRenderer(RenderStyle*)
{
    return new RenderSVGResourceLinearGradient(this);
}

static void setGradientAttributes(SVGGradientElement* element, LinearGradientAttributes& attributes, bool isLinear = true)
{
    if (!attributes.hasSpreadMethod() && element->spreadMethod()->isSpecified())
        attributes.setSpreadMethod(element->spreadMethod()->currentValue()->enumValue());

    if (!attributes.hasGradientUnits() && element->gradientUnits()->isSpecified())
        attributes.setGradientUnits(element->gradientUnits()->currentValue()->enumValue());

    if (!attributes.hasGradientTransform() && element->gradientTransform()->isSpecified()) {
        AffineTransform transform;
        element->gradientTransform()->currentValue()->concatenate(transform);
        attributes.setGradientTransform(transform);
    }

    if (!attributes.hasStops()) {
        const Vector<Gradient::ColorStop>& stops(element->buildStops());
        if (!stops.isEmpty())
            attributes.setStops(stops);
    }

    if (isLinear) {
        SVGLinearGradientElement* linear = toSVGLinearGradientElement(element);

        if (!attributes.hasX1() && linear->x1()->isSpecified())
            attributes.setX1(linear->x1()->currentValue());

        if (!attributes.hasY1() && linear->y1()->isSpecified())
            attributes.setY1(linear->y1()->currentValue());

        if (!attributes.hasX2() && linear->x2()->isSpecified())
            attributes.setX2(linear->x2()->currentValue());

        if (!attributes.hasY2() && linear->y2()->isSpecified())
            attributes.setY2(linear->y2()->currentValue());
    }
}

bool SVGLinearGradientElement::collectGradientAttributes(LinearGradientAttributes& attributes)
{
    if (!renderer())
        return false;

    HashSet<SVGGradientElement*> processedGradients;
    SVGGradientElement* current = this;

    setGradientAttributes(current, attributes);
    processedGradients.add(current);

    while (true) {
        // Respect xlink:href, take attributes from referenced element
        Node* refNode = SVGURIReference::targetElementFromIRIString(current->href()->currentValue()->value(), treeScope());
        if (refNode && isSVGGradientElement(*refNode)) {
            current = toSVGGradientElement(refNode);

            // Cycle detection
            if (processedGradients.contains(current))
                return true;

            if (!current->renderer())
                return false;

            setGradientAttributes(current, attributes, isSVGLinearGradientElement(*current));
            processedGradients.add(current);
        } else {
            return true;
        }
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool SVGLinearGradientElement::selfHasRelativeLengths() const
{
    return m_x1->currentValue()->isRelative()
        || m_y1->currentValue()->isRelative()
        || m_x2->currentValue()->isRelative()
        || m_y2->currentValue()->isRelative();
}

}
