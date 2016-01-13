// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/rendering/compositing/CompositingInputsUpdater.h"

#include "core/rendering/RenderBlock.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/compositing/CompositedLayerMapping.h"

namespace WebCore {

CompositingInputsUpdater::CompositingInputsUpdater(RenderLayer* rootRenderLayer)
    : m_geometryMap(UseTransforms)
    , m_rootRenderLayer(rootRenderLayer)
{
    rootRenderLayer->updateDescendantDependentFlags();
}

CompositingInputsUpdater::~CompositingInputsUpdater()
{
}

void CompositingInputsUpdater::update(RenderLayer* layer, UpdateType updateType, AncestorInfo info)
{
    if (!layer->childNeedsCompositingInputsUpdate() && updateType != ForceUpdate)
        return;

    m_geometryMap.pushMappingsToAncestor(layer, layer->parent());

    if (layer->hasCompositedLayerMapping())
        info.enclosingCompositedLayer = layer;

    if (layer->needsCompositingInputsUpdate()) {
        if (info.enclosingCompositedLayer)
            info.enclosingCompositedLayer->compositedLayerMapping()->setNeedsGraphicsLayerUpdate(GraphicsLayerUpdateSubtree);
        updateType = ForceUpdate;
    }

    if (updateType == ForceUpdate) {
        RenderLayer::CompositingInputs properties;

        if (!layer->isRootLayer()) {
            properties.clippedAbsoluteBoundingBox = enclosingIntRect(m_geometryMap.absoluteRect(layer->boundingBoxForCompositingOverlapTest()));
            // FIXME: Setting the absBounds to 1x1 instead of 0x0 makes very little sense,
            // but removing this code will make JSGameBench sad.
            // See https://codereview.chromium.org/13912020/
            if (properties.clippedAbsoluteBoundingBox.isEmpty())
                properties.clippedAbsoluteBoundingBox.setSize(IntSize(1, 1));

            IntRect clipRect = pixelSnappedIntRect(layer->clipper().backgroundClipRect(ClipRectsContext(m_rootRenderLayer, AbsoluteClipRects)).rect());
            properties.clippedAbsoluteBoundingBox.intersect(clipRect);

            const RenderLayer* parent = layer->parent();
            properties.opacityAncestor = parent->isTransparent() ? parent : parent->compositingInputs().opacityAncestor;
            properties.transformAncestor = parent->transform() ? parent : parent->compositingInputs().transformAncestor;
            properties.filterAncestor = parent->hasFilter() ? parent : parent->compositingInputs().filterAncestor;

            if (layer->renderer()->isOutOfFlowPositioned() && info.ancestorScrollingLayer && !layer->subtreeIsInvisible()) {
                const RenderObject* container = layer->renderer()->containingBlock();
                const RenderObject* scroller = info.ancestorScrollingLayer->renderer();
                properties.isUnclippedDescendant = scroller != container && scroller->isDescendantOf(container);
            }
        }

        layer->updateCompositingInputs(properties);
    }

    if (layer->scrollsOverflow())
        info.ancestorScrollingLayer = layer;

    for (RenderLayer* child = layer->firstChild(); child; child = child->nextSibling())
        update(child, updateType, info);

    m_geometryMap.popMappingsToAncestor(layer->parent());

    layer->clearChildNeedsCompositingInputsUpdate();
}

#if ASSERT_ENABLED

void CompositingInputsUpdater::assertNeedsCompositingInputsUpdateBitsCleared(RenderLayer* layer)
{
    ASSERT(!layer->childNeedsCompositingInputsUpdate());
    ASSERT(!layer->needsCompositingInputsUpdate());

    for (RenderLayer* child = layer->firstChild(); child; child = child->nextSibling())
        assertNeedsCompositingInputsUpdateBitsCleared(child);
}

#endif

} // namespace WebCore
