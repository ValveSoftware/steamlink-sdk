/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2009, 2010, 2011 Google Inc. All rights reserved.
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

#ifndef HTMLTextFormControlElement_h
#define HTMLTextFormControlElement_h

#include "core/CoreExport.h"
#include "core/editing/VisiblePosition.h"
#include "core/html/HTMLFormControlElementWithState.h"

namespace blink {

class ExceptionState;
class Range;

enum TextFieldSelectionDirection { SelectionHasNoDirection, SelectionHasForwardDirection, SelectionHasBackwardDirection };
enum TextFieldEventBehavior { DispatchNoEvent, DispatchChangeEvent, DispatchInputAndChangeEvent };
enum NeedToDispatchSelectEvent { DispatchSelectEvent, NotDispatchSelectEvent };

class CORE_EXPORT HTMLTextFormControlElement : public HTMLFormControlElementWithState {
public:
    // Common flag for HTMLInputElement::tooLong(), HTMLTextAreaElement::tooLong(),
    // HTMLInputElement::tooShort() and HTMLTextAreaElement::tooShort().
    enum NeedsToCheckDirtyFlag {CheckDirtyFlag, IgnoreDirtyFlag};
    // Option of setSelectionRange.
    enum SelectionOption {
        ChangeSelection,
        ChangeSelectionAndFocus,
        ChangeSelectionIfFocused,
        NotChangeSelection
    };

    ~HTMLTextFormControlElement() override;

    void forwardEvent(Event*);


    InsertionNotificationRequest insertedInto(ContainerNode*) override;

    // The derived class should return true if placeholder processing is needed.
    virtual bool isPlaceholderVisible() const = 0;
    virtual void setPlaceholderVisibility(bool) = 0;
    virtual bool supportsPlaceholder() const = 0;
    String strippedPlaceholder() const;
    HTMLElement* placeholderElement() const;
    void updatePlaceholderVisibility();

    VisiblePosition visiblePositionForIndex(int) const;
    int indexForVisiblePosition(const VisiblePosition&) const;
    int selectionStart() const;
    int selectionEnd() const;
    const AtomicString& selectionDirection() const;
    void setSelectionStart(int);
    void setSelectionEnd(int);
    void setSelectionDirection(const String&);
    void select(NeedToDispatchSelectEvent = DispatchSelectEvent);
    virtual void setRangeText(const String& replacement, ExceptionState&);
    virtual void setRangeText(const String& replacement, unsigned start, unsigned end, const String& selectionMode, ExceptionState&);
    void setSelectionRange(int start, int end, const String& direction);
    void setSelectionRange(int start, int end, TextFieldSelectionDirection = SelectionHasNoDirection, NeedToDispatchSelectEvent = DispatchSelectEvent, SelectionOption = ChangeSelection);
    Range* selection() const;

    virtual bool supportsAutocapitalize() const = 0;
    virtual const AtomicString& defaultAutocapitalize() const = 0;
    const AtomicString& autocapitalize() const;
    void setAutocapitalize(const AtomicString&);

    void dispatchFormControlChangeEvent() final;
    void enqueueChangeEvent();

    virtual String value() const = 0;
    virtual void setValue(const String&, TextFieldEventBehavior = DispatchNoEvent) = 0;

    HTMLElement* innerEditorElement() const;

    void selectionChanged(bool userTriggered);
    bool lastChangeWasUserEdit() const;
    virtual void setInnerEditorValue(const String&);
    String innerEditorValue() const;
    Node* createPlaceholderBreakElement() const;

    String directionForFormData() const;

    void setTextAsOfLastFormControlChangeEvent(const String& text) { m_textAsOfLastFormControlChangeEvent = text; }

    // These functions don't cause synchronous layout and SpellChecker uses
    // them to improve performance.
    // Passed |Position| must point inside of a text form control.
    static Position startOfWord(const Position&);
    static Position endOfWord(const Position&);
    static Position startOfSentence(const Position&);
    static Position endOfSentence(const Position&);

protected:
    HTMLTextFormControlElement(const QualifiedName&, Document&, HTMLFormElement*);
    bool isPlaceholderEmpty() const;
    virtual void updatePlaceholderText() = 0;

    void parseAttribute(const QualifiedName&, const AtomicString&, const AtomicString&) override;

    void cacheSelection(int start, int end, TextFieldSelectionDirection direction)
    {
        ASSERT(start >= 0);
        m_cachedSelectionStart = start;
        m_cachedSelectionEnd = end;
        m_cachedSelectionDirection = direction;
    }

    void restoreCachedSelection();

    void defaultEventHandler(Event*) override;
    virtual void subtreeHasChanged() = 0;

    void setLastChangeWasNotUserEdit() { m_lastChangeWasUserEdit = false; }
    void addPlaceholderBreakElementIfNecessary();
    String valueWithHardLineBreaks() const;

    virtual bool shouldDispatchFormControlChangeEvent(String&, String&);
    void copyNonAttributePropertiesFromElement(const Element&) override;

private:
    int computeSelectionStart() const;
    int computeSelectionEnd() const;
    TextFieldSelectionDirection computeSelectionDirection() const;

    void dispatchFocusEvent(Element* oldFocusedElement, WebFocusType, InputDeviceCapabilities* sourceCapabilities) final;
    void dispatchBlurEvent(Element* newFocusedElement, WebFocusType, InputDeviceCapabilities* sourceCapabilities) final;
    void scheduleSelectEvent();

    // Returns true if user-editable value is empty. Used to check placeholder visibility.
    virtual bool isEmptyValue() const = 0;
    // Returns true if suggested value is empty. Used to check placeholder visibility.
    virtual bool isEmptySuggestedValue() const { return true; }
    // Called in dispatchFocusEvent(), after placeholder process, before calling parent's dispatchFocusEvent().
    virtual void handleFocusEvent(Element* /* oldFocusedNode */, WebFocusType) { }
    // Called in dispatchBlurEvent(), after placeholder process, before calling parent's dispatchBlurEvent().
    virtual void handleBlurEvent() { }

    bool placeholderShouldBeVisible() const;

    String m_textAsOfLastFormControlChangeEvent;
    bool m_lastChangeWasUserEdit;

    int m_cachedSelectionStart;
    int m_cachedSelectionEnd;
    TextFieldSelectionDirection m_cachedSelectionDirection;
};

inline bool isHTMLTextFormControlElement(const Element& element)
{
    return element.isTextFormControl();
}

DEFINE_HTMLELEMENT_TYPE_CASTS_WITH_FUNCTION(HTMLTextFormControlElement);

HTMLTextFormControlElement* enclosingTextFormControl(const Position&);
HTMLTextFormControlElement* enclosingTextFormControl(Node*);

} // namespace blink

#endif
