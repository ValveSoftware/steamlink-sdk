/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef HTMLFormElement_h
#define HTMLFormElement_h

#include "core/CoreExport.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLFormControlElement.h"
#include "core/html/forms/RadioButtonGroupScope.h"
#include "core/loader/FormSubmission.h"

namespace blink {

class Event;
class FormAssociatedElement;
class HTMLFormControlElement;
class HTMLFormControlsCollection;
class HTMLImageElement;
class RadioNodeListOrElement;

class CORE_EXPORT HTMLFormElement final : public HTMLElement {
    DEFINE_WRAPPERTYPEINFO();
public:
    static HTMLFormElement* create(Document&);
    ~HTMLFormElement() override;
    DECLARE_VIRTUAL_TRACE();

    HTMLFormControlsCollection* elements();
    void getNamedElements(const AtomicString&, HeapVector<Member<Element>>&);

    unsigned length() const;
    HTMLElement* item(unsigned index);

    String enctype() const { return m_attributes.encodingType(); }
    void setEnctype(const AtomicString&);

    String encoding() const { return m_attributes.encodingType(); }
    void setEncoding(const AtomicString& value) { setEnctype(value); }

    bool shouldAutocomplete() const;

    void associate(FormAssociatedElement&);
    void disassociate(FormAssociatedElement&);
    void associate(HTMLImageElement&);
    void disassociate(HTMLImageElement&);
    void didAssociateByParser();

    void prepareForSubmission(Event*);
    void submitFromJavaScript();
    void reset();

    void setDemoted(bool);

    void submitImplicitly(Event*, bool fromImplicitSubmissionTrigger);

    String name() const;

    bool noValidate() const;

    const AtomicString& action() const;

    String method() const;
    void setMethod(const AtomicString&);

    // Find the 'default button.'
    // https://html.spec.whatwg.org/multipage/forms.html#default-button
    HTMLFormControlElement* findDefaultButton() const;

    bool checkValidity();
    bool reportValidity();
    bool matchesValidityPseudoClasses() const final;
    bool isValidElement() final;

    RadioButtonGroupScope& radioButtonGroupScope() { return m_radioButtonGroupScope; }

    const FormAssociatedElement::List& associatedElements() const;
    const HeapVector<Member<HTMLImageElement>>& imageElements();

    void anonymousNamedGetter(const AtomicString& name, RadioNodeListOrElement&);
    void invalidateDefaultButtonStyle() const;

private:
    explicit HTMLFormElement(Document&);

    bool layoutObjectIsNeeded(const ComputedStyle&) override;
    InsertionNotificationRequest insertedInto(ContainerNode*) override;
    void removedFrom(ContainerNode*) override;
    void finishParsingChildren() override;

    void handleLocalEvents(Event&) override;

    void parseAttribute(const QualifiedName&, const AtomicString&, const AtomicString&) override;
    bool isURLAttribute(const Attribute&) const override;
    bool hasLegalLinkAttribute(const QualifiedName&) const override;

    bool shouldRegisterAsNamedItem() const override { return true; }

    void copyNonAttributePropertiesFromElement(const Element&) override;

    void submitDialog(FormSubmission*);
    void submit(Event*, bool activateSubmitButton);

    void scheduleFormSubmission(FormSubmission*);

    void collectAssociatedElements(Node& root, FormAssociatedElement::List&) const;
    void collectImageElements(Node& root, HeapVector<Member<HTMLImageElement>>&);

    // Returns true if the submission should proceed.
    bool validateInteractively();

    // Validates each of the controls, and stores controls of which 'invalid'
    // event was not canceled to the specified vector. Returns true if there
    // are any invalid controls in this form.
    bool checkInvalidControlsAndCollectUnhandled(HeapVector<Member<HTMLFormControlElement>>*, CheckValidityEventBehavior);

    Element* elementFromPastNamesMap(const AtomicString&);
    void addToPastNamesMap(Element*, const AtomicString& pastName);
    void removeFromPastNamesMap(HTMLElement&);

    typedef HeapHashMap<AtomicString, Member<Element>> PastNamesMap;

    FormSubmission::Attributes m_attributes;
    Member<PastNamesMap> m_pastNamesMap;

    RadioButtonGroupScope m_radioButtonGroupScope;

    // Do not access m_associatedElements directly. Use associatedElements() instead.
    FormAssociatedElement::List m_associatedElements;
    // Do not access m_imageElements directly. Use imageElements() instead.
    HeapVector<Member<HTMLImageElement>> m_imageElements;

    bool m_associatedElementsAreDirty : 1;
    bool m_imageElementsAreDirty : 1;
    bool m_hasElementsAssociatedByParser : 1;
    bool m_hasElementsAssociatedByFormAttribute : 1;
    bool m_didFinishParsingChildren : 1;

    bool m_isSubmittingOrInUserJSSubmitEvent : 1;
    bool m_shouldSubmit : 1;

    bool m_isInResetFunction : 1;

    bool m_wasDemoted : 1;
};

} // namespace blink

#endif // HTMLFormElement_h
