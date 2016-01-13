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

#include "platform/geometry/FloatPoint.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/text/AtomicStringHash.h"

namespace WebCore {

class Document;
class RenderSVGResourceContainer;
class SubtreeLayoutScope;
class SVGElement;
#if ENABLE(SVG_FONTS)
class SVGFontFaceElement;
#endif
class SVGResourcesCache;
class SVGSVGElement;
class Element;

typedef WillBeHeapHashSet<RawPtrWillBeMember<SVGElement> > SVGElementSet;

class SVGDocumentExtensions : public NoBaseWillBeGarbageCollectedFinalized<SVGDocumentExtensions> {
    WTF_MAKE_NONCOPYABLE(SVGDocumentExtensions); WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED;
public:
    typedef HashSet<Element*> SVGPendingElements;
    explicit SVGDocumentExtensions(Document*);
    ~SVGDocumentExtensions();

    void addTimeContainer(SVGSVGElement*);
    void removeTimeContainer(SVGSVGElement*);

    void addResource(const AtomicString& id, RenderSVGResourceContainer*);
    void removeResource(const AtomicString& id);
    RenderSVGResourceContainer* resourceById(const AtomicString& id) const;

    static void serviceOnAnimationFrame(Document&, double monotonicAnimationStartTime);

    void startAnimations();
    void pauseAnimations();
    void dispatchSVGLoadEventToOutermostSVGElements();

    void reportWarning(const String&);
    void reportError(const String&);

    SVGResourcesCache* resourcesCache() const { return m_resourcesCache.get(); }

    SVGElementSet* setOfElementsReferencingTarget(SVGElement* referencedElement) const;
    void addElementReferencingTarget(SVGElement* referencingElement, SVGElement* referencedElement);
    void removeAllTargetReferencesForElement(SVGElement*);
    void rebuildAllElementReferencesForTarget(SVGElement*);
    void removeAllElementReferencesForTarget(SVGElement*);

    void addSVGRootWithRelativeLengthDescendents(SVGSVGElement*);
    void removeSVGRootWithRelativeLengthDescendents(SVGSVGElement*);
    bool isSVGRootWithRelativeLengthDescendents(SVGSVGElement*) const;
    void invalidateSVGRootsWithRelativeLengthDescendents(SubtreeLayoutScope*);

#if ENABLE(SVG_FONTS)
    const WillBeHeapHashSet<RawPtrWillBeMember<SVGFontFaceElement> >& svgFontFaceElements() const { return m_svgFontFaceElements; }
    void registerSVGFontFaceElement(SVGFontFaceElement*);
    void unregisterSVGFontFaceElement(SVGFontFaceElement*);

    void registerPendingSVGFontFaceElementsForRemoval(PassRefPtrWillBeRawPtr<SVGFontFaceElement>);
    void removePendingSVGFontFaceElementsForRemoval();
#endif

    bool zoomAndPanEnabled() const;

    void startPan(const FloatPoint& start);
    void updatePan(const FloatPoint& pos) const;

    static SVGSVGElement* rootElement(const Document&);
    SVGSVGElement* rootElement() const;

    void trace(Visitor*);

private:
    RawPtrWillBeMember<Document> m_document;
    WillBeHeapHashSet<RawPtrWillBeMember<SVGSVGElement> > m_timeContainers; // For SVG 1.2 support this will need to be made more general.
#if ENABLE(SVG_FONTS)
    WillBeHeapHashSet<RawPtrWillBeMember<SVGFontFaceElement> > m_svgFontFaceElements;
    // SVGFontFaceElements that are pending and scheduled for removal.
    WillBeHeapHashSet<RefPtrWillBeMember<SVGFontFaceElement> > m_pendingSVGFontFaceElementsForRemoval;
#endif
    HashMap<AtomicString, RenderSVGResourceContainer*> m_resources;
    HashMap<AtomicString, OwnPtr<SVGPendingElements> > m_pendingResources; // Resources that are pending.
    HashMap<AtomicString, OwnPtr<SVGPendingElements> > m_pendingResourcesForRemoval; // Resources that are pending and scheduled for removal.
    typedef WillBeHeapHashMap<RawPtrWillBeMember<SVGElement>, OwnPtrWillBeMember<SVGElementSet> > ElementDependenciesMap;
    ElementDependenciesMap m_elementDependencies;
    OwnPtr<SVGResourcesCache> m_resourcesCache;
    WillBeHeapHashSet<RawPtrWillBeMember<SVGSVGElement> > m_relativeLengthSVGRoots; // Root SVG elements with relative length descendants.
    FloatPoint m_translate;
#if ASSERT_ENABLED
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
    PassOwnPtr<SVGPendingElements> removePendingResource(const AtomicString& id);

    void serviceAnimations(double monotonicAnimationStartTime);

    // The following two functions are used for scheduling a pending resource to be removed.
    void markPendingResourcesForRemoval(const AtomicString&);
    Element* removeElementFromPendingResourcesForRemoval(const AtomicString&);

private:
    PassOwnPtr<SVGPendingElements> removePendingResourceForRemoval(const AtomicString&);
};

}

#endif
