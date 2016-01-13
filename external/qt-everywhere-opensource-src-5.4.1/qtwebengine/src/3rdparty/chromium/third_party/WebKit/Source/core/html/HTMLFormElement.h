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

#include "core/html/HTMLElement.h"
#include "core/html/HTMLFormControlElement.h"
#include "core/html/forms/RadioButtonGroupScope.h"
#include "core/loader/FormSubmission.h"
#include "wtf/OwnPtr.h"
#include "wtf/WeakPtr.h"

namespace WTF{
class TextEncoding;
}

namespace WebCore {

class Event;
class FormAssociatedElement;
class FormData;
class GenericEventQueue;
class HTMLFormControlElement;
class HTMLFormControlsCollection;
class HTMLImageElement;
class HTMLInputElement;

class HTMLFormElement FINAL : public HTMLElement {
public:
    static PassRefPtrWillBeRawPtr<HTMLFormElement> create(Document&);
    virtual ~HTMLFormElement();
    virtual void trace(Visitor*) OVERRIDE;

    PassRefPtrWillBeRawPtr<HTMLFormControlsCollection> elements();
    void getNamedElements(const AtomicString&, WillBeHeapVector<RefPtrWillBeMember<Element> >&);

    unsigned length() const;
    Element* item(unsigned index);

    String enctype() const { return m_attributes.encodingType(); }
    void setEnctype(const AtomicString&);

    String encoding() const { return m_attributes.encodingType(); }
    void setEncoding(const AtomicString& value) { setEnctype(value); }

    bool shouldAutocomplete() const;

    void associate(FormAssociatedElement&);
    void disassociate(FormAssociatedElement&);
    void associate(HTMLImageElement&);
    void disassociate(HTMLImageElement&);
#if !ENABLE(OILPAN)
    WeakPtr<HTMLFormElement> createWeakPtr();
#endif
    void didAssociateByParser();

    void prepareForSubmission(Event*);
    void submit();
    void submitFromJavaScript();
    void reset();

    void setDemoted(bool);

    void submitImplicitly(Event*, bool fromImplicitSubmissionTrigger);

    String name() const;

    bool noValidate() const;

    const AtomicString& action() const;

    String method() const;
    void setMethod(const AtomicString&);

    bool wasUserSubmitted() const;

    HTMLFormControlElement* defaultButton() const;

    bool checkValidity();

    enum AutocompleteResult {
        AutocompleteResultSuccess,
        AutocompleteResultErrorDisabled,
        AutocompleteResultErrorCancel,
        AutocompleteResultErrorInvalid,
    };

    void requestAutocomplete();
    void finishRequestAutocomplete(AutocompleteResult);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(autocomplete);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(autocompleteerror);

    RadioButtonGroupScope& radioButtonGroupScope() { return m_radioButtonGroupScope; }

    const FormAssociatedElement::List& associatedElements() const;
    const WillBeHeapVector<RawPtrWillBeMember<HTMLImageElement> >& imageElements();

    void anonymousNamedGetter(const AtomicString& name, bool&, RefPtrWillBeRawPtr<RadioNodeList>&, bool&, RefPtrWillBeRawPtr<Element>&);

private:
    explicit HTMLFormElement(Document&);

    virtual bool rendererIsNeeded(const RenderStyle&) OVERRIDE;
    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;
    virtual void finishParsingChildren() OVERRIDE;

    virtual void handleLocalEvents(Event*) OVERRIDE;

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual bool isURLAttribute(const Attribute&) const OVERRIDE;
    virtual bool hasLegalLinkAttribute(const QualifiedName&) const OVERRIDE;

    virtual bool shouldRegisterAsNamedItem() const OVERRIDE { return true; }

    virtual void copyNonAttributePropertiesFromElement(const Element&) OVERRIDE;

    void submitDialog(PassRefPtrWillBeRawPtr<FormSubmission>);
    void submit(Event*, bool activateSubmitButton, bool processingUserGesture, FormSubmissionTrigger);

    void scheduleFormSubmission(PassRefPtrWillBeRawPtr<FormSubmission>);

    void collectAssociatedElements(Node& root, FormAssociatedElement::List&) const;
    void collectImageElements(Node& root, WillBeHeapVector<RawPtrWillBeMember<HTMLImageElement> >&);

    // Returns true if the submission should proceed.
    bool validateInteractively(Event*);

    // Validates each of the controls, and stores controls of which 'invalid'
    // event was not canceled to the specified vector. Returns true if there
    // are any invalid controls in this form.
    bool checkInvalidControlsAndCollectUnhandled(WillBeHeapVector<RefPtrWillBeMember<FormAssociatedElement> >*);

    Element* elementFromPastNamesMap(const AtomicString&);
    void addToPastNamesMap(Element*, const AtomicString& pastName);
    void removeFromPastNamesMap(HTMLElement&);

    typedef WillBeHeapHashMap<AtomicString, RawPtrWillBeMember<Element> > PastNamesMap;

    FormSubmission::Attributes m_attributes;
    OwnPtrWillBeMember<PastNamesMap> m_pastNamesMap;

    RadioButtonGroupScope m_radioButtonGroupScope;

    // Do not access m_associatedElements directly. Use associatedElements() instead.
    FormAssociatedElement::List m_associatedElements;
    // Do not access m_imageElements directly. Use imageElements() instead.
    WillBeHeapVector<RawPtrWillBeMember<HTMLImageElement> > m_imageElements;
#if !ENABLE(OILPAN)
    WeakPtrFactory<HTMLFormElement> m_weakPtrFactory;
#endif
    bool m_associatedElementsAreDirty;
    bool m_imageElementsAreDirty;
    bool m_hasElementsAssociatedByParser;
    bool m_didFinishParsingChildren;

    bool m_wasUserSubmitted;
    bool m_isSubmittingOrInUserJSSubmitEvent;
    bool m_shouldSubmit;

    bool m_isInResetFunction;

    bool m_wasDemoted;

    OwnPtrWillBeMember<GenericEventQueue> m_pendingAutocompleteEventsQueue;
};

} // namespace WebCore

#endif // HTMLFormElement_h
