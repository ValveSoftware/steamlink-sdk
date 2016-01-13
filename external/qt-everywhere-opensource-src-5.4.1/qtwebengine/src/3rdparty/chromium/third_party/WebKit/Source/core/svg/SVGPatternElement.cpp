/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "core/svg/SVGPatternElement.h"

#include "core/XLinkNames.h"
#include "core/dom/ElementTraversal.h"
#include "core/rendering/svg/RenderSVGResourcePattern.h"
#include "core/svg/PatternAttributes.h"
#include "platform/transforms/AffineTransform.h"

namespace WebCore {

inline SVGPatternElement::SVGPatternElement(Document& document)
    : SVGElement(SVGNames::patternTag, document)
    , SVGURIReference(this)
    , SVGTests(this)
    , SVGFitToViewBox(this)
    , m_x(SVGAnimatedLength::create(this, SVGNames::xAttr, SVGLength::create(LengthModeWidth), AllowNegativeLengths))
    , m_y(SVGAnimatedLength::create(this, SVGNames::yAttr, SVGLength::create(LengthModeHeight), AllowNegativeLengths))
    , m_width(SVGAnimatedLength::create(this, SVGNames::widthAttr, SVGLength::create(LengthModeWidth), ForbidNegativeLengths))
    , m_height(SVGAnimatedLength::create(this, SVGNames::heightAttr, SVGLength::create(LengthModeHeight), ForbidNegativeLengths))
    , m_patternTransform(SVGAnimatedTransformList::create(this, SVGNames::patternTransformAttr, SVGTransformList::create()))
    , m_patternUnits(SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>::create(this, SVGNames::patternUnitsAttr, SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX))
    , m_patternContentUnits(SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>::create(this, SVGNames::patternContentUnitsAttr, SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE))
{
    ScriptWrappable::init(this);

    addToPropertyMap(m_x);
    addToPropertyMap(m_y);
    addToPropertyMap(m_width);
    addToPropertyMap(m_height);
    addToPropertyMap(m_patternTransform);
    addToPropertyMap(m_patternUnits);
    addToPropertyMap(m_patternContentUnits);
}

DEFINE_NODE_FACTORY(SVGPatternElement)

bool SVGPatternElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGURIReference::addSupportedAttributes(supportedAttributes);
        SVGTests::addSupportedAttributes(supportedAttributes);
        SVGFitToViewBox::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::patternUnitsAttr);
        supportedAttributes.add(SVGNames::patternContentUnitsAttr);
        supportedAttributes.add(SVGNames::patternTransformAttr);
        supportedAttributes.add(SVGNames::xAttr);
        supportedAttributes.add(SVGNames::yAttr);
        supportedAttributes.add(SVGNames::widthAttr);
        supportedAttributes.add(SVGNames::heightAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGPatternElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    SVGParsingError parseError = NoError;

    if (!isSupportedAttribute(name)) {
        SVGElement::parseAttribute(name, value);
    } else if (name == SVGNames::patternUnitsAttr) {
        m_patternUnits->setBaseValueAsString(value, parseError);
    } else if (name == SVGNames::patternContentUnitsAttr) {
        m_patternContentUnits->setBaseValueAsString(value, parseError);
    } else if (name == SVGNames::patternTransformAttr) {
        m_patternTransform->setBaseValueAsString(value, parseError);
    } else if (name == SVGNames::xAttr) {
        m_x->setBaseValueAsString(value, parseError);
    } else if (name == SVGNames::yAttr) {
        m_y->setBaseValueAsString(value, parseError);
    } else if (name == SVGNames::widthAttr) {
        m_width->setBaseValueAsString(value, parseError);
    } else if (name == SVGNames::heightAttr) {
        m_height->setBaseValueAsString(value, parseError);
    } else if (SVGURIReference::parseAttribute(name, value, parseError)) {
    } else if (SVGTests::parseAttribute(name, value)) {
    } else if (SVGFitToViewBox::parseAttribute(name, value, document(), parseError)) {
    } else {
        ASSERT_NOT_REACHED();
    }

    reportAttributeParsingError(parseError, name, value);
}

void SVGPatternElement::svgAttributeChanged(const QualifiedName& attrName)
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

void SVGPatternElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);

    if (changedByParser)
        return;

    if (RenderObject* object = renderer())
        object->setNeedsLayoutAndFullPaintInvalidation();
}

RenderObject* SVGPatternElement::createRenderer(RenderStyle*)
{
    return new RenderSVGResourcePattern(this);
}

static void setPatternAttributes(const SVGPatternElement* element, PatternAttributes& attributes)
{
    if (!attributes.hasX() && element->x()->isSpecified())
        attributes.setX(element->x()->currentValue());

    if (!attributes.hasY() && element->y()->isSpecified())
        attributes.setY(element->y()->currentValue());

    if (!attributes.hasWidth() && element->width()->isSpecified())
        attributes.setWidth(element->width()->currentValue());

    if (!attributes.hasHeight() && element->height()->isSpecified())
        attributes.setHeight(element->height()->currentValue());

    if (!attributes.hasViewBox() && element->viewBox()->isSpecified() && element->viewBox()->currentValue()->isValid())
        attributes.setViewBox(element->viewBox()->currentValue()->value());

    if (!attributes.hasPreserveAspectRatio() && element->preserveAspectRatio()->isSpecified())
        attributes.setPreserveAspectRatio(element->preserveAspectRatio()->currentValue());

    if (!attributes.hasPatternUnits() && element->patternUnits()->isSpecified())
        attributes.setPatternUnits(element->patternUnits()->currentValue()->enumValue());

    if (!attributes.hasPatternContentUnits() && element->patternContentUnits()->isSpecified())
        attributes.setPatternContentUnits(element->patternContentUnits()->currentValue()->enumValue());

    if (!attributes.hasPatternTransform() && element->patternTransform()->isSpecified()) {
        AffineTransform transform;
        element->patternTransform()->currentValue()->concatenate(transform);
        attributes.setPatternTransform(transform);
    }

    if (!attributes.hasPatternContentElement() && ElementTraversal::firstWithin(*element))
        attributes.setPatternContentElement(element);
}

void SVGPatternElement::collectPatternAttributes(PatternAttributes& attributes) const
{
    HashSet<const SVGPatternElement*> processedPatterns;
    const SVGPatternElement* current = this;

    while (true) {
        setPatternAttributes(current, attributes);
        processedPatterns.add(current);

        // Respect xlink:href, take attributes from referenced element
        Node* refNode = SVGURIReference::targetElementFromIRIString(current->hrefString(), treeScope());
        if (isSVGPatternElement(refNode)) {
            current = toSVGPatternElement(refNode);

            // Cycle detection
            if (processedPatterns.contains(current))
                return;
        } else {
            return;
        }
    }

    ASSERT_NOT_REACHED();
}

AffineTransform SVGPatternElement::localCoordinateSpaceTransform(SVGElement::CTMScope) const
{
    AffineTransform matrix;
    m_patternTransform->currentValue()->concatenate(matrix);
    return matrix;
}

bool SVGPatternElement::selfHasRelativeLengths() const
{
    return m_x->currentValue()->isRelative()
        || m_y->currentValue()->isRelative()
        || m_width->currentValue()->isRelative()
        || m_height->currentValue()->isRelative();
}

}
