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

#ifndef RenderSVGResourceContainer_h
#define RenderSVGResourceContainer_h

#include "core/rendering/svg/RenderSVGHiddenContainer.h"
#include "core/rendering/svg/RenderSVGResource.h"

namespace WebCore {

class RenderLayer;

class RenderSVGResourceContainer : public RenderSVGHiddenContainer,
                                   public RenderSVGResource {
public:
    explicit RenderSVGResourceContainer(SVGElement*);
    virtual ~RenderSVGResourceContainer();

    virtual void layout() OVERRIDE;
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) OVERRIDE FINAL;

    virtual bool isSVGResourceContainer() const OVERRIDE FINAL { return true; }

    static AffineTransform transformOnNonScalingStroke(RenderObject*, const AffineTransform& resourceTransform);

    void idChanged();
    void addClientRenderLayer(Node*);
    void addClientRenderLayer(RenderLayer*);
    void removeClientRenderLayer(RenderLayer*);

    void invalidateCacheAndMarkForLayout(SubtreeLayoutScope* = 0);

protected:
    // When adding modes, make sure we don't overflow m_invalidationMask below.
    enum InvalidationMode {
        LayoutAndBoundariesInvalidation = 1 << 0,
        BoundariesInvalidation = 1 << 1,
        RepaintInvalidation = 1 << 2,
        ParentOnlyInvalidation = 1 << 3
    };

    // Used from the invalidateClient/invalidateClients methods from classes, inheriting from us.
    void markAllClientsForInvalidation(InvalidationMode);
    void markAllClientLayersForInvalidation();
    void markClientForInvalidation(RenderObject*, InvalidationMode);

    void clearInvalidationMask() { m_invalidationMask = 0; }

    static AffineTransform computeResourceSpaceTransform(RenderObject*, const AffineTransform& baseTransform, const SVGRenderStyle*, unsigned short resourceMode);

    bool m_isInLayout;

private:
    friend class SVGResourcesCache;
    void addClient(RenderObject*);
    void removeClient(RenderObject*);

    virtual void willBeDestroyed() OVERRIDE FINAL;
    void registerResource();

    AtomicString m_id;
    // Track global (markAllClientsForInvalidation) invals to avoid redundant crawls.
    unsigned m_invalidationMask : 8;

    bool m_registered : 1;
    bool m_isInvalidating : 1;
    // 22 padding bits available

    HashSet<RenderObject*> m_clients;
    HashSet<RenderLayer*> m_clientLayers;
};

inline RenderSVGResourceContainer* getRenderSVGResourceContainerById(TreeScope& treeScope, const AtomicString& id)
{
    if (id.isEmpty())
        return 0;

    if (RenderSVGResourceContainer* renderResource = treeScope.document().accessSVGExtensions().resourceById(id))
        return renderResource;

    return 0;
}

template<typename Renderer>
Renderer* getRenderSVGResourceById(TreeScope& treeScope, const AtomicString& id)
{
    if (RenderSVGResourceContainer* container = getRenderSVGResourceContainerById(treeScope, id))
        return container->cast<Renderer>();

    return 0;
}

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderSVGResourceContainer, isSVGResourceContainer());

}

#endif
