/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef SVGElement_h
#define SVGElement_h

#include "core/SVGElementTypeHelpers.h"
#include "core/dom/Element.h"
#include "core/svg/SVGAnimatedString.h"
#include "core/svg/SVGParsingError.h"
#include "core/svg/properties/SVGPropertyInfo.h"
#include "platform/Timer.h"
#include "wtf/HashMap.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

class AffineTransform;
class CSSCursorImageValue;
class Document;
class SVGAnimatedPropertyBase;
class SubtreeLayoutScope;
class SVGCursorElement;
class SVGDocumentExtensions;
class SVGElementRareData;
class SVGFitToViewBox;
class SVGSVGElement;

void mapAttributeToCSSProperty(HashMap<StringImpl*, CSSPropertyID>* propertyNameToIdMap, const QualifiedName& attrName);

class SVGElement : public Element {
public:
    virtual ~SVGElement();
    virtual void attach(const AttachContext&) OVERRIDE;
    virtual void detach(const AttachContext&) OVERRIDE;

    virtual short tabIndex() const OVERRIDE;
    virtual bool supportsFocus() const OVERRIDE { return false; }

    bool isOutermostSVGSVGElement() const;

    virtual String title() const OVERRIDE;
    bool hasRelativeLengths() const { return !m_elementsWithRelativeLengths.isEmpty(); }
    static bool isAnimatableCSSProperty(const QualifiedName&);
    enum CTMScope {
        NearestViewportScope, // Used by SVGGraphicsElement::getCTM()
        ScreenScope, // Used by SVGGraphicsElement::getScreenCTM()
        AncestorScope // Used by SVGSVGElement::get{Enclosure|Intersection}List()
    };
    virtual AffineTransform localCoordinateSpaceTransform(CTMScope) const;
    virtual bool needsPendingResourceHandling() const { return true; }

    bool instanceUpdatesBlocked() const;
    void setInstanceUpdatesBlocked(bool);

    const AtomicString& xmlbase() const;
    void setXMLbase(const AtomicString&);

    const AtomicString& xmllang() const;
    void setXMLlang(const AtomicString&);

    const AtomicString& xmlspace() const;
    void setXMLspace(const AtomicString&);

    SVGSVGElement* ownerSVGElement() const;
    SVGElement* viewportElement() const;

    SVGDocumentExtensions& accessDocumentSVGExtensions();

    virtual bool isSVGGraphicsElement() const { return false; }
    virtual bool isFilterEffect() const { return false; }
    virtual bool isTextContent() const { return false; }
    virtual bool isTextPositioning() const { return false; }
    virtual bool isStructurallyExternal() const { return false; }

    // For SVGTests
    virtual bool isValid() const { return true; }

    virtual void svgAttributeChanged(const QualifiedName&);

    PassRefPtr<SVGAnimatedPropertyBase> propertyFromAttribute(const QualifiedName& attributeName);
    static AnimatedPropertyType animatedPropertyTypeForCSSAttribute(const QualifiedName& attributeName);

    void sendSVGLoadEventIfPossible(bool sendParentLoadEvents = false);
    void sendSVGLoadEventIfPossibleAsynchronously();
    void svgLoadEventTimerFired(Timer<SVGElement>*);
    virtual Timer<SVGElement>* svgLoadEventTimer();

    virtual AffineTransform* supplementalTransform() { return 0; }

    void invalidateSVGAttributes() { ensureUniqueElementData().m_animatedSVGAttributesAreDirty = true; }
    void invalidateSVGPresentationAttributeStyle() { ensureUniqueElementData().m_presentationAttributeStyleIsDirty = true; }

    const WillBeHeapHashSet<RawPtrWillBeWeakMember<SVGElement> >& instancesForElement() const;
    void mapInstanceToElement(SVGElement*);
    void removeInstanceMapping(SVGElement*);

    bool getBoundingBox(FloatRect&);

    void setCursorElement(SVGCursorElement*);
    void setCursorImageValue(CSSCursorImageValue*);

#if !ENABLE(OILPAN)
    void cursorElementRemoved();
    void cursorImageValueRemoved();
#endif

    SVGElement* correspondingElement();
    void setCorrespondingElement(SVGElement*);
    SVGUseElement* correspondingUseElement() const;

    void synchronizeAnimatedSVGAttribute(const QualifiedName&) const;

    virtual PassRefPtr<RenderStyle> customStyleForRenderer() OVERRIDE FINAL;

    virtual void synchronizeRequiredFeatures() { }
    virtual void synchronizeRequiredExtensions() { }
    virtual void synchronizeSystemLanguage() { }

#ifndef NDEBUG
    virtual bool isAnimatableAttribute(const QualifiedName&) const;
#endif

    MutableStylePropertySet* animatedSMILStyleProperties() const;
    MutableStylePropertySet* ensureAnimatedSMILStyleProperties();
    void setUseOverrideComputedStyle(bool);

    virtual bool haveLoadedRequiredResources();

    virtual bool addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture = false) OVERRIDE FINAL;
    virtual bool removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture = false) OVERRIDE FINAL;

    void invalidateRelativeLengthClients(SubtreeLayoutScope* = 0);

    bool isContextElement() const { return m_isContextElement; }
    void setContextElement() { m_isContextElement = true; }

    void addToPropertyMap(PassRefPtr<SVGAnimatedPropertyBase>);

    SVGAnimatedString* className() { return m_className.get(); }

    bool inUseShadowTree() const;

    class InvalidationGuard {
        STACK_ALLOCATED();
        WTF_MAKE_NONCOPYABLE(InvalidationGuard);
    public:
        InvalidationGuard(SVGElement* element) : m_element(element) { }
        ~InvalidationGuard() { m_element->invalidateInstances(); }

    private:
        RawPtrWillBeMember<SVGElement> m_element;
    };

    class InstanceUpdateBlocker {
        STACK_ALLOCATED();
        WTF_MAKE_NONCOPYABLE(InstanceUpdateBlocker);
    public:
        InstanceUpdateBlocker(SVGElement* targetElement);
        ~InstanceUpdateBlocker();

    private:
        RawPtrWillBeMember<SVGElement> m_targetElement;
    };

    void invalidateInstances();

    virtual void trace(Visitor*) OVERRIDE;

    static const AtomicString& eventParameterName();

protected:
    SVGElement(const QualifiedName&, Document&, ConstructionType = CreateSVGElement);

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;

    virtual void attributeChanged(const QualifiedName&, const AtomicString&, AttributeModificationReason = ModifiedDirectly) OVERRIDE;

    virtual bool isPresentationAttribute(const QualifiedName&) const OVERRIDE;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) OVERRIDE;
    virtual bool rendererIsNeeded(const RenderStyle&) OVERRIDE;

    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;
    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0) OVERRIDE;

    static CSSPropertyID cssPropertyIdForSVGAttributeName(const QualifiedName&);
    void updateRelativeLengthsInformation() { updateRelativeLengthsInformation(selfHasRelativeLengths(), this); }
    void updateRelativeLengthsInformation(bool hasRelativeLengths, SVGElement*);

    virtual bool selfHasRelativeLengths() const { return false; }

    SVGElementRareData* ensureSVGRareData();

    inline bool hasSVGRareData() const { return m_SVGRareData; }
    inline SVGElementRareData* svgRareData() const
    {
        ASSERT(m_SVGRareData);
        return m_SVGRareData.get();
    }

    // SVGFitToViewBox::parseAttribute uses reportAttributeParsingError.
    friend class SVGFitToViewBox;
    void reportAttributeParsingError(SVGParsingError, const QualifiedName&, const AtomicString&);
    bool hasFocusEventListeners() const;

private:
    // FIXME: Author shadows should be allowed
    // https://bugs.webkit.org/show_bug.cgi?id=77938
    virtual bool areAuthorShadowsAllowed() const OVERRIDE FINAL { return false; }

    RenderStyle* computedStyle(PseudoId = NOPSEUDO);
    virtual RenderStyle* virtualComputedStyle(PseudoId pseudoElementSpecifier = NOPSEUDO) OVERRIDE FINAL { return computedStyle(pseudoElementSpecifier); }
    virtual void willRecalcStyle(StyleRecalcChange) OVERRIDE;

    void buildPendingResourcesIfNeeded();

    bool supportsSpatialNavigationFocus() const;

    WillBeHeapHashSet<RawPtrWillBeWeakMember<SVGElement> > m_elementsWithRelativeLengths;

    typedef HashMap<QualifiedName, RefPtr<SVGAnimatedPropertyBase> > AttributeToPropertyMap;
    AttributeToPropertyMap m_newAttributeToPropertyMap;

#if ASSERT_ENABLED
    bool m_inRelativeLengthClientsInvalidation;
#endif
    unsigned m_isContextElement : 1;

    OwnPtrWillBeMember<SVGElementRareData> m_SVGRareData;
    RefPtr<SVGAnimatedString> m_className;
};

struct SVGAttributeHashTranslator {
    static unsigned hash(const QualifiedName& key)
    {
        if (key.hasPrefix()) {
            QualifiedNameComponents components = { nullAtom.impl(), key.localName().impl(), key.namespaceURI().impl() };
            return hashComponents(components);
        }
        return DefaultHash<QualifiedName>::Hash::hash(key);
    }
    static bool equal(const QualifiedName& a, const QualifiedName& b) { return a.matches(b); }
};

DEFINE_ELEMENT_TYPE_CASTS(SVGElement, isSVGElement());

template <> inline bool isElementOfType<const SVGElement>(const Node& node) { return node.isSVGElement(); }

}

#endif
