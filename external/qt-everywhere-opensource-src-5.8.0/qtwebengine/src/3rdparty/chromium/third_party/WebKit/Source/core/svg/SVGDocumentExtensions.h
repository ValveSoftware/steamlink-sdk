/*
 * Copyright (C) 2006 Apple Inc. All rights reserved.
 * Copyright (C) 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef SVGDocumentExtensions_h
#define SVGDocumentExtensions_h

#include "core/layout/svg/SVGResourcesCache.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

class Document;
class LayoutSVGResourceContainer;
class SubtreeLayoutScope;
class SVGSVGElement;
class SVGElement;
class Element;

class SVGDocumentExtensions : public GarbageCollectedFinalized<SVGDocumentExtensions> {
    WTF_MAKE_NONCOPYABLE(SVGDocumentExtensions);
public:
    typedef HeapHashSet<Member<Element>> SVGPendingElements;
    explicit SVGDocumentExtensions(Document*);
    ~SVGDocumentExtensions();

    void addTimeContainer(SVGSVGElement*);
    void removeTimeContainer(SVGSVGElement*);

    // Records the SVG element as having a Web Animation on an SVG attribute that needs applying.
    void addWebAnimationsPendingSVGElement(SVGElement&);

    void addResource(const AtomicString& id, LayoutSVGResourceContainer*);
    void removeResource(const AtomicString& id);
    LayoutSVGResourceContainer* resourceById(const AtomicString& id) const;

    static void serviceOnAnimationFrame(Document&);

    void startAnimations();
    void pauseAnimations();
    void dispatchSVGLoadEventToOutermostSVGElements();

    void reportError(const String&);

    SVGResourcesCache& resourcesCache() { return m_resourcesCache; }

    void addSVGRootWithRelativeLengthDescendents(SVGSVGElement*);
    void removeSVGRootWithRelativeLengthDescendents(SVGSVGElement*);
    bool isSVGRootWithRelativeLengthDescendents(SVGSVGElement*) const;
    void invalidateSVGRootsWithRelativeLengthDescendents(SubtreeLayoutScope*);

    bool zoomAndPanEnabled() const;

    void startPan(const FloatPoint& start);
    void updatePan(const FloatPoint& pos) const;

    static SVGSVGElement* rootElement(const Document&);
    SVGSVGElement* rootElement() const;

    DECLARE_TRACE();

private:
    Member<Document> m_document;
    HeapHashSet<Member<SVGSVGElement>> m_timeContainers; // For SVG 1.2 support this will need to be made more general.
    using SVGElementSet = HeapHashSet<Member<SVGElement>>;
    SVGElementSet m_webAnimationsPendingSVGElements;
    HashMap<AtomicString, LayoutSVGResourceContainer*> m_resources;
    HeapHashMap<AtomicString, Member<SVGPendingElements>> m_pendingResources; // Resources that are pending.
    HeapHashMap<AtomicString, Member<SVGPendingElements>> m_pendingResourcesForRemoval; // Resources that are pending and scheduled for removal.
    SVGResourcesCache m_resourcesCache;
    HeapHashSet<Member<SVGSVGElement>> m_relativeLengthSVGRoots; // Root SVG elements with relative length descendants.
    FloatPoint m_translate;
#if ENABLE(ASSERT)
    bool m_inRelativeLengthSVGRootsInvalidation;
#endif

public:
    // This HashMap contains a list of pending resources. Pending resources, are such
    // which are referenced by any object in the SVG document, but do NOT exist yet.
    // For instance, dynamically build gradients / patterns / clippers...
    void addPendingResource(const AtomicString& id, Element*);
    bool hasPendingResource(const AtomicString& id) const;
    bool isElementPendingResources(Element*) const;
    bool isElementPendingResource(Element*, const AtomicString& id) const;
    void clearHasPendingResourcesIfPossible(Element*);
    void removeElementFromPendingResources(Element*);
    SVGPendingElements* removePendingResource(const AtomicString& id);

    void serviceAnimations();

    // The following two functions are used for scheduling a pending resource to be removed.
    void markPendingResourcesForRemoval(const AtomicString&);
    Element* removeElementFromPendingResourcesForRemoval(const AtomicString&);

private:
    SVGPendingElements* removePendingResourceForRemoval(const AtomicString&);
};

} // namespace blink

#endif
