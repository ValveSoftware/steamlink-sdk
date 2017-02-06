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

#include "core/svg/SVGPathElement.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/dom/StyleChangeReason.h"
#include "core/layout/svg/LayoutSVGPath.h"
#include "core/svg/SVGMPathElement.h"
#include "core/svg/SVGPathQuery.h"
#include "core/svg/SVGPathUtilities.h"
#include "core/svg/SVGPointTearOff.h"

namespace blink {

class SVGAnimatedPathLength final : public SVGAnimatedNumber {
public:
    static SVGAnimatedPathLength* create(SVGPathElement* contextElement)
    {
        return new SVGAnimatedPathLength(contextElement);
    }

    SVGParsingError setBaseValueAsString(const String& value) override
    {
        SVGParsingError parseStatus = SVGAnimatedNumber::setBaseValueAsString(value);
        if (parseStatus == SVGParseStatus::NoError && baseValue()->value() < 0)
            parseStatus = SVGParseStatus::NegativeValue;
        return parseStatus;
    }

private:
    explicit SVGAnimatedPathLength(SVGPathElement* contextElement)
        : SVGAnimatedNumber(contextElement, SVGNames::pathLengthAttr, SVGNumber::create())
    {
    }
};

inline SVGPathElement::SVGPathElement(Document& document)
    : SVGGeometryElement(SVGNames::pathTag, document)
    , m_pathLength(SVGAnimatedPathLength::create(this))
    , m_path(SVGAnimatedPath::create(this, SVGNames::dAttr))
{
    addToPropertyMap(m_pathLength);
    addToPropertyMap(m_path);
}

DEFINE_TRACE(SVGPathElement)
{
    visitor->trace(m_pathLength);
    visitor->trace(m_path);
    SVGGeometryElement::trace(visitor);
}

DEFINE_NODE_FACTORY(SVGPathElement)

const StylePath* SVGPathElement::stylePath() const
{
    if (LayoutObject* layoutObject = this->layoutObject()) {
        const StylePath* stylePath = layoutObject->styleRef().svgStyle().d();
        if (stylePath)
            return stylePath;
        return StylePath::emptyPath();
    }
    return m_path->currentValue()->stylePath();
}

float SVGPathElement::pathLengthScaleFactor() const
{
    if (!pathLength()->isSpecified())
        return 1;
    float authorPathLength = pathLength()->currentValue()->value();
    if (authorPathLength < 0)
        return 1;
    if (!authorPathLength)
        return 0;
    float computedPathLength = stylePath()->length();
    if (!computedPathLength)
        return 1;
    return computedPathLength / authorPathLength;
}

Path SVGPathElement::asPath() const
{
    return stylePath()->path();
}

float SVGPathElement::getTotalLength()
{
    document().updateStyleAndLayoutIgnorePendingStylesheets();
    return SVGPathQuery(pathByteStream()).getTotalLength();
}

SVGPointTearOff* SVGPathElement::getPointAtLength(float length)
{
    document().updateStyleAndLayoutIgnorePendingStylesheets();
    FloatPoint point = SVGPathQuery(pathByteStream()).getPointAtLength(length);
    return SVGPointTearOff::create(SVGPoint::create(point), 0, PropertyIsNotAnimVal);
}

unsigned SVGPathElement::getPathSegAtLength(float length)
{
    document().updateStyleAndLayoutIgnorePendingStylesheets();
    return SVGPathQuery(pathByteStream()).getPathSegIndexAtLength(length);
}

bool SVGPathElement::isPresentationAttribute(const QualifiedName& attrName) const
{
    if (attrName == SVGNames::dAttr)
        return true;
    return SVGGeometryElement::isPresentationAttribute(attrName);
}

bool SVGPathElement::isPresentationAttributeWithSVGDOM(const QualifiedName& attrName) const
{
    if (attrName == SVGNames::dAttr)
        return true;
    return SVGGeometryElement::isPresentationAttributeWithSVGDOM(attrName);
}

void SVGPathElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::dAttr) {
        SVGElement::InvalidationGuard invalidationGuard(this);
        invalidateSVGPresentationAttributeStyle();
        setNeedsStyleRecalc(LocalStyleChange,
            StyleChangeReasonForTracing::fromAttribute(attrName));

        if (LayoutSVGShape* layoutPath = toLayoutSVGShape(this->layoutObject()))
            layoutPath->setNeedsShapeUpdate();

        invalidateMPathDependencies();
        if (layoutObject())
            markForLayoutAndParentResourceInvalidation(layoutObject());

        return;
    }

    if (attrName == SVGNames::pathLengthAttr) {
        SVGElement::InvalidationGuard invalidationGuard(this);
        if (layoutObject())
            markForLayoutAndParentResourceInvalidation(layoutObject());
        return;
    }

    SVGGeometryElement::svgAttributeChanged(attrName);
}

void SVGPathElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStylePropertySet* style)
{
    SVGAnimatedPropertyBase* property = propertyFromAttribute(name);
    if (property == m_path) {
        SVGAnimatedPath* path = this->path();
        // If this is a <use> instance, return the referenced path to maximize geometry sharing.
        if (const SVGElement* element = correspondingElement())
            path = toSVGPathElement(element)->path();

        CSSPathValue* pathValue = path->currentValue()->pathValue();
        if (pathValue->stylePath()->byteStream().isEmpty()) {
            addPropertyToPresentationAttributeStyle(style, CSSPropertyD, CSSPrimitiveValue::createIdentifier(CSSValueNone));
            return;
        }
        addPropertyToPresentationAttributeStyle(style, CSSPropertyD, pathValue);
        return;
    }
    SVGGeometryElement::collectStyleForPresentationAttribute(name, value, style);
}

void SVGPathElement::invalidateMPathDependencies()
{
    // <mpath> can only reference <path> but this dependency is not handled in
    // markForLayoutAndParentResourceInvalidation so we update any mpath dependencies manually.
    if (SVGElementSet* dependencies = setOfIncomingReferences()) {
        for (SVGElement* element : *dependencies) {
            if (isSVGMPathElement(*element))
                toSVGMPathElement(element)->targetPathChanged();
        }
    }
}

Node::InsertionNotificationRequest SVGPathElement::insertedInto(ContainerNode* rootParent)
{
    SVGGeometryElement::insertedInto(rootParent);
    invalidateMPathDependencies();
    return InsertionDone;
}

void SVGPathElement::removedFrom(ContainerNode* rootParent)
{
    SVGGeometryElement::removedFrom(rootParent);
    invalidateMPathDependencies();
}

} // namespace blink
