/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#include "core/layout/svg/LayoutSVGResourceFilter.h"

#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"

namespace blink {

DEFINE_TRACE(FilterData)
{
    visitor->trace(filter);
    visitor->trace(nodeMap);
}

void FilterData::dispose()
{
    nodeMap = nullptr;
    filter = nullptr;
}

LayoutSVGResourceFilter::LayoutSVGResourceFilter(SVGFilterElement* node)
    : LayoutSVGResourceContainer(node)
{
}

LayoutSVGResourceFilter::~LayoutSVGResourceFilter()
{
}

void LayoutSVGResourceFilter::disposeFilterMap()
{
    for (auto& filter : m_filter)
        filter.value->dispose();
    m_filter.clear();
}

void LayoutSVGResourceFilter::willBeDestroyed()
{
    disposeFilterMap();
    LayoutSVGResourceContainer::willBeDestroyed();
}

bool LayoutSVGResourceFilter::isChildAllowed(LayoutObject* child, const ComputedStyle&) const
{
    return child->isSVGResourceFilterPrimitive();
}

void LayoutSVGResourceFilter::removeAllClientsFromCache(bool markForInvalidation)
{
    // LayoutSVGResourceFilter::removeClientFromCache will be called for
    // all clients through markAllClientsForInvalidation so no explicit
    // display item invalidation is needed here.
    disposeFilterMap();
    markAllClientsForInvalidation(markForInvalidation ? LayoutAndBoundariesInvalidation : ParentOnlyInvalidation);
}

void LayoutSVGResourceFilter::removeClientFromCache(LayoutObject* client, bool markForInvalidation)
{
    ASSERT(client);

    bool filterCached = m_filter.contains(client);
    if (filterCached)
        m_filter.remove(client);

    // If the filter has a cached subtree, invalidate the associated display item.
    if (markForInvalidation && filterCached)
        markClientForInvalidation(client, PaintInvalidation);

    markClientForInvalidation(client, markForInvalidation ? BoundariesInvalidation : ParentOnlyInvalidation);
}

FloatRect LayoutSVGResourceFilter::resourceBoundingBox(const LayoutObject* object)
{
    if (SVGFilterElement* element = toSVGFilterElement(this->element()))
        return SVGLengthContext::resolveRectangle<SVGFilterElement>(element, element->filterUnits()->currentValue()->enumValue(), object->objectBoundingBox());

    return FloatRect();
}

void LayoutSVGResourceFilter::primitiveAttributeChanged(LayoutObject* object, const QualifiedName& attribute)
{
    SVGFilterPrimitiveStandardAttributes* primitive = static_cast<SVGFilterPrimitiveStandardAttributes*>(object->node());

    for (auto& filter : m_filter) {
        FilterData* filterData = filter.value.get();
        if (filterData->m_state != FilterData::ReadyToPaint)
            continue;

        SVGFilterGraphNodeMap* nodeMap = filterData->nodeMap.get();
        FilterEffect* effect = nodeMap->effectByRenderer(object);
        if (!effect)
            continue;
        // Since all effects shares the same attribute value, all
        // or none of them will be changed.
        if (!primitive->setFilterEffectAttribute(effect, attribute))
            return;
        nodeMap->invalidateDependentEffects(effect);

        // Issue paint invalidations for the image on the screen.
        markClientForInvalidation(filter.key, PaintInvalidation);
    }
    markAllResourceClientsForInvalidation();
}

} // namespace blink
