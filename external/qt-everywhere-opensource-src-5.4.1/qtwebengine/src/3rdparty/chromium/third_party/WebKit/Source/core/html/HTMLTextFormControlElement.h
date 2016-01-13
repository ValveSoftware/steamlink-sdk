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

#include "core/html/HTMLFormControlElementWithState.h"

namespace WebCore {

class ExceptionState;
class Position;
class Range;
class RenderTextControl;
class VisiblePosition;

enum TextFieldSelectionDirection { SelectionHasNoDirection, SelectionHasForwardDirection, SelectionHasBackwardDirection };
enum TextFieldEventBehavior { DispatchNoEvent, DispatchChangeEvent, DispatchInputAndChangeEvent };

class HTMLTextFormControlElement : public HTMLFormControlElementWithState {
public:
    // Common flag for HTMLInputElement::tooLong() and HTMLTextAreaElement::tooLong().
    enum NeedsToCheckDirtyFlag {CheckDirtyFlag, IgnoreDirtyFlag};

    virtual ~HTMLTextFormControlElement();

    void forwardEvent(Event*);


    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;

    // The derived class should return true if placeholder processing is needed.
    virtual bool supportsPlaceholder() const = 0;
    String strippedPlaceholder() const;
    bool placeholderShouldBeVisible() const;
    HTMLElement* placeholderElement() const;
    void updatePlaceholderVisibility(bool);

    VisiblePosition visiblePositionForIndex(int) const;
    int indexForVisiblePosition(const VisiblePosition&) const;
    int selectionStart() const;
    int selectionEnd() const;
    const AtomicString& selectionDirection() const;
    void setSelectionStart(int);
    void setSelectionEnd(int);
    void setSelectionDirection(const String&);
    void select();
    virtual void setRangeText(const String& replacement, ExceptionState&);
    virtual void setRangeText(const String& replacement, unsigned start, unsigned end, const String& selectionMode, ExceptionState&);
    void setSelectionRange(int start, int end, const String& direction);
    void setSelectionRange(int start, int end, TextFieldSelectionDirection = SelectionHasNoDirection);
    PassRefPtrWillBeRawPtr<Range> selection() const;

    virtual void dispatchFormControlChangeEvent() OVERRIDE FINAL;

    virtual String value() const = 0;

    HTMLElement* innerEditorElement() const;

    void selectionChanged(bool userTriggered);
    bool lastChangeWasUserEdit() const;
    virtual void setInnerEditorValue(const String&);
    String innerEditorValue() const;

    String directionForFormData() const;

    void setTextAsOfLastFormControlChangeEvent(const String& text) { m_textAsOfLastFormControlChangeEvent = text; }

protected:
    HTMLTextFormControlElement(const QualifiedName&, Document&, HTMLFormElement*);
    bool isPlaceholderEmpty() const;
    virtual void updatePlaceholderText() = 0;

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;

    void cacheSelection(int start, int end, TextFieldSelectionDirection direction)
    {
        ASSERT(start >= 0);
        m_cachedSelectionStart = start;
        m_cachedSelectionEnd = end;
        m_cachedSelectionDirection = direction;
    }

    void restoreCachedSelection();

    virtual void defaultEventHandler(Event*) OVERRIDE;
    virtual void subtreeHasChanged() = 0;

    void setLastChangeWasNotUserEdit() { m_lastChangeWasUserEdit = false; }

    String valueWithHardLineBreaks() const;

    virtual bool shouldDispatchFormControlChangeEvent(String&, String&);

private:
    int computeSelectionStart() const;
    int computeSelectionEnd() const;
    TextFieldSelectionDirection computeSelectionDirection() const;

    virtual void dispatchFocusEvent(Element* oldFocusedElement, FocusType) OVERRIDE FINAL;
    virtual void dispatchBlurEvent(Element* newFocusedElement) OVERRIDE FINAL;

    // Returns true if user-editable value is empty. Used to check placeholder visibility.
    virtual bool isEmptyValue() const = 0;
    // Returns true if suggested value is empty. Used to check placeholder visibility.
    virtual bool isEmptySuggestedValue() const { return true; }
    // Called in dispatchFocusEvent(), after placeholder process, before calling parent's dispatchFocusEvent().
    virtual void handleFocusEvent(Element* /* oldFocusedNode */, FocusType) { }
    // Called in dispatchBlurEvent(), after placeholder process, before calling parent's dispatchBlurEvent().
    virtual void handleBlurEvent() { }

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

} // namespace

#endif
