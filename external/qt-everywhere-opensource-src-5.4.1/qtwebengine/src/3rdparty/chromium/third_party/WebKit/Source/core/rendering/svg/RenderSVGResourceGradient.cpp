/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "core/rendering/svg/RenderSVGResourceGradient.h"

#include "core/rendering/svg/RenderSVGShape.h"
#include "core/rendering/svg/SVGRenderSupport.h"
#include "platform/graphics/GraphicsContext.h"

namespace WebCore {

RenderSVGResourceGradient::RenderSVGResourceGradient(SVGGradientElement* node)
    : RenderSVGResourceContainer(node)
    , m_shouldCollectGradientAttributes(true)
{
}

void RenderSVGResourceGradient::removeAllClientsFromCache(bool markForInvalidation)
{
    m_gradientMap.clear();
    m_shouldCollectGradientAttributes = true;
    markAllClientsForInvalidation(markForInvalidation ? RepaintInvalidation : ParentOnlyInvalidation);
}

void RenderSVGResourceGradient::removeClientFromCache(RenderObject* client, bool markForInvalidation)
{
    ASSERT(client);
    m_gradientMap.remove(client);
    markClientForInvalidation(client, markForInvalidation ? RepaintInvalidation : ParentOnlyInvalidation);
}

bool RenderSVGResourceGradient::applyResource(RenderObject* object, RenderStyle* style, GraphicsContext*& context, unsigned short resourceMode)
{
    ASSERT(object);
    ASSERT(style);
    ASSERT(context);
    ASSERT(resourceMode != ApplyToDefaultMode);

    clearInvalidationMask();

    // Be sure to synchronize all SVG properties on the gradientElement _before_ processing any further.
    // Otherwhise the call to collectGradientAttributes() in createTileImage(), may cause the SVG DOM property
    // synchronization to kick in, which causes removeAllClientsFromCache() to be called, which in turn deletes our
    // GradientData object! Leaving out the line below will cause svg/dynamic-updates/SVG*GradientElement-svgdom* to crash.
    SVGGradientElement* gradientElement = toSVGGradientElement(element());
    if (!gradientElement)
        return false;

    if (m_shouldCollectGradientAttributes) {
        gradientElement->synchronizeAnimatedSVGAttribute(anyQName());
        if (!collectGradientAttributes(gradientElement))
            return false;

        m_shouldCollectGradientAttributes = false;
    }

    // Spec: When the geometry of the applicable element has no width or height and objectBoundingBox is specified,
    // then the given effect (e.g. a gradient or a filter) will be ignored.
    FloatRect objectBoundingBox = object->objectBoundingBox();
    if (gradientUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX && objectBoundingBox.isEmpty())
        return false;

    OwnPtr<GradientData>& gradientData = m_gradientMap.add(object, nullptr).storedValue->value;
    if (!gradientData)
        gradientData = adoptPtr(new GradientData);

    // Create gradient object
    if (!gradientData->gradient) {
        buildGradient(gradientData.get());

        // We want the text bounding box applied to the gradient space transform now, so the gradient shader can use it.
        if (gradientUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX && !objectBoundingBox.isEmpty()) {
            gradientData->userspaceTransform.translate(objectBoundingBox.x(), objectBoundingBox.y());
            gradientData->userspaceTransform.scaleNonUniform(objectBoundingBox.width(), objectBoundingBox.height());
        }

        AffineTransform gradientTransform;
        calculateGradientTransform(gradientTransform);

        gradientData->userspaceTransform *= gradientTransform;
    }

    if (!gradientData->gradient)
        return false;

    const SVGRenderStyle* svgStyle = style->svgStyle();
    ASSERT(svgStyle);

    AffineTransform computedGradientSpaceTransform = computeResourceSpaceTransform(object, gradientData->userspaceTransform, svgStyle, resourceMode);
    gradientData->gradient->setGradientSpaceTransform(computedGradientSpaceTransform);

    // Draw gradient
    context->save();

    if (resourceMode & ApplyToTextMode)
        context->setTextDrawingMode(resourceMode & ApplyToFillMode ? TextModeFill : TextModeStroke);

    if (resourceMode & ApplyToFillMode) {
        context->setAlphaAsFloat(svgStyle->fillOpacity());
        context->setFillGradient(gradientData->gradient);
        context->setFillRule(svgStyle->fillRule());
    } else if (resourceMode & ApplyToStrokeMode) {
        context->setAlphaAsFloat(svgStyle->strokeOpacity());
        context->setStrokeGradient(gradientData->gradient);
        SVGRenderSupport::applyStrokeStyleToContext(context, style, object);
    }

    return true;
}

void RenderSVGResourceGradient::postApplyResource(RenderObject*, GraphicsContext*& context, unsigned short resourceMode, const Path* path, const RenderSVGShape* shape)
{
    ASSERT(context);
    ASSERT(resourceMode != ApplyToDefaultMode);

    if (resourceMode & ApplyToFillMode) {
        if (path)
            context->fillPath(*path);
        else if (shape)
            shape->fillShape(context);
    }
    if (resourceMode & ApplyToStrokeMode) {
        if (path)
            context->strokePath(*path);
        else if (shape)
            shape->strokeShape(context);
    }

    context->restore();
}

void RenderSVGResourceGradient::addStops(GradientData* gradientData, const Vector<Gradient::ColorStop>& stops) const
{
    ASSERT(gradientData->gradient);

    const Vector<Gradient::ColorStop>::const_iterator end = stops.end();
    for (Vector<Gradient::ColorStop>::const_iterator it = stops.begin(); it != end; ++it)
        gradientData->gradient->addColorStop(*it);
}

GradientSpreadMethod RenderSVGResourceGradient::platformSpreadMethodFromSVGType(SVGSpreadMethodType method) const
{
    switch (method) {
    case SVGSpreadMethodUnknown:
    case SVGSpreadMethodPad:
        return SpreadMethodPad;
    case SVGSpreadMethodReflect:
        return SpreadMethodReflect;
    case SVGSpreadMethodRepeat:
        return SpreadMethodRepeat;
    }

    ASSERT_NOT_REACHED();
    return SpreadMethodPad;
}

}
