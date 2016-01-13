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

#include "core/svg/SVGRadialGradientElement.h"

#include "core/rendering/svg/RenderSVGResourceRadialGradient.h"
#include "core/svg/RadialGradientAttributes.h"
#include "core/svg/SVGTransformList.h"

namespace WebCore {

inline SVGRadialGradientElement::SVGRadialGradientElement(Document& document)
    : SVGGradientElement(SVGNames::radialGradientTag, document)
    , m_cx(SVGAnimatedLength::create(this, SVGNames::cxAttr, SVGLength::create(LengthModeWidth), AllowNegativeLengths))
    , m_cy(SVGAnimatedLength::create(this, SVGNames::cyAttr, SVGLength::create(LengthModeHeight), AllowNegativeLengths))
    , m_r(SVGAnimatedLength::create(this, SVGNames::rAttr, SVGLength::create(LengthModeOther), ForbidNegativeLengths))
    , m_fx(SVGAnimatedLength::create(this, SVGNames::fxAttr, SVGLength::create(LengthModeWidth), AllowNegativeLengths))
    , m_fy(SVGAnimatedLength::create(this, SVGNames::fyAttr, SVGLength::create(LengthModeHeight), AllowNegativeLengths))
    , m_fr(SVGAnimatedLength::create(this, SVGNames::frAttr, SVGLength::create(LengthModeOther), ForbidNegativeLengths))
{
    ScriptWrappable::init(this);

    // Spec: If the cx/cy/r attribute is not specified, the effect is as if a value of "50%" were specified.
    m_cx->setDefaultValueAsString("50%");
    m_cy->setDefaultValueAsString("50%");
    m_r->setDefaultValueAsString("50%");

    // SVG2-Draft Spec: If the fr attributed is not specified, the effect is as if a value of "0%" were specified.
    m_fr->setDefaultValueAsString("0%");

    addToPropertyMap(m_cx);
    addToPropertyMap(m_cy);
    addToPropertyMap(m_r);
    addToPropertyMap(m_fx);
    addToPropertyMap(m_fy);
    addToPropertyMap(m_fr);
}

DEFINE_NODE_FACTORY(SVGRadialGradientElement)

bool SVGRadialGradientElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        supportedAttributes.add(SVGNames::cxAttr);
        supportedAttributes.add(SVGNames::cyAttr);
        supportedAttributes.add(SVGNames::fxAttr);
        supportedAttributes.add(SVGNames::fyAttr);
        supportedAttributes.add(SVGNames::rAttr);
        supportedAttributes.add(SVGNames::frAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGRadialGradientElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    SVGParsingError parseError = NoError;

    if (!isSupportedAttribute(name))
        SVGGradientElement::parseAttribute(name, value);
    else if (name == SVGNames::cxAttr)
        m_cx->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::cyAttr)
        m_cy->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::rAttr)
        m_r->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::fxAttr)
        m_fx->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::fyAttr)
        m_fy->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::frAttr)
        m_fr->setBaseValueAsString(value, parseError);
    else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, name, value);
}

void SVGRadialGradientElement::svgAttributeChanged(const QualifiedName& attrName)
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

RenderObject* SVGRadialGradientElement::createRenderer(RenderStyle*)
{
    return new RenderSVGResourceRadialGradient(this);
}

static void setGradientAttributes(SVGGradientElement* element, RadialGradientAttributes& attributes, bool isRadial = true)
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

    if (isRadial) {
        SVGRadialGradientElement* radial = toSVGRadialGradientElement(element);

        if (!attributes.hasCx() && radial->cx()->isSpecified())
            attributes.setCx(radial->cx()->currentValue());

        if (!attributes.hasCy() && radial->cy()->isSpecified())
            attributes.setCy(radial->cy()->currentValue());

        if (!attributes.hasR() && radial->r()->isSpecified())
            attributes.setR(radial->r()->currentValue());

        if (!attributes.hasFx() && radial->fx()->isSpecified())
            attributes.setFx(radial->fx()->currentValue());

        if (!attributes.hasFy() && radial->fy()->isSpecified())
            attributes.setFy(radial->fy()->currentValue());

        if (!attributes.hasFr() && radial->fr()->isSpecified())
            attributes.setFr(radial->fr()->currentValue());
    }
}

bool SVGRadialGradientElement::collectGradientAttributes(RadialGradientAttributes& attributes)
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
                break;

            if (!current->renderer())
                return false;

            setGradientAttributes(current, attributes, isSVGRadialGradientElement(*current));
            processedGradients.add(current);
        } else {
            break;
        }
    }

    // Handle default values for fx/fy
    if (!attributes.hasFx())
        attributes.setFx(attributes.cx());

    if (!attributes.hasFy())
        attributes.setFy(attributes.cy());

    return true;
}

bool SVGRadialGradientElement::selfHasRelativeLengths() const
{
    return m_cx->currentValue()->isRelative()
        || m_cy->currentValue()->isRelative()
        || m_r->currentValue()->isRelative()
        || m_fx->currentValue()->isRelative()
        || m_fy->currentValue()->isRelative()
        || m_fr->currentValue()->isRelative();
}

}
