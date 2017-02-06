/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#ifndef SVGUseElement_h
#define SVGUseElement_h

#include "core/events/EventSender.h"
#include "core/fetch/DocumentResource.h"
#include "core/svg/SVGAnimatedLength.h"
#include "core/svg/SVGGeometryElement.h"
#include "core/svg/SVGGraphicsElement.h"
#include "core/svg/SVGURIReference.h"
#include "platform/heap/Handle.h"

namespace blink {

using SVGUseEventSender = EventSender<SVGUseElement>;

class SVGUseElement final : public SVGGraphicsElement,
    public SVGURIReference,
    public DocumentResourceClient {

    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(SVGUseElement);
    USING_PRE_FINALIZER(SVGUseElement, dispose);
public:
    static SVGUseElement* create(Document&);
    ~SVGUseElement() override;

    void invalidateShadowTree();

    // Return the element that should be used for clipping,
    // or null if a valid clip element is not directly referenced.
    SVGGraphicsElement* visibleTargetGraphicsElementForClipping() const;

    SVGAnimatedLength* x() const { return m_x.get(); }
    SVGAnimatedLength* y() const { return m_y.get(); }
    SVGAnimatedLength* width() const { return m_width.get(); }
    SVGAnimatedLength* height() const { return m_height.get(); }

    void buildPendingResource() override;

    void dispatchPendingEvent(SVGUseEventSender*);
    void toClipPath(Path&) const;

    DECLARE_VIRTUAL_TRACE();

private:
    explicit SVGUseElement(Document&);

    void dispose();

    FloatRect getBBox() override;

    bool isPresentationAttribute(const QualifiedName&) const override;
    void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) override;
    bool isPresentationAttributeWithSVGDOM(const QualifiedName&) const override;

    bool isStructurallyExternal() const override { return !hrefString().isNull() && isExternalURIReference(hrefString(), document()); }

    InsertionNotificationRequest insertedInto(ContainerNode*) override;
    void removedFrom(ContainerNode*) override;

    void svgAttributeChanged(const QualifiedName&) override;

    LayoutObject* createLayoutObject(const ComputedStyle&) override;

    void scheduleShadowTreeRecreation();
    void cancelShadowTreeRecreation();
    bool haveLoadedRequiredResources() override { return !isStructurallyExternal() || m_haveFiredLoadEvent; }

    bool selfHasRelativeLengths() const override;

    // Instance tree handling
    void buildShadowAndInstanceTree(SVGElement& target);
    void clearInstanceRoot();
    Element* createInstanceTree(SVGElement& targetRoot) const;
    void clearShadowTree();
    bool hasCycleUseReferencing(const SVGUseElement&, const ContainerNode& targetInstance, SVGElement*& newTarget) const;
    bool expandUseElementsInShadowTree();
    void addReferencesToFirstDegreeNestedUseElements(SVGElement& target);

    void invalidateDependentShadowTrees();

    bool resourceIsStillLoading() const;
    bool resourceIsValid() const;
    Document* externalDocument() const;
    bool instanceTreeIsLoading() const;
    void notifyFinished(Resource*) override;
    String debugName() const override { return "SVGUseElement"; }
    void setDocumentResource(DocumentResource*);

    Member<SVGAnimatedLength> m_x;
    Member<SVGAnimatedLength> m_y;
    Member<SVGAnimatedLength> m_width;
    Member<SVGAnimatedLength> m_height;

    bool m_haveFiredLoadEvent;
    bool m_needsShadowTreeRecreation;
    Member<SVGElement> m_targetElementInstance;
    Member<DocumentResource> m_resource;
};

} // namespace blink

#endif // SVGUseElement_h
