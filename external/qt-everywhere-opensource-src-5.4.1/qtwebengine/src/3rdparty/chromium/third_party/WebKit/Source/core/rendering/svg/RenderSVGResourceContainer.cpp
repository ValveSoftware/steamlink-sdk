/*
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

#include "core/rendering/svg/RenderSVGResourceContainer.h"

#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/svg/SVGRenderingContext.h"
#include "core/rendering/svg/SVGResourcesCache.h"
#include "core/svg/SVGGraphicsElement.h"

#include "wtf/TemporaryChange.h"

namespace WebCore {

static inline SVGDocumentExtensions& svgExtensionsFromElement(SVGElement* element)
{
    ASSERT(element);
    return element->document().accessSVGExtensions();
}

RenderSVGResourceContainer::RenderSVGResourceContainer(SVGElement* node)
    : RenderSVGHiddenContainer(node)
    , m_isInLayout(false)
    , m_id(node->getIdAttribute())
    , m_invalidationMask(0)
    , m_registered(false)
    , m_isInvalidating(false)
{
}

RenderSVGResourceContainer::~RenderSVGResourceContainer()
{
    if (m_registered)
        svgExtensionsFromElement(element()).removeResource(m_id);
}

void RenderSVGResourceContainer::layout()
{
    // FIXME: Investigate a way to detect and break resource layout dependency cycles early.
    // Then we can remove this method altogether, and fall back onto RenderSVGHiddenContainer::layout().
    ASSERT(needsLayout());
    if (m_isInLayout)
        return;

    TemporaryChange<bool> inLayoutChange(m_isInLayout, true);

    RenderSVGHiddenContainer::layout();

    clearInvalidationMask();
}

void RenderSVGResourceContainer::willBeDestroyed()
{
    SVGResourcesCache::resourceDestroyed(this);
    RenderSVGHiddenContainer::willBeDestroyed();
}

void RenderSVGResourceContainer::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderSVGHiddenContainer::styleDidChange(diff, oldStyle);

    if (!m_registered) {
        m_registered = true;
        registerResource();
    }
}

void RenderSVGResourceContainer::idChanged()
{
    // Invalidate all our current clients.
    removeAllClientsFromCache();

    // Remove old id, that is guaranteed to be present in cache.
    SVGDocumentExtensions& extensions = svgExtensionsFromElement(element());
    extensions.removeResource(m_id);
    m_id = element()->getIdAttribute();

    registerResource();
}

void RenderSVGResourceContainer::markAllClientsForInvalidation(InvalidationMode mode)
{
    if ((m_clients.isEmpty() && m_clientLayers.isEmpty()) || m_isInvalidating)
        return;

    if (m_invalidationMask & mode)
        return;

    m_invalidationMask |= mode;
    m_isInvalidating = true;
    bool needsLayout = mode == LayoutAndBoundariesInvalidation;
    bool markForInvalidation = mode != ParentOnlyInvalidation;

    HashSet<RenderObject*>::iterator end = m_clients.end();
    for (HashSet<RenderObject*>::iterator it = m_clients.begin(); it != end; ++it) {
        RenderObject* client = *it;
        if (client->isSVGResourceContainer()) {
            toRenderSVGResourceContainer(client)->removeAllClientsFromCache(markForInvalidation);
            continue;
        }

        if (markForInvalidation)
            markClientForInvalidation(client, mode);

        RenderSVGResource::markForLayoutAndParentResourceInvalidation(client, needsLayout);
    }

    markAllClientLayersForInvalidation();

    m_isInvalidating = false;
}

void RenderSVGResourceContainer::markAllClientLayersForInvalidation()
{
    HashSet<RenderLayer*>::iterator layerEnd = m_clientLayers.end();
    for (HashSet<RenderLayer*>::iterator it = m_clientLayers.begin(); it != layerEnd; ++it)
        (*it)->filterNeedsRepaint();
}

void RenderSVGResourceContainer::markClientForInvalidation(RenderObject* client, InvalidationMode mode)
{
    ASSERT(client);
    ASSERT(!m_clients.isEmpty());

    switch (mode) {
    case LayoutAndBoundariesInvalidation:
    case BoundariesInvalidation:
        client->setNeedsBoundariesUpdate();
        break;
    case RepaintInvalidation:
        if (client->view()) {
            if (RuntimeEnabledFeatures::repaintAfterLayoutEnabled() && frameView()->isInPerformLayout())
                client->setShouldDoFullPaintInvalidationAfterLayout(true);
            else
                client->paintInvalidationForWholeRenderer();
        }
        break;
    case ParentOnlyInvalidation:
        break;
    }
}

void RenderSVGResourceContainer::addClient(RenderObject* client)
{
    ASSERT(client);
    m_clients.add(client);
    clearInvalidationMask();
}

void RenderSVGResourceContainer::removeClient(RenderObject* client)
{
    ASSERT(client);
    removeClientFromCache(client, false);
    m_clients.remove(client);
}

void RenderSVGResourceContainer::addClientRenderLayer(Node* node)
{
    ASSERT(node);
    if (!node->renderer() || !node->renderer()->hasLayer())
        return;
    m_clientLayers.add(toRenderLayerModelObject(node->renderer())->layer());
    clearInvalidationMask();
}

void RenderSVGResourceContainer::addClientRenderLayer(RenderLayer* client)
{
    ASSERT(client);
    m_clientLayers.add(client);
    clearInvalidationMask();
}

void RenderSVGResourceContainer::removeClientRenderLayer(RenderLayer* client)
{
    ASSERT(client);
    m_clientLayers.remove(client);
}

void RenderSVGResourceContainer::invalidateCacheAndMarkForLayout(SubtreeLayoutScope* layoutScope)
{
    if (selfNeedsLayout())
        return;

    setNeedsLayoutAndFullPaintInvalidation(MarkContainingBlockChain, layoutScope);

    if (everHadLayout())
        removeAllClientsFromCache();
}

void RenderSVGResourceContainer::registerResource()
{
    SVGDocumentExtensions& extensions = svgExtensionsFromElement(element());
    if (!extensions.hasPendingResource(m_id)) {
        extensions.addResource(m_id, this);
        return;
    }

    OwnPtr<SVGDocumentExtensions::SVGPendingElements> clients(extensions.removePendingResource(m_id));

    // Cache us with the new id.
    extensions.addResource(m_id, this);

    // Update cached resources of pending clients.
    const SVGDocumentExtensions::SVGPendingElements::const_iterator end = clients->end();
    for (SVGDocumentExtensions::SVGPendingElements::const_iterator it = clients->begin(); it != end; ++it) {
        ASSERT((*it)->hasPendingResources());
        extensions.clearHasPendingResourcesIfPossible(*it);
        RenderObject* renderer = (*it)->renderer();
        if (!renderer)
            continue;

        StyleDifference diff;
        diff.setNeedsFullLayout();
        SVGResourcesCache::clientStyleChanged(renderer, diff, renderer->style());
        renderer->setNeedsLayoutAndFullPaintInvalidation();
    }
}

static bool shouldTransformOnTextPainting(RenderObject* object, AffineTransform& resourceTransform)
{
    ASSERT(object);

    // This method should only be called for RenderObjects that deal with text rendering. Cmp. RenderObject.h's is*() methods.
    ASSERT(object->isSVGText() || object->isSVGTextPath() || object->isSVGInline());

    // In text drawing, the scaling part of the graphics context CTM is removed, compare SVGInlineTextBox::paintTextWithShadows.
    // So, we use that scaling factor here, too, and then push it down to pattern or gradient space
    // in order to keep the pattern or gradient correctly scaled.
    float scalingFactor = SVGRenderingContext::calculateScreenFontSizeScalingFactor(object);
    if (scalingFactor == 1)
        return false;
    resourceTransform.scale(scalingFactor);
    return true;
}

AffineTransform RenderSVGResourceContainer::computeResourceSpaceTransform(RenderObject* object, const AffineTransform& baseTransform, const SVGRenderStyle* svgStyle, unsigned short resourceMode)
{
    AffineTransform computedSpaceTransform = baseTransform;
    if (resourceMode & ApplyToTextMode) {
        // Depending on the font scaling factor, we may need to apply an
        // additional transform (scale-factor) the paintserver, since text
        // painting removes the scale factor from the context. (See
        // SVGInlineTextBox::paintTextWithShadows.)
        AffineTransform additionalTextTransformation;
        if (shouldTransformOnTextPainting(object, additionalTextTransformation))
            computedSpaceTransform = additionalTextTransformation * computedSpaceTransform;
    }
    if (resourceMode & ApplyToStrokeMode) {
        // Non-scaling stroke needs to reset the transform back to the host transform.
        if (svgStyle->vectorEffect() == VE_NON_SCALING_STROKE)
            computedSpaceTransform = transformOnNonScalingStroke(object, computedSpaceTransform);
    }
    return computedSpaceTransform;
}

// FIXME: This does not belong here.
AffineTransform RenderSVGResourceContainer::transformOnNonScalingStroke(RenderObject* object, const AffineTransform& resourceTransform)
{
    if (!object->isSVGShape())
        return resourceTransform;

    SVGGraphicsElement* element = toSVGGraphicsElement(object->node());
    AffineTransform transform = element->getScreenCTM(SVGGraphicsElement::DisallowStyleUpdate);
    transform *= resourceTransform;
    return transform;
}

}
