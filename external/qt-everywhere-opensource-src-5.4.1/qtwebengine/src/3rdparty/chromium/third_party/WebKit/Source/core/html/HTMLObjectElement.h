/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
 *
 */

#ifndef HTMLObjectElement_h
#define HTMLObjectElement_h

#include "core/html/FormAssociatedElement.h"
#include "core/html/HTMLPlugInElement.h"

namespace WebCore {

class HTMLFormElement;

class HTMLObjectElement FINAL : public HTMLPlugInElement, public FormAssociatedElement {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(HTMLObjectElement);

public:
    static PassRefPtrWillBeRawPtr<HTMLObjectElement> create(Document&, HTMLFormElement*, bool createdByParser);
    virtual ~HTMLObjectElement();
    virtual void trace(Visitor*) OVERRIDE;

    const String& classId() const { return m_classId; }

    virtual HTMLFormElement* formOwner() const OVERRIDE;

    bool containsJavaApplet() const;

    virtual bool hasFallbackContent() const OVERRIDE;
    virtual bool useFallbackContent() const OVERRIDE;
    virtual void renderFallbackContent() OVERRIDE;

    virtual bool isFormControlElement() const OVERRIDE { return false; }

    virtual bool isEnumeratable() const OVERRIDE { return true; }
    virtual bool isInteractiveContent() const OVERRIDE;
    virtual bool appendFormData(FormDataList&, bool) OVERRIDE;

    virtual bool isObjectElement() const OVERRIDE { return true; }

    // Implementations of constraint validation API.
    // Note that the object elements are always barred from constraint validation.
    virtual String validationMessage() const OVERRIDE { return String(); }
    bool checkValidity() { return true; }
    virtual void setCustomValidity(const String&) OVERRIDE { }

#if !ENABLE(OILPAN)
    using Node::ref;
    using Node::deref;
#endif

    virtual bool canContainRangeEndPoint() const OVERRIDE { return useFallbackContent(); }

    bool isExposed() const;

private:
    HTMLObjectElement(Document&, HTMLFormElement*, bool createdByParser);

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual bool isPresentationAttribute(const QualifiedName&) const OVERRIDE;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) OVERRIDE;

    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;

    virtual bool rendererIsNeeded(const RenderStyle&) OVERRIDE;
    virtual void didMoveToNewDocument(Document& oldDocument) OVERRIDE;

    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0) OVERRIDE;

    virtual bool isURLAttribute(const Attribute&) const OVERRIDE;
    virtual bool hasLegalLinkAttribute(const QualifiedName&) const OVERRIDE;
    virtual const QualifiedName& subResourceAttributeName() const OVERRIDE;
    virtual const AtomicString imageSourceURL() const OVERRIDE;

    virtual RenderWidget* existingRenderWidget() const OVERRIDE;

    virtual void updateWidgetInternal() OVERRIDE;
    void updateDocNamedItem();

    void reattachFallbackContent();

    // FIXME: This function should not deal with url or serviceType
    // so that we can better share code between <object> and <embed>.
    void parametersForPlugin(Vector<String>& paramNames, Vector<String>& paramValues, String& url, String& serviceType);

    bool shouldAllowQuickTimeClassIdQuirk();
    bool hasValidClassId();

    void reloadPluginOnAttributeChange(const QualifiedName&);

#if !ENABLE(OILPAN)
    virtual void refFormAssociatedElement() OVERRIDE { ref(); }
    virtual void derefFormAssociatedElement() OVERRIDE { deref(); }
#endif

    virtual bool shouldRegisterAsNamedItem() const OVERRIDE { return true; }
    virtual bool shouldRegisterAsExtraNamedItem() const OVERRIDE { return true; }

    String m_classId;
    bool m_useFallbackContent : 1;
};

// Intentionally left unimplemented, template specialization needs to be provided for specific
// return types.
template<typename T> inline const T& toElement(const FormAssociatedElement&);
template<typename T> inline const T* toElement(const FormAssociatedElement*);

// Make toHTMLObjectElement() accept a FormAssociatedElement as input instead of a Node.
template<> inline const HTMLObjectElement* toElement<HTMLObjectElement>(const FormAssociatedElement* element)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!element || !element->isFormControlElement());
    const HTMLObjectElement* objectElement = static_cast<const HTMLObjectElement*>(element);
    // We need to assert after the cast because FormAssociatedElement doesn't
    // have hasTagName.
    ASSERT_WITH_SECURITY_IMPLICATION(!objectElement || objectElement->hasTagName(HTMLNames::objectTag));
    return objectElement;
}

template<> inline const HTMLObjectElement& toElement<HTMLObjectElement>(const FormAssociatedElement& element)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!element.isFormControlElement());
    const HTMLObjectElement& objectElement = static_cast<const HTMLObjectElement&>(element);
    // We need to assert after the cast because FormAssociatedElement doesn't
    // have hasTagName.
    ASSERT_WITH_SECURITY_IMPLICATION(objectElement.hasTagName(HTMLNames::objectTag));
    return objectElement;
}

}

#endif
