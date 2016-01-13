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

#include "core/SVGNames.h"
#include "core/fetch/DocumentResource.h"
#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGAnimatedLength.h"
#include "core/svg/SVGGraphicsElement.h"
#include "core/svg/SVGURIReference.h"

namespace WebCore {

class DocumentResource;

class SVGUseElement FINAL : public SVGGraphicsElement,
                            public SVGURIReference,
                            public DocumentResourceClient {
public:
    static PassRefPtrWillBeRawPtr<SVGUseElement> create(Document&);
    virtual ~SVGUseElement();

    void invalidateShadowTree();

    RenderObject* rendererClipChild() const;

    SVGAnimatedLength* x() const { return m_x.get(); }
    SVGAnimatedLength* y() const { return m_y.get(); }
    SVGAnimatedLength* width() const { return m_width.get(); }
    SVGAnimatedLength* height() const { return m_height.get(); }

    virtual void buildPendingResource() OVERRIDE;

    virtual void trace(Visitor*) OVERRIDE;

private:
    explicit SVGUseElement(Document&);

    virtual bool isStructurallyExternal() const OVERRIDE { return !hrefString().isNull() && isExternalURIReference(hrefString(), document()); }

    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE;

    virtual RenderObject* createRenderer(RenderStyle*) OVERRIDE;
    virtual void toClipPath(Path&) OVERRIDE;

    void clearResourceReferences();
    void buildShadowAndInstanceTree(SVGElement* target);

    void scheduleShadowTreeRecreation();
    virtual bool haveLoadedRequiredResources() OVERRIDE { return !isStructurallyExternal() || m_haveFiredLoadEvent; }

    virtual bool selfHasRelativeLengths() const OVERRIDE;

    // Instance tree handling
    bool buildShadowTree(SVGElement* target, SVGElement* targetInstance, bool foundUse);
    bool hasCycleUseReferencing(SVGUseElement*, ContainerNode* targetInstance, SVGElement*& newTarget);
    bool expandUseElementsInShadowTree(SVGElement*);
    void expandSymbolElementsInShadowTree(SVGElement*);

    void transferUseAttributesToReplacedElement(SVGElement* from, SVGElement* to) const;

    void invalidateDependentShadowTrees();

    bool resourceIsStillLoading();
    Document* externalDocument() const;
    bool instanceTreeIsLoading(SVGElement*);
    virtual void notifyFinished(Resource*) OVERRIDE;
    TreeScope* referencedScope() const;
    void setDocumentResource(ResourcePtr<DocumentResource>);

    virtual Timer<SVGElement>* svgLoadEventTimer() OVERRIDE { return &m_svgLoadEventTimer; }

    RefPtr<SVGAnimatedLength> m_x;
    RefPtr<SVGAnimatedLength> m_y;
    RefPtr<SVGAnimatedLength> m_width;
    RefPtr<SVGAnimatedLength> m_height;

    bool m_haveFiredLoadEvent;
    bool m_needsShadowTreeRecreation;
    RefPtrWillBeMember<SVGElement> m_targetElementInstance;
    ResourcePtr<DocumentResource> m_resource;
    Timer<SVGElement> m_svgLoadEventTimer;
};

}

#endif
