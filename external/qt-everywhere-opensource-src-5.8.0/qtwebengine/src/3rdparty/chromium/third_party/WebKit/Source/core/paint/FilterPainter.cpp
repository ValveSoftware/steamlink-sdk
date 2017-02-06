// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/FilterPainter.h"

#include "core/paint/FilterEffectBuilder.h"
#include "core/paint/LayerClipRecorder.h"
#include "core/paint/PaintLayer.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/CompositorFilterOperations.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/filters/FilterEffect.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/graphics/paint/FilterDisplayItem.h"
#include "platform/graphics/paint/PaintController.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

FilterPainter::FilterPainter(PaintLayer& layer, GraphicsContext& context, const LayoutPoint& offsetFromRoot, const ClipRect& clipRect, PaintLayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags,
    LayoutRect& rootRelativeBounds, bool& rootRelativeBoundsComputed)
    : m_filterInProgress(false)
    , m_context(context)
    , m_layoutObject(layer.layoutObject())
{
    if (!layer.paintsWithFilters())
        return;

    FilterEffect* lastEffect = layer.lastFilterEffect();
    if (!lastEffect)
        return;

    ASSERT(layer.filterInfo());

    lastEffect->determineFilterPrimitiveSubregion(MapRectForward);
    sk_sp<SkImageFilter> imageFilter = SkiaImageFilterBuilder::build(lastEffect, ColorSpaceDeviceRGB);
    if (!imageFilter)
        return;

    if (!rootRelativeBoundsComputed) {
        rootRelativeBounds = layer.physicalBoundingBoxIncludingReflectionAndStackingChildren(offsetFromRoot);
        rootRelativeBoundsComputed = true;
    }

    // We'll handle clipping to the dirty rect before filter rasterization.
    // Filter processing will automatically expand the clip rect and the offscreen to accommodate any filter outsets.
    // FIXME: It is incorrect to just clip to the damageRect here once multiple fragments are involved.

    // Subsequent code should not clip to the dirty rect, since we've already
    // done it above, and doing it later will defeat the outsets.
    paintingInfo.clipToDirtyRect = false;

    if (clipRect.rect() != paintingInfo.paintDirtyRect || clipRect.hasRadius()) {
        m_clipRecorder = wrapUnique(new LayerClipRecorder(context, *layer.layoutObject(), DisplayItem::ClipLayerFilter, clipRect, &paintingInfo, LayoutPoint(), paintFlags));
    }

    ASSERT(m_layoutObject);
    if (!context.getPaintController().displayItemConstructionIsDisabled()) {
        FilterOperations filterOperations(layer.computeFilterOperations(m_layoutObject->styleRef()));
        std::unique_ptr<CompositorFilterOperations> compositorFilterOperations = CompositorFilterOperations::create();
        SkiaImageFilterBuilder::buildFilterOperations(filterOperations, compositorFilterOperations.get());
        // FIXME: It's possible to have empty CompositorFilterOperations here even
        // though the SkImageFilter produced above is non-null, since the
        // layer's FilterEffectBuilder can have a stale representation of
        // the layer's filter. See crbug.com/502026.
        if (compositorFilterOperations->isEmpty())
            return;
        LayoutRect visualBounds(rootRelativeBounds);
        if (layer.enclosingPaginationLayer()) {
            // Filters are set up before pagination, so we need to make the bounding box visual on our own.
            visualBounds.moveBy(-offsetFromRoot);
            layer.convertFromFlowThreadToVisualBoundingBoxInAncestor(paintingInfo.rootLayer, visualBounds);
        }
        context.getPaintController().createAndAppend<BeginFilterDisplayItem>(*m_layoutObject, std::move(imageFilter), FloatRect(visualBounds), std::move(compositorFilterOperations));
    }

    m_filterInProgress = true;
}

FilterPainter::~FilterPainter()
{
    if (!m_filterInProgress)
        return;

    m_context.getPaintController().endItem<EndFilterDisplayItem>(*m_layoutObject);
}

} // namespace blink
