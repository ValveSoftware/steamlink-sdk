/*
 * Copyright (C) 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#include "core/rendering/svg/SVGRenderingContext.h"

#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/svg/RenderSVGImage.h"
#include "core/rendering/svg/RenderSVGResource.h"
#include "core/rendering/svg/RenderSVGResourceFilter.h"
#include "core/rendering/svg/RenderSVGResourceMasker.h"
#include "core/rendering/svg/SVGResources.h"
#include "core/rendering/svg/SVGResourcesCache.h"

static int kMaxImageBufferSize = 4096;

namespace WebCore {

static inline bool isRenderingMaskImage(RenderObject* object)
{
    if (object->frame() && object->frame()->view())
        return object->frame()->view()->paintBehavior() & PaintBehaviorRenderingSVGMask;
    return false;
}

SVGRenderingContext::~SVGRenderingContext()
{
    // Fast path if we don't need to restore anything.
    if (!(m_renderingFlags & ActionsNeeded))
        return;

    ASSERT(m_object && m_paintInfo);

    if (m_renderingFlags & PostApplyResources) {
        ASSERT(m_masker || m_clipper || m_filter);
        ASSERT(SVGResourcesCache::cachedResourcesForRenderObject(m_object));

        if (m_filter) {
            ASSERT(SVGResourcesCache::cachedResourcesForRenderObject(m_object)->filter() == m_filter);
            m_filter->postApplyResource(m_object, m_paintInfo->context, ApplyToDefaultMode, 0, 0);
            m_paintInfo->context = m_savedContext;
            m_paintInfo->rect = m_savedPaintRect;
        }

        if (m_clipper) {
            ASSERT(SVGResourcesCache::cachedResourcesForRenderObject(m_object)->clipper() == m_clipper);
            m_clipper->postApplyStatefulResource(m_object, m_paintInfo->context, m_clipperContext);
        }

        if (m_masker) {
            ASSERT(SVGResourcesCache::cachedResourcesForRenderObject(m_object)->masker() == m_masker);
            m_masker->postApplyResource(m_object, m_paintInfo->context, ApplyToDefaultMode, 0, 0);
        }
    }

    if (m_renderingFlags & EndOpacityLayer)
        m_paintInfo->context->endLayer();

    if (m_renderingFlags & RestoreGraphicsContext)
        m_paintInfo->context->restore();
}

void SVGRenderingContext::prepareToRenderSVGContent(RenderObject* object, PaintInfo& paintInfo, NeedsGraphicsContextSave needsGraphicsContextSave)
{
    ASSERT(object);

#ifndef NDEBUG
    // This function must not be called twice!
    ASSERT(!(m_renderingFlags & PrepareToRenderSVGContentWasCalled));
    m_renderingFlags |= PrepareToRenderSVGContentWasCalled;
#endif

    m_object = object;
    m_paintInfo = &paintInfo;
    m_filter = 0;

    // We need to save / restore the context even if the initialization failed.
    if (needsGraphicsContextSave == SaveGraphicsContext) {
        m_paintInfo->context->save();
        m_renderingFlags |= RestoreGraphicsContext;
    }

    RenderStyle* style = m_object->style();
    ASSERT(style);

    const SVGRenderStyle* svgStyle = style->svgStyle();
    ASSERT(svgStyle);

    // Setup transparency layers before setting up SVG resources!
    bool isRenderingMask = isRenderingMaskImage(m_object);
    // RenderLayer takes care of root opacity.
    float opacity = (object->isSVGRoot() || isRenderingMask) ? 1 : style->opacity();
    bool hasBlendMode = style->hasBlendMode() && !isRenderingMask;

    if (opacity < 1 || hasBlendMode || style->hasIsolation()) {
        FloatRect repaintRect = m_object->paintInvalidationRectInLocalCoordinates();
        m_paintInfo->context->clip(repaintRect);

        if (hasBlendMode) {
            if (!(m_renderingFlags & RestoreGraphicsContext)) {
                m_paintInfo->context->save();
                m_renderingFlags |= RestoreGraphicsContext;
            }
            m_paintInfo->context->setCompositeOperation(CompositeSourceOver, style->blendMode());
        }

        m_paintInfo->context->beginTransparencyLayer(opacity);

        if (hasBlendMode)
            m_paintInfo->context->setCompositeOperation(CompositeSourceOver, blink::WebBlendModeNormal);

        m_renderingFlags |= EndOpacityLayer;
    }

    ClipPathOperation* clipPathOperation = style->clipPath();
    if (clipPathOperation && clipPathOperation->type() == ClipPathOperation::SHAPE) {
        ShapeClipPathOperation* clipPath = toShapeClipPathOperation(clipPathOperation);
        m_paintInfo->context->clipPath(clipPath->path(object->objectBoundingBox()), clipPath->windRule());
    }

    SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(m_object);
    if (!resources) {
        if (svgStyle->hasFilter())
            return;

        m_renderingFlags |= RenderingPrepared;
        return;
    }

    if (!isRenderingMask) {
        if (RenderSVGResourceMasker* masker = resources->masker()) {
            if (!masker->applyResource(m_object, style, m_paintInfo->context, ApplyToDefaultMode))
                return;
            m_masker = masker;
            m_renderingFlags |= PostApplyResources;
        }
    }

    RenderSVGResourceClipper* clipper = resources->clipper();
    if (!clipPathOperation && clipper) {
        if (!clipper->applyStatefulResource(m_object, m_paintInfo->context, m_clipperContext))
            return;
        m_clipper = clipper;
        m_renderingFlags |= PostApplyResources;
    }

    if (!isRenderingMask) {
        m_filter = resources->filter();
        if (m_filter) {
            m_savedContext = m_paintInfo->context;
            m_savedPaintRect = m_paintInfo->rect;
            // Return with false here may mean that we don't need to draw the content
            // (because it was either drawn before or empty) but we still need to apply the filter.
            m_renderingFlags |= PostApplyResources;
            if (!m_filter->applyResource(m_object, style, m_paintInfo->context, ApplyToDefaultMode))
                return;

            // Since we're caching the resulting bitmap and do not invalidate it on repaint rect
            // changes, we need to paint the whole filter region. Otherwise, elements not visible
            // at the time of the initial paint (due to scrolling, window size, etc.) will never
            // be drawn.
            m_paintInfo->rect = IntRect(m_filter->drawingRegion(m_object));
        }
    }

    m_renderingFlags |= RenderingPrepared;
}

static AffineTransform& currentContentTransformation()
{
    DEFINE_STATIC_LOCAL(AffineTransform, s_currentContentTransformation, ());
    return s_currentContentTransformation;
}

float SVGRenderingContext::calculateScreenFontSizeScalingFactor(const RenderObject* renderer)
{
    ASSERT(renderer);

    AffineTransform ctm;
    // FIXME: calculateDeviceSpaceTransformation() queries layer compositing state - which is not
    // supported during layout. Hence, the result may not include all CSS transforms.
    calculateDeviceSpaceTransformation(renderer, ctm);
    return narrowPrecisionToFloat(sqrt((pow(ctm.xScale(), 2) + pow(ctm.yScale(), 2)) / 2));
}

void SVGRenderingContext::calculateDeviceSpaceTransformation(const RenderObject* renderer, AffineTransform& absoluteTransform)
{
    // FIXME: trying to compute a device space transform at record time is wrong. All clients
    // should be updated to avoid relying on this information, and the method should be removed.

    ASSERT(renderer);
    // We're about to possibly clear renderer, so save the deviceScaleFactor now.
    float deviceScaleFactor = renderer->document().frameHost()->deviceScaleFactor();

    // Walk up the render tree, accumulating SVG transforms.
    absoluteTransform = currentContentTransformation();
    while (renderer) {
        absoluteTransform = renderer->localToParentTransform() * absoluteTransform;
        if (renderer->isSVGRoot())
            break;
        renderer = renderer->parent();
    }

    // Continue walking up the layer tree, accumulating CSS transforms.
    RenderLayer* layer = renderer ? renderer->enclosingLayer() : 0;
    while (layer && layer->isAllowedToQueryCompositingState()) {
        // We can stop at compositing layers, to match the backing resolution.
        // FIXME: should we be computing the transform to the nearest composited layer,
        // or the nearest composited layer that does not paint into its ancestor?
        // I think this is the nearest composited ancestor since we will inherit its
        // transforms in the composited layer tree.
        if (layer->compositingState() != NotComposited)
            break;

        if (TransformationMatrix* layerTransform = layer->transform())
            absoluteTransform = layerTransform->toAffineTransform() * absoluteTransform;

        layer = layer->parent();
    }

    absoluteTransform.scale(deviceScaleFactor);
}

void SVGRenderingContext::renderSubtree(GraphicsContext* context, RenderObject* item, const AffineTransform& subtreeContentTransformation)
{
    ASSERT(item);
    ASSERT(context);

    PaintInfo info(context, PaintInfo::infiniteRect(), PaintPhaseForeground, PaintBehaviorNormal);

    AffineTransform& contentTransformation = currentContentTransformation();
    AffineTransform savedContentTransformation = contentTransformation;
    contentTransformation = subtreeContentTransformation * contentTransformation;

    ASSERT(!item->needsLayout());
    item->paint(info, IntPoint());

    contentTransformation = savedContentTransformation;
}

FloatRect SVGRenderingContext::clampedAbsoluteTargetRect(const FloatRect& absoluteTargetRect)
{
    const FloatSize maxImageBufferSize(kMaxImageBufferSize, kMaxImageBufferSize);
    return FloatRect(absoluteTargetRect.location(), absoluteTargetRect.size().shrunkTo(maxImageBufferSize));
}

void SVGRenderingContext::clear2DRotation(AffineTransform& transform)
{
    AffineTransform::DecomposedType decomposition;
    transform.decompose(decomposition);
    decomposition.angle = 0;
    transform.recompose(decomposition);
}

bool SVGRenderingContext::bufferForeground(OwnPtr<ImageBuffer>& imageBuffer)
{
    ASSERT(m_paintInfo);
    ASSERT(m_object->isSVGImage());
    FloatRect boundingBox = m_object->objectBoundingBox();

    // Invalidate an existing buffer if the scale is not correct.
    if (imageBuffer) {
        AffineTransform transform = m_paintInfo->context->getCTM();
        IntSize expandedBoundingBox = expandedIntSize(boundingBox.size());
        IntSize bufferSize(static_cast<int>(ceil(expandedBoundingBox.width() * transform.xScale())), static_cast<int>(ceil(expandedBoundingBox.height() * transform.yScale())));
        if (bufferSize != imageBuffer->size())
            imageBuffer.clear();
    }

    // Create a new buffer and paint the foreground into it.
    if (!imageBuffer) {
        if ((imageBuffer = m_paintInfo->context->createCompatibleBuffer(expandedIntSize(boundingBox.size())))) {
            GraphicsContext* bufferedRenderingContext = imageBuffer->context();
            bufferedRenderingContext->translate(-boundingBox.x(), -boundingBox.y());
            PaintInfo bufferedInfo(*m_paintInfo);
            bufferedInfo.context = bufferedRenderingContext;
            toRenderSVGImage(m_object)->paintForeground(bufferedInfo);
        } else
            return false;
    }

    m_paintInfo->context->drawImageBuffer(imageBuffer.get(), boundingBox);
    return true;
}

}
