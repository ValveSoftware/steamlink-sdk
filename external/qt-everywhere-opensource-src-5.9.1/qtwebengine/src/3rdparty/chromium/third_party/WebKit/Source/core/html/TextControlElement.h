/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
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

#ifndef TextControlElement_h
#define TextControlElement_h

#include "base/gtest_prod_util.h"
#include "core/CoreExport.h"
#include "core/editing/VisiblePosition.h"
#include "core/html/HTMLFormControlElementWithState.h"

namespace blink {

class ExceptionState;
class Range;

enum TextFieldSelectionDirection {
  SelectionHasNoDirection,
  SelectionHasForwardDirection,
  SelectionHasBackwardDirection
};
enum TextFieldEventBehavior {
  DispatchNoEvent,
  DispatchChangeEvent,
  DispatchInputAndChangeEvent
};

class CORE_EXPORT TextControlElement : public HTMLFormControlElementWithState {
 public:
  // Common flag for HTMLInputElement::tooLong(),
  // HTMLTextAreaElement::tooLong(),
  // HTMLInputElement::tooShort() and HTMLTextAreaElement::tooShort().
  enum NeedsToCheckDirtyFlag { CheckDirtyFlag, IgnoreDirtyFlag };

  ~TextControlElement() override;

  void forwardEvent(Event*);

  void setFocused(bool flag) override;
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
  void select();
  virtual void setRangeText(const String& replacement, ExceptionState&);
  virtual void setRangeText(const String& replacement,
                            unsigned start,
                            unsigned end,
                            const String& selectionMode,
                            ExceptionState&);
  // Web-exposed setSelectionRange() function. This schedule to dispatch
  // 'select' event.
  void setSelectionRangeForBinding(int start,
                                   int end,
                                   const String& direction = "none");
  // Blink-internal version of setSelectionRange(). This translates "none"
  // direction to "forward" on platforms without "none" direction.
  // This returns true if it updated cached selection and/or FrameSelection.
  bool setSelectionRange(int start,
                         int end,
                         TextFieldSelectionDirection = SelectionHasNoDirection);
  Range* selection() const;

  virtual bool supportsAutocapitalize() const = 0;
  virtual const AtomicString& defaultAutocapitalize() const = 0;
  const AtomicString& autocapitalize() const;
  void setAutocapitalize(const AtomicString&);

  int maxLength() const;
  int minLength() const;
  void setMaxLength(int, ExceptionState&);
  void setMinLength(int, ExceptionState&);

  // Dispatch 'input' event, and update
  // m_wasChangedSinceLastFormControlChangeEvent flag.
  void dispatchFormControlInputEvent();
  // Dispatch 'change' event if the value is updated.
  void dispatchFormControlChangeEvent();
  // Enqueue 'change' event if the value is updated.
  void enqueueChangeEvent();
  void setTextAsOfLastFormControlChangeEvent(const String& text) {
    m_textAsOfLastFormControlChangeEvent = text;
  }
  // A user has changed the value since the last 'change' event.
  bool wasChangedSinceLastFormControlChangeEvent() const {
    return m_wasChangedSinceLastFormControlChangeEvent;
  }
  void setChangedSinceLastFormControlChangeEvent(bool);

  virtual String value() const = 0;
  virtual void setValue(const String&,
                        TextFieldEventBehavior = DispatchNoEvent) = 0;

  HTMLElement* innerEditorElement() const;

  void selectionChanged(bool userTriggered);
  bool lastChangeWasUserEdit() const;
  virtual void setInnerEditorValue(const String&);
  String innerEditorValue() const;
  Node* createPlaceholderBreakElement() const;

  String directionForFormData() const;

  // These functions don't cause synchronous layout and SpellChecker uses
  // them to improve performance.
  // Passed |Position| must point inside of a text form control.
  static Position startOfWord(const Position&);
  static Position endOfWord(const Position&);
  static Position startOfSentence(const Position&);
  static Position endOfSentence(const Position&);

 protected:
  TextControlElement(const QualifiedName&, Document&, HTMLFormElement*);
  bool isPlaceholderEmpty() const;
  virtual void updatePlaceholderText() = 0;

  void parseAttribute(const QualifiedName&,
                      const AtomicString&,
                      const AtomicString&) override;

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
  void cacheSelection(int start,
                      int end,
                      TextFieldSelectionDirection direction) {
    DCHECK_GE(start, 0);
    DCHECK_LE(start, end);
    m_cachedSelectionStart = start;
    m_cachedSelectionEnd = end;
    m_cachedSelectionDirection = direction;
  }
  static int indexForPosition(HTMLElement* innerEditor, const Position&);

  void dispatchFocusEvent(Element* oldFocusedElement,
                          WebFocusType,
                          InputDeviceCapabilities* sourceCapabilities) final;
  void dispatchBlurEvent(Element* newFocusedElement,
                         WebFocusType,
                         InputDeviceCapabilities* sourceCapabilities) final;
  void scheduleSelectEvent();

  // Returns true if user-editable value is empty. Used to check placeholder
  // visibility.
  virtual bool isEmptyValue() const = 0;
  // Returns true if suggested value is empty. Used to check placeholder
  // visibility.
  virtual bool isEmptySuggestedValue() const { return true; }
  // Called in dispatchFocusEvent(), after placeholder process, before calling
  // parent's dispatchFocusEvent().
  virtual void handleFocusEvent(Element* /* oldFocusedNode */, WebFocusType) {}
  // Called in dispatchBlurEvent(), after placeholder process, before calling
  // parent's dispatchBlurEvent().
  virtual void handleBlurEvent() {}

  bool placeholderShouldBeVisible() const;

  String m_textAsOfLastFormControlChangeEvent;
  bool m_wasChangedSinceLastFormControlChangeEvent = false;
  bool m_lastChangeWasUserEdit;

  int m_cachedSelectionStart;
  int m_cachedSelectionEnd;
  TextFieldSelectionDirection m_cachedSelectionDirection;

  FRIEND_TEST_ALL_PREFIXES(TextControlElementTest, IndexForPosition);
};

inline bool isTextControlElement(const Element& element) {
  return element.isTextControl();
}

DEFINE_HTMLELEMENT_TYPE_CASTS_WITH_FUNCTION(TextControlElement);

TextControlElement* enclosingTextControl(const Position&);
TextControlElement* enclosingTextControl(Node*);

}  // namespace blink

#endif
