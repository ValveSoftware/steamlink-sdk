/*
 * Copyright (C) 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/rendering/compositing/GraphicsLayerUpdater.h"

#include "core/html/HTMLMediaElement.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderLayerReflectionInfo.h"
#include "core/rendering/RenderPart.h"
#include "core/rendering/compositing/CompositedLayerMapping.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"

namespace WebCore {

GraphicsLayerUpdater::UpdateContext::UpdateContext(const UpdateContext& other, const RenderLayer& layer)
    : m_compositingStackingContext(other.m_compositingStackingContext)
    , m_compositingAncestor(other.compositingContainer(layer))
{
    CompositingState compositingState = layer.compositingState();
    if (compositingState != NotComposited && compositingState != PaintsIntoGroupedBacking) {
        m_compositingAncestor = &layer;
        if (layer.stackingNode()->isStackingContext())
            m_compositingStackingContext = &layer;
    }
}

const RenderLayer* GraphicsLayerUpdater::UpdateContext::compositingContainer(const RenderLayer& layer) const
{
    return layer.stackingNode()->isNormalFlowOnly() ? m_compositingAncestor : m_compositingStackingContext;
}

GraphicsLayerUpdater::GraphicsLayerUpdater()
    : m_needsRebuildTree(false)
{
}

GraphicsLayerUpdater::~GraphicsLayerUpdater()
{
}

void GraphicsLayerUpdater::update(Vector<RenderLayer*>& layersNeedingPaintInvalidation, RenderLayer& layer, UpdateType updateType, const UpdateContext& context)
{
    if (layer.hasCompositedLayerMapping()) {
        CompositedLayerMappingPtr mapping = layer.compositedLayerMapping();

        const RenderLayer* compositingContainer = context.compositingContainer(layer);
        ASSERT(compositingContainer == layer.ancestorCompositingLayer());
        if (mapping->updateRequiresOwnBackingStoreForAncestorReasons(compositingContainer))
            updateType = ForceUpdate;

        // Note carefully: here we assume that the compositing state of all descendants have been updated already,
        // so it is legitimate to compute and cache the composited bounds for this layer.
        mapping->updateCompositedBounds(updateType);

        if (RenderLayerReflectionInfo* reflection = layer.reflectionInfo()) {
            if (reflection->reflectionLayer()->hasCompositedLayerMapping())
                reflection->reflectionLayer()->compositedLayerMapping()->updateCompositedBounds(ForceUpdate);
        }

        if (mapping->updateGraphicsLayerConfiguration(updateType))
            m_needsRebuildTree = true;

        mapping->updateGraphicsLayerGeometry(updateType, compositingContainer, layersNeedingPaintInvalidation);

        updateType = mapping->updateTypeForChildren(updateType);
        mapping->clearNeedsGraphicsLayerUpdate();

        if (!layer.parent())
            layer.compositor()->updateRootLayerPosition();

        if (mapping->hasUnpositionedOverflowControlsLayers())
            layer.scrollableArea()->positionOverflowControls(IntSize());
    }

    UpdateContext childContext(context, layer);
    for (RenderLayer* child = layer.firstChild(); child; child = child->nextSibling())
        update(layersNeedingPaintInvalidation, *child, updateType, childContext);
}

#if ASSERT_ENABLED

void GraphicsLayerUpdater::assertNeedsToUpdateGraphicsLayerBitsCleared(RenderLayer& layer)
{
    if (layer.hasCompositedLayerMapping())
        layer.compositedLayerMapping()->assertNeedsToUpdateGraphicsLayerBitsCleared();

    for (RenderLayer* child = layer.firstChild(); child; child = child->nextSibling())
        assertNeedsToUpdateGraphicsLayerBitsCleared(*child);
}

#endif

} // namespace WebCore
