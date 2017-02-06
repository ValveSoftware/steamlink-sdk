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

#include "core/layout/svg/LayoutSVGResourceContainer.h"

#include "core/SVGElementTypeHelpers.h"
#include "core/layout/svg/LayoutSVGResourceClipper.h"
#include "core/layout/svg/LayoutSVGResourceFilter.h"
#include "core/layout/svg/LayoutSVGResourceMasker.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/paint/PaintLayer.h"

#include "wtf/TemporaryChange.h"

namespace blink {

static inline SVGDocumentExtensions& svgExtensionsFromElement(Element* element)
{
    ASSERT(element);
    return element->document().accessSVGExtensions();
}

LayoutSVGResourceContainer::LayoutSVGResourceContainer(SVGElement* node)
    : LayoutSVGHiddenContainer(node)
    , m_isInLayout(false)
    , m_id(node->getIdAttribute())
    , m_invalidationMask(0)
    , m_registered(false)
    , m_isInvalidating(false)
{
}

LayoutSVGResourceContainer::~LayoutSVGResourceContainer()
{
}

void LayoutSVGResourceContainer::layout()
{
    // FIXME: Investigate a way to detect and break resource layout dependency cycles early.
    // Then we can remove this method altogether, and fall back onto LayoutSVGHiddenContainer::layout().
    ASSERT(needsLayout());
    if (m_isInLayout)
        return;

    TemporaryChange<bool> inLayoutChange(m_isInLayout, true);

    LayoutSVGHiddenContainer::layout();

    clearInvalidationMask();
}

void LayoutSVGResourceContainer::willBeDestroyed()
{
    // Detach all clients referring to this resource. If the resource itself is
    // a client, it will be detached from any such resources by the call to
    // LayoutSVGHiddenContainer::willBeDestroyed() below.
    detachAllClients();

    for (SVGResourceClient* client : m_resourceClients)
        client->filterWillBeDestroyed(toSVGFilterElement(element()));
    m_resourceClients.clear();

    LayoutSVGHiddenContainer::willBeDestroyed();
    if (m_registered)
        svgExtensionsFromElement(element()).removeResource(m_id);
}

void LayoutSVGResourceContainer::styleDidChange(StyleDifference diff, const ComputedStyle* oldStyle)
{
    LayoutSVGHiddenContainer::styleDidChange(diff, oldStyle);

    if (!m_registered) {
        m_registered = true;
        registerResource();
    }
}

void LayoutSVGResourceContainer::detachAllClients()
{
    for (auto* client : m_clients) {
        // Unlink the resource from the client's SVGResources. (The actual
        // removal will be signaled after processing all the clients.)
        SVGResources* resources = SVGResourcesCache::cachedResourcesForLayoutObject(client);
        ASSERT(resources); // Or else the client wouldn't be in the list in the first place.
        resources->resourceDestroyed(this);

        // Add a pending resolution based on the id of the old resource.
        Element* clientElement = toElement(client->node());
        svgExtensionsFromElement(clientElement).addPendingResource(m_id, clientElement);
    }

    removeAllClientsFromCache();
}

void LayoutSVGResourceContainer::idChanged()
{
    // Invalidate all our current clients.
    removeAllClientsFromCache();

    // Remove old id, that is guaranteed to be present in cache.
    SVGDocumentExtensions& extensions = svgExtensionsFromElement(element());
    extensions.removeResource(m_id);
    m_id = element()->getIdAttribute();

    registerResource();
}

void LayoutSVGResourceContainer::markAllClientsForInvalidation(InvalidationMode mode)
{
    if ((m_clients.isEmpty() && m_resourceClients.isEmpty()) || m_isInvalidating)
        return;

    if (m_invalidationMask & mode)
        return;

    m_invalidationMask |= mode;
    m_isInvalidating = true;
    bool needsLayout = mode == LayoutAndBoundariesInvalidation;
    bool markForInvalidation = mode != ParentOnlyInvalidation;

    for (auto* client : m_clients) {
        if (client->isSVGResourceContainer()) {
            toLayoutSVGResourceContainer(client)->removeAllClientsFromCache(markForInvalidation);
            continue;
        }

        if (markForInvalidation)
            markClientForInvalidation(client, mode);

        LayoutSVGResourceContainer::markForLayoutAndParentResourceInvalidation(client, needsLayout);
    }

    markAllResourceClientsForInvalidation();

    m_isInvalidating = false;
}

void LayoutSVGResourceContainer::markAllResourceClientsForInvalidation()
{
    for (SVGResourceClient* client : m_resourceClients)
        client->filterNeedsInvalidation();
}

void LayoutSVGResourceContainer::markClientForInvalidation(LayoutObject* client, InvalidationMode mode)
{
    ASSERT(client);
    ASSERT(!m_clients.isEmpty());

    switch (mode) {
    case LayoutAndBoundariesInvalidation:
    case BoundariesInvalidation:
        client->setNeedsBoundariesUpdate();
        break;
    case PaintInvalidation:
        // Since LayoutSVGInlineTexts don't have SVGResources (they use their
        // parent's), they will not be notified of changes to paint servers. So
        // if the client is one that could have a LayoutSVGInlineText use a
        // paint invalidation reason that will force paint invalidation of the
        // entire <text>/<tspan>/... subtree.
        client->setShouldDoFullPaintInvalidation(PaintInvalidationSVGResourceChange);
        break;
    case ParentOnlyInvalidation:
        break;
    }
}

void LayoutSVGResourceContainer::addClient(LayoutObject* client)
{
    ASSERT(client);
    m_clients.add(client);
    clearInvalidationMask();
}

void LayoutSVGResourceContainer::removeClient(LayoutObject* client)
{
    ASSERT(client);
    removeClientFromCache(client, false);
    m_clients.remove(client);
}

void LayoutSVGResourceContainer::addResourceClient(SVGResourceClient* client)
{
    ASSERT(client);
    m_resourceClients.add(client);
    clearInvalidationMask();
}

void LayoutSVGResourceContainer::removeResourceClient(SVGResourceClient* client)
{
    ASSERT(client);
    m_resourceClients.remove(client);
}

void LayoutSVGResourceContainer::invalidateCacheAndMarkForLayout(SubtreeLayoutScope* layoutScope)
{
    if (selfNeedsLayout())
        return;

    setNeedsLayoutAndFullPaintInvalidation(LayoutInvalidationReason::SvgResourceInvalidated, MarkContainerChain, layoutScope);

    if (everHadLayout())
        removeAllClientsFromCache();
}

void LayoutSVGResourceContainer::registerResource()
{
    SVGDocumentExtensions& extensions = svgExtensionsFromElement(element());
    if (!extensions.hasPendingResource(m_id)) {
        extensions.addResource(m_id, this);
        return;
    }

    SVGDocumentExtensions::SVGPendingElements* clients(extensions.removePendingResource(m_id));

    // Cache us with the new id.
    extensions.addResource(m_id, this);

    // Update cached resources of pending clients.
    for (const auto& pendingClient : *clients) {
        ASSERT(pendingClient->hasPendingResources());
        extensions.clearHasPendingResourcesIfPossible(pendingClient);
        LayoutObject* layoutObject = pendingClient->layoutObject();
        if (!layoutObject)
            continue;

        const ComputedStyle& style = layoutObject->styleRef();

        // If the client has a layer (is a non-SVGElement) we need to signal
        // invalidation in the same way as is done in markAllResourceClientsForInvalidation above.
        if (layoutObject->hasLayer() && resourceType() == FilterResourceType) {
            if (!style.hasFilter())
                continue;
            toLayoutBoxModelObject(layoutObject)->layer()->filterNeedsPaintInvalidation();
            if (!layoutObject->isSVGRoot())
                continue;
            // A root SVG element with a filter, however, still needs to run
            // the full invalidation step below.
        }

        StyleDifference diff;
        diff.setNeedsFullLayout();
        SVGResourcesCache::clientStyleChanged(layoutObject, diff, style);
        layoutObject->setNeedsLayoutAndFullPaintInvalidation(LayoutInvalidationReason::SvgResourceInvalidated);
    }
}

static inline void removeFromCacheAndInvalidateDependencies(LayoutObject* object, bool needsLayout)
{
    ASSERT(object);
    if (SVGResources* resources = SVGResourcesCache::cachedResourcesForLayoutObject(object)) {
        if (LayoutSVGResourceFilter* filter = resources->filter())
            filter->removeClientFromCache(object);

        if (LayoutSVGResourceMasker* masker = resources->masker())
            masker->removeClientFromCache(object);

        if (LayoutSVGResourceClipper* clipper = resources->clipper())
            clipper->removeClientFromCache(object);
    }

    if (!object->node() || !object->node()->isSVGElement())
        return;

    SVGElementSet* dependencies = toSVGElement(object->node())->setOfIncomingReferences();
    if (!dependencies)
        return;

    // We allow cycles in SVGDocumentExtensions reference sets in order to avoid expensive
    // reference graph adjustments on changes, so we need to break possible cycles here.
    // This strong reference is safe, as it is guaranteed that this set will be emptied
    // at the end of recursion.
    DEFINE_STATIC_LOCAL(SVGElementSet, invalidatingDependencies, (new SVGElementSet));

    for (SVGElement* element : *dependencies) {
        if (LayoutObject* layoutObject = element->layoutObject()) {
            if (UNLIKELY(!invalidatingDependencies.add(element).isNewEntry)) {
                // Reference cycle: we are in process of invalidating this dependant.
                continue;
            }

            LayoutSVGResourceContainer::markForLayoutAndParentResourceInvalidation(layoutObject, needsLayout);
            invalidatingDependencies.remove(element);
        }
    }
}

void LayoutSVGResourceContainer::markForLayoutAndParentResourceInvalidation(LayoutObject* object, bool needsLayout)
{
    ASSERT(object);
    ASSERT(object->node());

    if (needsLayout && !object->documentBeingDestroyed())
        object->setNeedsLayoutAndFullPaintInvalidation(LayoutInvalidationReason::SvgResourceInvalidated);

    removeFromCacheAndInvalidateDependencies(object, needsLayout);

    // Invalidate resources in ancestor chain, if needed.
    LayoutObject* current = object->parent();
    while (current) {
        removeFromCacheAndInvalidateDependencies(current, needsLayout);

        if (current->isSVGResourceContainer()) {
            // This will process the rest of the ancestors.
            toLayoutSVGResourceContainer(current)->removeAllClientsFromCache();
            break;
        }

        current = current->parent();
    }
}

} // namespace blink
