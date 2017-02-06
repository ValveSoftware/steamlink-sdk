/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef HTMLSelectElement_h
#define HTMLSelectElement_h

#include "base/gtest_prod_util.h"
#include "core/CoreExport.h"
#include "core/html/HTMLContentElement.h"
#include "core/html/HTMLFormControlElementWithState.h"
#include "core/html/HTMLOptionsCollection.h"
#include "core/html/forms/TypeAhead.h"
#include "wtf/Vector.h"

namespace blink {

class AutoscrollController;
class ExceptionState;
class HTMLHRElement;
class HTMLOptGroupElement;
class HTMLOptionElement;
class HTMLOptionElementOrHTMLOptGroupElement;
class HTMLElementOrLong;
class PopupMenu;

class CORE_EXPORT HTMLSelectElement final : public HTMLFormControlElementWithState, private TypeAheadDataSource {
    DEFINE_WRAPPERTYPEINFO();
public:
    static HTMLSelectElement* create(Document&);
    static HTMLSelectElement* create(Document&, HTMLFormElement*);
    ~HTMLSelectElement() override;

    int selectedIndex() const;
    void setSelectedIndex(int);
    int suggestedIndex() const;
    void setSuggestedIndex(int);

    void optionSelectedByUser(int index, bool dispatchChangeEvent, bool allowMultipleSelection = false);

    // For ValidityState
    String validationMessage() const override;
    bool valueMissing() const override;

    String defaultToolTip() const override;
    void resetImpl() override;

    unsigned length() const;
    void setLength(unsigned, ExceptionState&);

    unsigned size() const { return m_size; }
    void setSize(unsigned);
    bool multiple() const { return m_multiple; }

    bool usesMenuList() const;

    void add(const HTMLOptionElementOrHTMLOptGroupElement&, const HTMLElementOrLong&, ExceptionState&);

    using Node::remove;
    void remove(int index);

    String value() const;
    void setValue(const String&, bool sendEvents = false);
    String suggestedValue() const;
    void setSuggestedValue(const String&);

    HTMLOptionsCollection* options();
    HTMLCollection* selectedOptions();

    void optionElementChildrenChanged(const HTMLOptionElement&);

    void invalidateSelectedItems();

    using ListItems = HeapVector<Member<HTMLElement>>;
    const ListItems& listItems() const;

    void accessKeyAction(bool sendMouseEvents) override;
    void accessKeySetSelectedIndex(int);

    void setOption(unsigned index, HTMLOptionElement*, ExceptionState&);

    Element* namedItem(const AtomicString& name);
    HTMLOptionElement* item(unsigned index);

    void scrollToSelection();
    void scrollToOption(HTMLOptionElement*);

    void listBoxSelectItem(int listIndex, bool allowMultiplySelections, bool shift, bool fireOnChangeNow = true);

    bool canSelectAll() const;
    void selectAll();
    int listToOptionIndex(int listIndex) const;
    void listBoxOnChange();
    int optionToListIndex(int optionIndex) const;
    int activeSelectionEndListIndex() const;
    HTMLOptionElement* activeSelectionEnd() const;
    void setActiveSelectionAnchor(HTMLOptionElement*);
    void setActiveSelectionEnd(HTMLOptionElement*);

    // For use in the implementation of HTMLOptionElement.
    void optionSelectionStateChanged(HTMLOptionElement*, bool optionIsSelected);
    void optionInserted(HTMLOptionElement&, bool optionIsSelected);
    void optionRemoved(HTMLOptionElement&);
    bool anonymousIndexedSetter(unsigned, HTMLOptionElement*, ExceptionState&);

    void optGroupInsertedOrRemoved(HTMLOptGroupElement&);
    void hrInsertedOrRemoved(HTMLHRElement&);

    void updateListOnLayoutObject();

    HTMLOptionElement* spatialNavigationFocusedOption();
    void handleMouseRelease();

    int listIndexForOption(const HTMLOptionElement&);

    // Helper functions for popup menu implementations.
    String itemText(const Element&) const;
    bool itemIsDisplayNone(Element&) const;
    // itemComputedStyle() returns nullptr only if the owner Document is not
    // active.  So, It returns a valid object when we open a popup.
    const ComputedStyle* itemComputedStyle(Element&) const;
    IntRect elementRectRelativeToViewport() const;
    // Text starting offset in LTR.
    LayoutUnit clientPaddingLeft() const;
    // Text starting offset in RTL.
    LayoutUnit clientPaddingRight() const;
    void valueChanged(unsigned listIndex);
    // A popup is canceled when the popup was hidden without selecting an item.
    void popupDidCancel();
    // Provisional selection is a selection made using arrow keys or type ahead.
    void provisionalSelectionChanged(unsigned);
    void popupDidHide();
    bool popupIsVisible() const { return m_popupIsVisible; }
    int optionIndexToBeShown() const;
    void showPopup();
    void hidePopup();
    PopupMenu* popup() const { return m_popup.get(); }
    void didMutateSubtree();

    void resetTypeAheadSessionForTesting();

    DECLARE_VIRTUAL_TRACE();

protected:
    HTMLSelectElement(Document&, HTMLFormElement*);

private:
    const AtomicString& formControlType() const override;

    bool shouldShowFocusRingOnMouseFocus() const override;

    void dispatchFocusEvent(Element* oldFocusedElement, WebFocusType, InputDeviceCapabilities* sourceCapabilities) override;
    void dispatchBlurEvent(Element* newFocusedElement, WebFocusType, InputDeviceCapabilities* sourceCapabilities) override;

    bool canStartSelection() const override { return false; }

    bool isEnumeratable() const override { return true; }
    bool isInteractiveContent() const override;
    bool supportsAutofocus() const override;
    bool supportLabels() const override { return true; }

    FormControlState saveFormControlState() const override;
    void restoreFormControlState(const FormControlState&) override;

    void parseAttribute(const QualifiedName&, const AtomicString&, const AtomicString&) override;
    bool isPresentationAttribute(const QualifiedName&) const override;

    LayoutObject* createLayoutObject(const ComputedStyle&) override;
    void didRecalcStyle(StyleRecalcChange) override;
    void detach(const AttachContext& = AttachContext()) override;
    void appendToFormData(FormData&) override;
    void didAddUserAgentShadowRoot(ShadowRoot&) override;

    void defaultEventHandler(Event*) override;

    void dispatchInputAndChangeEventForMenuList();

    // |subject| is an element which was inserted or removed.
    void setRecalcListItems(HTMLElement& subject);
    void recalcListItems() const;
    enum ResetReason {
        ResetReasonSelectedOptionRemoved,
        ResetReasonOthers
    };
    void resetToDefaultSelection(ResetReason = ResetReasonOthers);
    void typeAheadFind(KeyboardEvent*);
    void saveLastSelection();
    void saveListboxActiveSelection();
    // Returns the first selected OPTION, or nullptr.
    HTMLOptionElement* selectedOption() const;

    bool isOptionalFormControl() const override { return !isRequiredFormControl(); }
    bool isRequiredFormControl() const override;

    bool hasPlaceholderLabelOption() const;

    enum SelectOptionFlag {
        DeselectOtherOptions = 1 << 0,
        DispatchInputAndChangeEvent = 1 << 1,
        MakeOptionDirty = 1 << 2,
    };
    typedef unsigned SelectOptionFlags;
    void selectOption(int optionIndex, SelectOptionFlags);
    void selectOption(HTMLOptionElement*, SelectOptionFlags);
    void selectOption(HTMLOptionElement*, int optionIndex, SelectOptionFlags);
    void deselectItemsWithoutValidation(HTMLElement* elementToExclude = 0);
    void parseMultipleAttribute(const AtomicString&);
    HTMLOptionElement* lastSelectedOption() const;
    void updateSelectedState(HTMLOptionElement*, bool multi, bool shift);
    void menuListDefaultEventHandler(Event*);
    void handlePopupOpenKeyboardEvent(Event*);
    bool shouldOpenPopupForKeyDownEvent(KeyboardEvent*);
    bool shouldOpenPopupForKeyPressEvent(KeyboardEvent*);
    void listBoxDefaultEventHandler(Event*);
    void setOptionsChangedOnLayoutObject();
    size_t searchOptionsForValue(const String&, size_t listIndexStart, size_t listIndexEnd) const;
    void updateListBoxSelection(bool deselectOtherOptions, bool scroll = true);
    void setIndexToSelectOnCancel(int listIndex);

    enum SkipDirection {
        SkipBackwards = -1,
        SkipForwards = 1
    };
    HTMLOptionElement* nextValidOption(int listIndex, SkipDirection, int skip) const;
    HTMLOptionElement* nextSelectableOption(HTMLOptionElement*) const;
    HTMLOptionElement* previousSelectableOption(HTMLOptionElement*) const;
    HTMLOptionElement* firstSelectableOption() const;
    HTMLOptionElement* lastSelectableOption() const;
    HTMLOptionElement* nextSelectableOptionPageAway(HTMLOptionElement*, SkipDirection) const;
    HTMLOptionElement* eventTargetOption(const Event&);
    AutoscrollController* autoscrollController() const;
    void scrollToOptionTask();

    bool areAuthorShadowsAllowed() const override { return false; }
    void finishParsingChildren() override;

    // TypeAheadDataSource functions.
    int indexOfSelectedOption() const override;
    int optionCount() const override;
    String optionAtIndex(int index) const override;

    void observeTreeMutation();
    void unobserveTreeMutation();

    // m_listItems contains HTMLOptionElement, HTMLOptGroupElement, and
    // HTMLHRElement objects.
    mutable ListItems m_listItems;
    Vector<bool> m_lastOnChangeSelection;
    Vector<bool> m_cachedStateForActiveSelection;
    TypeAhead m_typeAhead;
    unsigned m_size;
    Member<HTMLOptionElement> m_lastOnChangeOption;
    Member<HTMLOptionElement> m_activeSelectionAnchor;
    Member<HTMLOptionElement> m_activeSelectionEnd;
    Member<HTMLOptionElement> m_optionToScrollTo;
    bool m_multiple;
    bool m_activeSelectionState;
    mutable bool m_shouldRecalcListItems;
    int m_suggestedIndex;
    bool m_isAutofilledByPreview;

    class PopupUpdater;
    Member<PopupUpdater> m_popupUpdater;
    Member<PopupMenu> m_popup;
    int m_indexToSelectOnCancel;
    bool m_popupIsVisible;

    FRIEND_TEST_ALL_PREFIXES(HTMLSelectElementTest, FirstSelectableOption);
    FRIEND_TEST_ALL_PREFIXES(HTMLSelectElementTest, LastSelectableOption);
    FRIEND_TEST_ALL_PREFIXES(HTMLSelectElementTest, NextSelectableOption);
    FRIEND_TEST_ALL_PREFIXES(HTMLSelectElementTest, PreviousSelectableOption);
};

} // namespace blink

#endif // HTMLSelectElement_h
