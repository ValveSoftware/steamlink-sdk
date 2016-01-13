/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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

#include "core/rendering/svg/RenderSVGResourceMasker.h"

#include "core/rendering/svg/RenderSVGResource.h"
#include "core/rendering/svg/SVGRenderingContext.h"
#include "core/svg/SVGElement.h"
#include "platform/graphics/DisplayList.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/Vector.h"

namespace WebCore {

const RenderSVGResourceType RenderSVGResourceMasker::s_resourceType = MaskerResourceType;

RenderSVGResourceMasker::RenderSVGResourceMasker(SVGMaskElement* node)
    : RenderSVGResourceContainer(node)
{
}

RenderSVGResourceMasker::~RenderSVGResourceMasker()
{
}

void RenderSVGResourceMasker::removeAllClientsFromCache(bool markForInvalidation)
{
    m_maskContentDisplayList.clear();
    m_maskContentBoundaries = FloatRect();
    markAllClientsForInvalidation(markForInvalidation ? LayoutAndBoundariesInvalidation : ParentOnlyInvalidation);
}

void RenderSVGResourceMasker::removeClientFromCache(RenderObject* client, bool markForInvalidation)
{
    ASSERT(client);
    markClientForInvalidation(client, markForInvalidation ? BoundariesInvalidation : ParentOnlyInvalidation);
}

bool RenderSVGResourceMasker::applyResource(RenderObject* object, RenderStyle*,
    GraphicsContext*& context, unsigned short resourceMode)
{
    ASSERT(object);
    ASSERT(context);
    ASSERT(style());
    ASSERT_UNUSED(resourceMode, resourceMode == ApplyToDefaultMode);
    ASSERT_WITH_SECURITY_IMPLICATION(!needsLayout());

    clearInvalidationMask();

    FloatRect repaintRect = object->paintInvalidationRectInLocalCoordinates();
    if (repaintRect.isEmpty() || !element()->hasChildren())
        return false;

    // Content layer start.
    context->beginTransparencyLayer(1, &repaintRect);

    return true;
}

void RenderSVGResourceMasker::postApplyResource(RenderObject* object, GraphicsContext*& context,
    unsigned short resourceMode, const Path*, const RenderSVGShape*)
{
    ASSERT(object);
    ASSERT(context);
    ASSERT(style());
    ASSERT_UNUSED(resourceMode, resourceMode == ApplyToDefaultMode);
    ASSERT_WITH_SECURITY_IMPLICATION(!needsLayout());

    FloatRect repaintRect = object->paintInvalidationRectInLocalCoordinates();

    const SVGRenderStyle* svgStyle = style()->svgStyle();
    ASSERT(svgStyle);
    ColorFilter maskLayerFilter = svgStyle->maskType() == MT_LUMINANCE
        ? ColorFilterLuminanceToAlpha : ColorFilterNone;
    ColorFilter maskContentFilter = svgStyle->colorInterpolation() == CI_LINEARRGB
        ? ColorFilterSRGBToLinearRGB : ColorFilterNone;

    // Mask layer start.
    context->beginLayer(1, CompositeDestinationIn, &repaintRect, maskLayerFilter);
    {
        // Draw the mask with color conversion (when needed).
        GraphicsContextStateSaver maskContentSaver(*context);
        context->setColorFilter(maskContentFilter);

        drawMaskForRenderer(context, object->objectBoundingBox());
    }

    // Transfer mask layer -> content layer (DstIn)
    context->endLayer();
    // Transfer content layer -> backdrop (SrcOver)
    context->endLayer();
}

void RenderSVGResourceMasker::drawMaskForRenderer(GraphicsContext* context, const FloatRect& targetBoundingBox)
{
    ASSERT(context);

    AffineTransform contentTransformation;
    SVGUnitTypes::SVGUnitType contentUnits = toSVGMaskElement(element())->maskContentUnits()->currentValue()->enumValue();
    if (contentUnits == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        contentTransformation.translate(targetBoundingBox.x(), targetBoundingBox.y());
        contentTransformation.scaleNonUniform(targetBoundingBox.width(), targetBoundingBox.height());
        context->concatCTM(contentTransformation);
    }

    if (!m_maskContentDisplayList)
        m_maskContentDisplayList = asDisplayList(context, contentTransformation);
    ASSERT(m_maskContentDisplayList);
    context->drawDisplayList(m_maskContentDisplayList.get());
}

PassRefPtr<DisplayList> RenderSVGResourceMasker::asDisplayList(GraphicsContext* context,
    const AffineTransform& contentTransform)
{
    ASSERT(context);

    // Using strokeBoundingBox (instead of paintInvalidationRectInLocalCoordinates) to avoid the intersection
    // with local clips/mask, which may yield incorrect results when mixing objectBoundingBox and
    // userSpaceOnUse units (http://crbug.com/294900).
    context->beginRecording(strokeBoundingBox());
    for (Element* childElement = ElementTraversal::firstWithin(*element()); childElement; childElement = ElementTraversal::nextSibling(*childElement)) {
        RenderObject* renderer = childElement->renderer();
        if (!childElement->isSVGElement() || !renderer)
            continue;
        RenderStyle* style = renderer->style();
        if (!style || style->display() == NONE || style->visibility() != VISIBLE)
            continue;

        SVGRenderingContext::renderSubtree(context, renderer, contentTransform);
    }

    return context->endRecording();
}

void RenderSVGResourceMasker::calculateMaskContentRepaintRect()
{
    for (SVGElement* childElement = Traversal<SVGElement>::firstChild(*element()); childElement; childElement = Traversal<SVGElement>::nextSibling(*childElement)) {
        RenderObject* renderer = childElement->renderer();
        if (!renderer)
            continue;
        RenderStyle* style = renderer->style();
        if (!style || style->display() == NONE || style->visibility() != VISIBLE)
             continue;
        m_maskContentBoundaries.unite(renderer->localToParentTransform().mapRect(renderer->paintInvalidationRectInLocalCoordinates()));
    }
}

FloatRect RenderSVGResourceMasker::resourceBoundingBox(const RenderObject* object)
{
    SVGMaskElement* maskElement = toSVGMaskElement(element());
    ASSERT(maskElement);

    FloatRect objectBoundingBox = object->objectBoundingBox();
    FloatRect maskBoundaries = SVGLengthContext::resolveRectangle<SVGMaskElement>(maskElement, maskElement->maskUnits()->currentValue()->enumValue(), objectBoundingBox);

    // Resource was not layouted yet. Give back clipping rect of the mask.
    if (selfNeedsLayout())
        return maskBoundaries;

    if (m_maskContentBoundaries.isEmpty())
        calculateMaskContentRepaintRect();

    FloatRect maskRect = m_maskContentBoundaries;
    if (maskElement->maskContentUnits()->currentValue()->value() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        AffineTransform transform;
        transform.translate(objectBoundingBox.x(), objectBoundingBox.y());
        transform.scaleNonUniform(objectBoundingBox.width(), objectBoundingBox.height());
        maskRect = transform.mapRect(maskRect);
    }

    maskRect.intersect(maskBoundaries);
    return maskRect;
}

}
