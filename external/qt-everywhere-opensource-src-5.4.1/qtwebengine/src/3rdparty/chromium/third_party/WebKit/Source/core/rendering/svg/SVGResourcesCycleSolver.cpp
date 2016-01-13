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
#include "core/rendering/svg/SVGResourcesCycleSolver.h"

// Set to a value > 0, to debug the resource cache.
#define DEBUG_CYCLE_DETECTION 0

#include "core/rendering/svg/RenderSVGResourceClipper.h"
#include "core/rendering/svg/RenderSVGResourceFilter.h"
#include "core/rendering/svg/RenderSVGResourceMarker.h"
#include "core/rendering/svg/RenderSVGResourceMasker.h"
#include "core/rendering/svg/SVGResources.h"
#include "core/rendering/svg/SVGResourcesCache.h"
#include "core/svg/SVGFilterElement.h"
#include "core/svg/SVGGradientElement.h"
#include "core/svg/SVGPatternElement.h"

namespace WebCore {

SVGResourcesCycleSolver::SVGResourcesCycleSolver(RenderObject* renderer, SVGResources* resources)
    : m_renderer(renderer)
    , m_resources(resources)
{
    ASSERT(m_renderer);
    ASSERT(m_resources);
}

SVGResourcesCycleSolver::~SVGResourcesCycleSolver()
{
}

struct ActiveFrame {
    typedef SVGResourcesCycleSolver::ResourceSet ResourceSet;

    ActiveFrame(ResourceSet& activeSet, RenderSVGResourceContainer* resource)
        : m_activeSet(activeSet)
        , m_resource(resource)
    {
        m_activeSet.add(m_resource);
    }
    ~ActiveFrame()
    {
        m_activeSet.remove(m_resource);
    }

    ResourceSet& m_activeSet;
    RenderSVGResourceContainer* m_resource;
};

bool SVGResourcesCycleSolver::resourceContainsCycles(RenderSVGResourceContainer* resource)
{
    // If we've traversed this sub-graph before and no cycles were observed, then
    // reuse that result.
    if (m_dagCache.contains(resource))
        return false;

    ActiveFrame frame(m_activeResources, resource);

    RenderObject* node = resource;
    while (node) {
        // Skip subtrees which are themselves resources. (They will be
        // processed - if needed - when they are actually referenced.)
        if (node != resource && node->isSVGResourceContainer()) {
            node = node->nextInPreOrderAfterChildren(resource);
            continue;
        }
        if (SVGResources* nodeResources = SVGResourcesCache::cachedResourcesForRenderObject(node)) {
            // Fetch all the resources referenced by |node|.
            ResourceSet nodeSet;
            nodeResources->buildSetOfResources(nodeSet);

            // Iterate resources referenced by |node|.
            ResourceSet::iterator end = nodeSet.end();
            for (ResourceSet::iterator it = nodeSet.begin(); it != end; ++it) {
                if (m_activeResources.contains(*it) || resourceContainsCycles(*it))
                    return true;
            }
        }
        node = node->nextInPreOrder(resource);
    }

    // No cycles found in (or from) this resource. Add it to the "DAG cache".
    m_dagCache.add(resource);
    return false;
}

void SVGResourcesCycleSolver::resolveCycles()
{
    ASSERT(m_activeResources.isEmpty());

    // If the starting RenderObject is a resource container itself, then add it
    // to the active set (to break direct self-references.)
    if (m_renderer->isSVGResourceContainer())
        m_activeResources.add(toRenderSVGResourceContainer(m_renderer));

    ResourceSet localResources;
    m_resources->buildSetOfResources(localResources);

    // This performs a depth-first search for a back-edge in all the
    // (potentially disjoint) graphs formed by the resources referenced by
    // |m_renderer|.
    ResourceSet::iterator end = localResources.end();
    for (ResourceSet::iterator it = localResources.begin(); it != end; ++it) {
        if (m_activeResources.contains(*it) || resourceContainsCycles(*it))
            breakCycle(*it);
    }

    m_activeResources.clear();
}

void SVGResourcesCycleSolver::breakCycle(RenderSVGResourceContainer* resourceLeadingToCycle)
{
    ASSERT(resourceLeadingToCycle);
    if (resourceLeadingToCycle == m_resources->linkedResource()) {
        m_resources->resetLinkedResource();
        return;
    }

    switch (resourceLeadingToCycle->resourceType()) {
    case MaskerResourceType:
        ASSERT(resourceLeadingToCycle == m_resources->masker());
        m_resources->resetMasker();
        break;
    case MarkerResourceType:
        ASSERT(resourceLeadingToCycle == m_resources->markerStart() || resourceLeadingToCycle == m_resources->markerMid() || resourceLeadingToCycle == m_resources->markerEnd());
        if (m_resources->markerStart() == resourceLeadingToCycle)
            m_resources->resetMarkerStart();
        if (m_resources->markerMid() == resourceLeadingToCycle)
            m_resources->resetMarkerMid();
        if (m_resources->markerEnd() == resourceLeadingToCycle)
            m_resources->resetMarkerEnd();
        break;
    case PatternResourceType:
    case LinearGradientResourceType:
    case RadialGradientResourceType:
        ASSERT(resourceLeadingToCycle == m_resources->fill() || resourceLeadingToCycle == m_resources->stroke());
        if (m_resources->fill() == resourceLeadingToCycle)
            m_resources->resetFill();
        if (m_resources->stroke() == resourceLeadingToCycle)
            m_resources->resetStroke();
        break;
    case FilterResourceType:
        ASSERT(resourceLeadingToCycle == m_resources->filter());
        m_resources->resetFilter();
        break;
    case ClipperResourceType:
        ASSERT(resourceLeadingToCycle == m_resources->clipper());
        m_resources->resetClipper();
        break;
    case SolidColorResourceType:
        ASSERT_NOT_REACHED();
        break;
    }
}

}
