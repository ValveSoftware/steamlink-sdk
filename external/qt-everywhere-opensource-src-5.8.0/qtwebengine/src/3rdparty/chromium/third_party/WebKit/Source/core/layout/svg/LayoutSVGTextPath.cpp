/*
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "core/layout/svg/LayoutSVGTextPath.h"

#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/svg/SVGPathElement.h"
#include "core/svg/SVGTextPathElement.h"
#include "platform/graphics/Path.h"
#include <memory>

namespace blink {

TreeScope& treeScopeForIdResolution(const SVGElement& element)
{
    if (SVGElement* correspondingElement = element.correspondingElement())
        return correspondingElement->treeScope();
    return element.treeScope();
}

PathPositionMapper::PathPositionMapper(const Path& path)
    : m_positionCalculator(path)
    , m_pathLength(path.length())
{
}

PathPositionMapper::PositionType PathPositionMapper::pointAndNormalAtLength(
    float length, FloatPoint& point, float& angle)
{
    if (length < 0)
        return BeforePath;
    if (length > m_pathLength)
        return AfterPath;
    ASSERT(length >= 0 && length <= m_pathLength);
    m_positionCalculator.pointAndNormalAtLength(length, point, angle);
    return OnPath;
}

LayoutSVGTextPath::LayoutSVGTextPath(Element* element)
    : LayoutSVGInline(element)
{
}

bool LayoutSVGTextPath::isChildAllowed(LayoutObject* child, const ComputedStyle&) const
{
    if (child->isText())
        return SVGLayoutSupport::isLayoutableTextNode(child);

    return child->isSVGInline() && !child->isSVGTextPath();
}

std::unique_ptr<PathPositionMapper> LayoutSVGTextPath::layoutPath() const
{
    const SVGTextPathElement& textPathElement = toSVGTextPathElement(*node());
    Element* targetElement = SVGURIReference::targetElementFromIRIString(
        textPathElement.hrefString(), treeScopeForIdResolution(textPathElement));

    if (!isSVGPathElement(targetElement))
        return nullptr;

    SVGPathElement& pathElement = toSVGPathElement(*targetElement);
    Path pathData = pathElement.asPath();
    if (pathData.isEmpty())
        return nullptr;

    // Spec:  The transform attribute on the referenced 'path' element represents a
    // supplemental transformation relative to the current user coordinate system for
    // the current 'text' element, including any adjustments to the current user coordinate
    // system due to a possible transform attribute on the current 'text' element.
    // http://www.w3.org/TR/SVG/text.html#TextPathElement
    pathData.transform(pathElement.calculateAnimatedLocalTransform());

    return PathPositionMapper::create(pathData);
}

float LayoutSVGTextPath::calculateStartOffset(float pathLength) const
{
    const SVGLength& startOffset = *toSVGTextPathElement(node())->startOffset()->currentValue();
    float textPathStartOffset = startOffset.valueAsPercentage();
    if (startOffset.typeWithCalcResolved() == CSSPrimitiveValue::UnitType::Percentage)
        textPathStartOffset *= pathLength;

    return textPathStartOffset;
}

} // namespace blink
