/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010, 2011 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#include "core/html/HTMLSelectElement.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "bindings/core/v8/HTMLElementOrLong.h"
#include "bindings/core/v8/HTMLOptionElementOrHTMLOptGroupElement.h"
#include "core/HTMLNames.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/Attribute.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/dom/MutationCallback.h"
#include "core/dom/MutationObserver.h"
#include "core/dom/MutationObserverInit.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/NodeListsNodeData.h"
#include "core/dom/NodeTraversal.h"
#include "core/events/GestureEvent.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/MouseEvent.h"
#include "core/events/ScopedEventQueue.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/FormData.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLHRElement.h"
#include "core/html/HTMLOptGroupElement.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/forms/FormController.h"
#include "core/input/EventHandler.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/layout/HitTestRequest.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutListBox.h"
#include "core/layout/LayoutMenuList.h"
#include "core/layout/LayoutTheme.h"
#include "core/page/AutoscrollController.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/page/SpatialNavigation.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/PopupMenu.h"
#include "platform/TraceEvent.h"
#include "platform/text/PlatformLocale.h"

using namespace WTF::Unicode;

namespace blink {

using namespace HTMLNames;

// Upper limit of m_listItems. According to the HTML standard, options larger
// than this limit doesn't work well because |selectedIndex| IDL attribute is
// signed.
static const unsigned maxListItems = INT_MAX;

HTMLSelectElement::HTMLSelectElement(Document& document, HTMLFormElement* form)
    : HTMLFormControlElementWithState(selectTag, document, form)
    , m_typeAhead(this)
    , m_size(0)
    , m_lastOnChangeOption(nullptr)
    , m_multiple(false)
    , m_activeSelectionState(false)
    , m_shouldRecalcListItems(false)
    , m_suggestedIndex(-1)
    , m_isAutofilledByPreview(false)
    , m_indexToSelectOnCancel(-1)
    , m_popupIsVisible(false)
{
    setHasCustomStyleCallbacks();
}

HTMLSelectElement* HTMLSelectElement::create(Document& document)
{
    HTMLSelectElement* select = new HTMLSelectElement(document, 0);
    select->ensureUserAgentShadowRoot();
    return select;
}

HTMLSelectElement* HTMLSelectElement::create(Document& document, HTMLFormElement* form)
{
    HTMLSelectElement* select = new HTMLSelectElement(document, form);
    select->ensureUserAgentShadowRoot();
    return select;
}

HTMLSelectElement::~HTMLSelectElement()
{
}

const AtomicString& HTMLSelectElement::formControlType() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, selectMultiple, ("select-multiple"));
    DEFINE_STATIC_LOCAL(const AtomicString, selectOne, ("select-one"));
    return m_multiple ? selectMultiple : selectOne;
}

void HTMLSelectElement::optionSelectedByUser(int optionIndex, bool fireOnChangeNow, bool allowMultipleSelection)
{
    // User interaction such as mousedown events can cause list box select
    // elements to send change events.  This produces that same behavior for
    // changes triggered by other code running on behalf of the user.
    if (!usesMenuList()) {
        updateSelectedState(item(optionIndex), allowMultipleSelection, false);
        setNeedsValidityCheck();
        if (fireOnChangeNow)
            listBoxOnChange();
        return;
    }

    // Bail out if this index is already the selected one, to avoid running
    // unnecessary JavaScript that can mess up autofill when there is no actual
    // change (see https://bugs.webkit.org/show_bug.cgi?id=35256 and
    // <rdar://7467917>).  The selectOption function does not behave this way,
    // possibly because other callers need a change event even in cases where
    // the selected option is not change.
    if (optionIndex == selectedIndex())
        return;

    selectOption(optionIndex, DeselectOtherOptions | MakeOptionDirty | (fireOnChangeNow ? DispatchInputAndChangeEvent : 0));
}

bool HTMLSelectElement::hasPlaceholderLabelOption() const
{
    // The select element has no placeholder label option if it has an attribute
    // "multiple" specified or a display size of non-1.
    //
    // The condition "size() > 1" is not compliant with the HTML5 spec as of Dec
    // 3, 2010. "size() != 1" is correct.  Using "size() > 1" here because
    // size() may be 0 in WebKit.  See the discussion at
    // https://bugs.webkit.org/show_bug.cgi?id=43887
    //
    // "0 size()" happens when an attribute "size" is absent or an invalid size
    // attribute is specified.  In this case, the display size should be assumed
    // as the default.  The default display size is 1 for non-multiple select
    // elements, and 4 for multiple select elements.
    //
    // Finally, if size() == 0 and non-multiple, the display size can be assumed
    // as 1.
    if (multiple() || size() > 1)
        return false;

    int listIndex = optionToListIndex(0);
    ASSERT(listIndex >= 0);
    if (listIndex < 0)
        return false;
    return !listIndex && toHTMLOptionElement(listItems()[listIndex])->value().isEmpty();
}

String HTMLSelectElement::validationMessage() const
{
    if (!willValidate())
        return String();
    if (customError())
        return customValidationMessage();
    if (valueMissing())
        return locale().queryString(WebLocalizedString::ValidationValueMissingForSelect);
    return String();
}

bool HTMLSelectElement::valueMissing() const
{
    if (!willValidate())
        return false;

    if (!isRequired())
        return false;

    int firstSelectionIndex = selectedIndex();

    // If a non-placeholer label option is selected (firstSelectionIndex > 0),
    // it's not value-missing.
    return firstSelectionIndex < 0 || (!firstSelectionIndex && hasPlaceholderLabelOption());
}

String HTMLSelectElement::defaultToolTip() const
{
    if (form() && form()->noValidate())
        return String();
    return validationMessage();
}

void HTMLSelectElement::listBoxSelectItem(int listIndex, bool allowMultiplySelections, bool shift, bool fireOnChangeNow)
{
    if (!multiple()) {
        optionSelectedByUser(listToOptionIndex(listIndex), fireOnChangeNow, false);
    } else {
        HTMLElement* element = listItems()[listIndex];
        if (isHTMLOptionElement(element))
            updateSelectedState(toHTMLOptionElement(element), allowMultiplySelections, shift);
        setNeedsValidityCheck();
        if (fireOnChangeNow)
            listBoxOnChange();
    }
}

bool HTMLSelectElement::usesMenuList() const
{
    if (LayoutTheme::theme().delegatesMenuListRendering())
        return true;

    return !m_multiple && m_size <= 1;
}

int HTMLSelectElement::activeSelectionEndListIndex() const
{
    HTMLOptionElement* option = activeSelectionEnd();
    return option ? option->listIndex() : -1;
}

HTMLOptionElement* HTMLSelectElement::activeSelectionEnd() const
{
    if (m_activeSelectionEnd)
        return m_activeSelectionEnd.get();
    return lastSelectedOption();
}

void HTMLSelectElement::add(const HTMLOptionElementOrHTMLOptGroupElement& element, const HTMLElementOrLong& before, ExceptionState& exceptionState)
{
    HTMLElement* elementToInsert;
    ASSERT(!element.isNull());
    if (element.isHTMLOptionElement())
        elementToInsert = element.getAsHTMLOptionElement();
    else
        elementToInsert = element.getAsHTMLOptGroupElement();

    HTMLElement* beforeElement;
    if (before.isHTMLElement())
        beforeElement = before.getAsHTMLElement();
    else if (before.isLong())
        beforeElement = options()->item(before.getAsLong());
    else
        beforeElement = nullptr;

    insertBefore(elementToInsert, beforeElement, exceptionState);
    setNeedsValidityCheck();
}

void HTMLSelectElement::remove(int optionIndex)
{
    int listIndex = optionToListIndex(optionIndex);
    if (listIndex >= 0)
        listItems()[listIndex]->remove(IGNORE_EXCEPTION);
}

String HTMLSelectElement::value() const
{
    if (HTMLOptionElement* option = selectedOption())
        return option->value();
    return "";
}

void HTMLSelectElement::setValue(const String &value, bool sendEvents)
{
    // We clear the previously selected option(s) when needed, to guarantee
    // calling setSelectedIndex() only once.
    int optionIndex = 0;
    if (value.isNull()) {
        optionIndex = -1;
    } else {
        // Find the option with value() matching the given parameter and make it
        // the current selection.
        for (auto& item : listItems()) {
            if (!isHTMLOptionElement(item))
                continue;
            if (toHTMLOptionElement(item)->value() == value)
                break;
            optionIndex++;
        }
        if (optionIndex >= static_cast<int>(listItems().size()))
            optionIndex = -1;
    }

    int previousSelectedIndex = selectedIndex();
    setSuggestedIndex(-1);
    if (m_isAutofilledByPreview)
        setAutofilled(false);
    SelectOptionFlags flags = DeselectOtherOptions | MakeOptionDirty;
    if (sendEvents)
        flags |= DispatchInputAndChangeEvent;
    selectOption(optionIndex, flags);

    if (sendEvents && previousSelectedIndex != selectedIndex() && !usesMenuList())
        listBoxOnChange();
}

String HTMLSelectElement::suggestedValue() const
{
    const ListItems& items = listItems();
    for (unsigned i = 0; i < items.size(); ++i) {
        if (isHTMLOptionElement(items[i]) && m_suggestedIndex >= 0) {
            if (i == static_cast<unsigned>(m_suggestedIndex))
                return toHTMLOptionElement(items[i])->value();
        }
    }
    return "";
}

void HTMLSelectElement::setSuggestedValue(const String& value)
{
    if (value.isNull()) {
        setSuggestedIndex(-1);
        return;
    }

    unsigned optionIndex = 0;
    for (auto& item : listItems()) {
        if (!isHTMLOptionElement(item))
            continue;
        if (toHTMLOptionElement(item)->value() == value) {
            setSuggestedIndex(optionIndex);
            m_isAutofilledByPreview = true;
            return;
        }
        optionIndex++;
    }

    setSuggestedIndex(-1);
}

bool HTMLSelectElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == alignAttr) {
        // Don't map 'align' attribute. This matches what Firefox, Opera and IE do.
        // See http://bugs.webkit.org/show_bug.cgi?id=12072
        return false;
    }

    return HTMLFormControlElementWithState::isPresentationAttribute(name);
}

void HTMLSelectElement::parseAttribute(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& value)
{
    if (name == sizeAttr) {
        unsigned oldSize = m_size;
        // Set the attribute value to a number.
        // This is important since the style rules for this attribute can
        // determine the appearance property.
        unsigned size = value.getString().toUInt();
        AtomicString attrSize = AtomicString::number(size);
        if (attrSize != value) {
            // FIXME: This is horribly factored.
            if (Attribute* sizeAttribute = ensureUniqueElementData().attributes().find(sizeAttr))
                sizeAttribute->setValue(attrSize);
        }
        m_size = std::max(size, 0u);
        setNeedsValidityCheck();
        if (m_size != oldSize) {
            if (inActiveDocument())
                lazyReattachIfAttached();
            resetToDefaultSelection();
            if (!usesMenuList())
                saveListboxActiveSelection();
        }
    } else if (name == multipleAttr) {
        parseMultipleAttribute(value);
    } else if (name == accesskeyAttr) {
        // FIXME: ignore for the moment.
        //
    } else if (name == disabledAttr) {
        HTMLFormControlElementWithState::parseAttribute(name, oldValue, value);
        if (popupIsVisible())
            hidePopup();

    } else {
        HTMLFormControlElementWithState::parseAttribute(name, oldValue, value);
    }
}

bool HTMLSelectElement::shouldShowFocusRingOnMouseFocus() const
{
    return true;
}

bool HTMLSelectElement::canSelectAll() const
{
    return !usesMenuList();
}

LayoutObject* HTMLSelectElement::createLayoutObject(const ComputedStyle&)
{
    if (usesMenuList())
        return new LayoutMenuList(this);
    return new LayoutListBox(this);
}

HTMLCollection* HTMLSelectElement::selectedOptions()
{
    return ensureCachedCollection<HTMLCollection>(SelectedOptions);
}

HTMLOptionsCollection* HTMLSelectElement::options()
{
    return ensureCachedCollection<HTMLOptionsCollection>(SelectOptions);
}

void HTMLSelectElement::optionElementChildrenChanged(const HTMLOptionElement& option)
{
    setNeedsValidityCheck();

    if (layoutObject()) {
        if (option.selected() && usesMenuList())
            layoutObject()->updateFromElement();
        if (AXObjectCache* cache = layoutObject()->document().existingAXObjectCache())
            cache->childrenChanged(this);
    }
}

void HTMLSelectElement::accessKeyAction(bool sendMouseEvents)
{
    focus();
    dispatchSimulatedClick(nullptr, sendMouseEvents ? SendMouseUpDownEvents : SendNoEvents);
}

void HTMLSelectElement::setSize(unsigned size)
{
    setUnsignedIntegralAttribute(sizeAttr, size);
}

Element* HTMLSelectElement::namedItem(const AtomicString& name)
{
    return options()->namedItem(name);
}

HTMLOptionElement* HTMLSelectElement::item(unsigned index)
{
    return options()->item(index);
}

void HTMLSelectElement::setOption(unsigned index, HTMLOptionElement* option, ExceptionState& exceptionState)
{
    int diff = index - length();
    // We should check |index >= maxListItems| first to avoid integer overflow.
    if (index >= maxListItems || listItems().size() + diff + 1 > maxListItems) {
        document().addConsoleMessage(ConsoleMessage::create(JSMessageSource, WarningMessageLevel,
            String::format("Blocked to expand the option list and set an option at index=%u.  The maximum list length is %u.", index, maxListItems)));
        return;
    }
    HTMLOptionElementOrHTMLOptGroupElement element;
    element.setHTMLOptionElement(option);
    HTMLElementOrLong before;
    // Out of array bounds? First insert empty dummies.
    if (diff > 0) {
        setLength(index, exceptionState);
        // Replace an existing entry?
    } else if (diff < 0) {
        before.setHTMLElement(options()->item(index + 1));
        remove(index);
    }
    // Finally add the new element.
    if (!exceptionState.hadException()) {
        add(element, before, exceptionState);
        if (diff >= 0 && option->selected())
            optionSelectionStateChanged(option, true);
    }
}

void HTMLSelectElement::setLength(unsigned newLen, ExceptionState& exceptionState)
{
    // We should check |newLen > maxListItems| first to avoid integer overflow.
    if (newLen > maxListItems || listItems().size() + newLen - length() > maxListItems) {
        document().addConsoleMessage(ConsoleMessage::create(JSMessageSource, WarningMessageLevel,
            String::format("Blocked to expand the option list to %u items.  The maximum list length is %u.", newLen, maxListItems)));
        return;
    }
    int diff = length() - newLen;

    if (diff < 0) { // Add dummy elements.
        do {
            appendChild(document().createElement(optionTag, CreatedByCreateElement), exceptionState);
            if (exceptionState.hadException())
                break;
        } while (++diff);
    } else {
        // Removing children fires mutation events, which might mutate the DOM
        // further, so we first copy out a list of elements that we intend to
        // remove then attempt to remove them one at a time.
        HeapVector<Member<Element>> itemsToRemove;
        size_t optionIndex = 0;
        for (auto& item : listItems()) {
            if (isHTMLOptionElement(item) && optionIndex++ >= newLen) {
                ASSERT(item->parentNode());
                itemsToRemove.append(item.get());
            }
        }

        for (auto& item : itemsToRemove) {
            if (item->parentNode())
                item->parentNode()->removeChild(item.get(), exceptionState);
        }
    }
    setNeedsValidityCheck();
}

bool HTMLSelectElement::isRequiredFormControl() const
{
    return isRequired();
}

// Returns the 1st valid OPTION |skip| items from |listIndex| in direction
// |direction| if there is one.
// Otherwise, it returns the valid OPTION closest to that boundary which is past
// |listIndex| if there is one.
// Otherwise, it returns nullptr.
// Valid means that it is enabled and visible.
HTMLOptionElement* HTMLSelectElement::nextValidOption(int listIndex, SkipDirection direction, int skip) const
{
    ASSERT(direction == SkipBackwards || direction == SkipForwards);
    const ListItems& listItems = this->listItems();
    HTMLOptionElement* lastGoodOption = nullptr;
    int size = listItems.size();
    for (listIndex += direction; listIndex >= 0 && listIndex < size; listIndex += direction) {
        --skip;
        HTMLElement* element = listItems[listIndex];
        if (!isHTMLOptionElement(*element))
            continue;
        if (toHTMLOptionElement(*element).isDisplayNone())
            continue;
        if (element->isDisabledFormControl())
            continue;
        if (!usesMenuList() && !element->layoutObject())
            continue;
        lastGoodOption = toHTMLOptionElement(element);
        if (skip <= 0)
            break;
    }
    return lastGoodOption;
}

HTMLOptionElement* HTMLSelectElement::nextSelectableOption(HTMLOptionElement* startOption) const
{
    return nextValidOption(startOption ? startOption->listIndex() : -1, SkipForwards, 1);
}

HTMLOptionElement* HTMLSelectElement::previousSelectableOption(HTMLOptionElement* startOption) const
{
    return nextValidOption(startOption ? startOption->listIndex() : listItems().size(), SkipBackwards, 1);
}

HTMLOptionElement* HTMLSelectElement::firstSelectableOption() const
{
    // TODO(tkent): This is not efficient.  nextSlectableOption(nullptr) is
    // faster.
    return nextValidOption(listItems().size(), SkipBackwards, INT_MAX);
}

HTMLOptionElement* HTMLSelectElement::lastSelectableOption() const
{
    // TODO(tkent): This is not efficient.  previousSlectableOption(nullptr) is
    // faster.
    return nextValidOption(-1, SkipForwards, INT_MAX);
}

// Returns the index of the next valid item one page away from |startIndex| in
// direction |direction|.
HTMLOptionElement* HTMLSelectElement::nextSelectableOptionPageAway(HTMLOptionElement* startOption, SkipDirection direction) const
{
    const ListItems& items = listItems();
    // Can't use m_size because layoutObject forces a minimum size.
    int pageSize = 0;
    if (layoutObject()->isListBox())
        pageSize = toLayoutListBox(layoutObject())->size() - 1; // -1 so we still show context.

    // One page away, but not outside valid bounds.
    // If there is a valid option item one page away, the index is chosen.
    // If there is no exact one page away valid option, returns startIndex or
    // the most far index.
    int startIndex = startOption ? startOption->listIndex() : -1;
    int edgeIndex = (direction == SkipForwards) ? 0 : (items.size() - 1);
    int skipAmount = pageSize + ((direction == SkipForwards) ? startIndex : (edgeIndex - startIndex));
    return nextValidOption(edgeIndex, direction, skipAmount);
}

void HTMLSelectElement::selectAll()
{
    ASSERT(!usesMenuList());
    if (!layoutObject() || !m_multiple)
        return;

    // Save the selection so it can be compared to the new selectAll selection
    // when dispatching change events.
    saveLastSelection();

    m_activeSelectionState = true;
    setActiveSelectionAnchor(nextSelectableOption(nullptr));
    setActiveSelectionEnd(previousSelectableOption(nullptr));

    updateListBoxSelection(false, false);
    listBoxOnChange();
    setNeedsValidityCheck();
}

void HTMLSelectElement::saveLastSelection()
{
    if (usesMenuList()) {
        m_lastOnChangeOption = selectedOption();
        return;
    }

    m_lastOnChangeSelection.clear();
    for (auto& element : listItems())
        m_lastOnChangeSelection.append(isHTMLOptionElement(*element) && toHTMLOptionElement(element)->selected());
}

void HTMLSelectElement::setActiveSelectionAnchor(HTMLOptionElement* option)
{
    m_activeSelectionAnchor = option;
    if (!usesMenuList())
        saveListboxActiveSelection();
}

void HTMLSelectElement::saveListboxActiveSelection()
{
    // Cache the selection state so we can restore the old selection as the new
    // selection pivots around this anchor index.
    // Example:
    // 1. Press the mouse button on the second OPTION
    //   m_activeSelectionAnchorIndex = 1
    // 2. Drag the mouse pointer onto the fifth OPTION
    //   m_activeSelectionEndIndex = 4, options at 1-4 indices are selected.
    // 3. Drag the mouse pointer onto the fourth OPTION
    //   m_activeSelectionEndIndex = 3, options at 1-3 indices are selected.
    //   updateListBoxSelection needs to clear selection of the fifth OPTION.
    m_cachedStateForActiveSelection.resize(0);
    for (auto& element : listItems()) {
        m_cachedStateForActiveSelection.append(isHTMLOptionElement(*element) && toHTMLOptionElement(element)->selected());
    }
}

void HTMLSelectElement::setActiveSelectionEnd(HTMLOptionElement* option)
{
    m_activeSelectionEnd = option;
}

void HTMLSelectElement::updateListBoxSelection(bool deselectOtherOptions, bool scroll)
{
    ASSERT(layoutObject());
    ASSERT(layoutObject()->isListBox() || m_multiple);

    int activeSelectionAnchorIndex = m_activeSelectionAnchor ? m_activeSelectionAnchor->listIndex() : -1;
    int activeSelectionEndIndex = m_activeSelectionEnd ? m_activeSelectionEnd->listIndex() : -1;
    int start = std::min(activeSelectionAnchorIndex, activeSelectionEndIndex);
    int end = std::max(activeSelectionAnchorIndex, activeSelectionEndIndex);

    const ListItems& items = listItems();
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        if (!isHTMLOptionElement(*items[i]))
            continue;
        HTMLOptionElement& option = toHTMLOptionElement(*items[i]);
        if (option.isDisabledFormControl() || !option.layoutObject())
            continue;
        if (i >= start && i <= end) {
            option.setSelectedState(m_activeSelectionState);
            option.setDirty(true);
        } else if (deselectOtherOptions || i >= static_cast<int>(m_cachedStateForActiveSelection.size())) {
            option.setSelectedState(false);
            option.setDirty(true);
        } else {
            option.setSelectedState(m_cachedStateForActiveSelection[i]);
        }
    }

    setNeedsValidityCheck();
    if (scroll)
        scrollToSelection();
    notifyFormStateChanged();
}

void HTMLSelectElement::listBoxOnChange()
{
    ASSERT(!usesMenuList() || m_multiple);

    const ListItems& items = listItems();

    // If the cached selection list is empty, or the size has changed, then fire
    // dispatchFormControlChangeEvent, and return early.
    // FIXME: Why? This looks unreasonable.
    if (m_lastOnChangeSelection.isEmpty() || m_lastOnChangeSelection.size() != items.size()) {
        dispatchFormControlChangeEvent();
        return;
    }

    // Update m_lastOnChangeSelection and fire dispatchFormControlChangeEvent.
    bool fireOnChange = false;
    for (unsigned i = 0; i < items.size(); ++i) {
        HTMLElement* element = items[i];
        bool selected = isHTMLOptionElement(*element) && toHTMLOptionElement(element)->selected();
        if (selected != m_lastOnChangeSelection[i])
            fireOnChange = true;
        m_lastOnChangeSelection[i] = selected;
    }

    if (fireOnChange) {
        dispatchInputEvent();
        dispatchFormControlChangeEvent();
    }
}

void HTMLSelectElement::dispatchInputAndChangeEventForMenuList()
{
    ASSERT(usesMenuList());

    HTMLOptionElement* selectedOption = this->selectedOption();
    if (m_lastOnChangeOption.get() != selectedOption) {
        m_lastOnChangeOption = selectedOption;
        dispatchInputEvent();
        dispatchFormControlChangeEvent();
    }
}

void HTMLSelectElement::scrollToSelection()
{
    if (!isFinishedParsingChildren())
        return;
    if (usesMenuList())
        return;
    scrollToOption(activeSelectionEnd());
    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->listboxActiveIndexChanged(this);
}

void HTMLSelectElement::setOptionsChangedOnLayoutObject()
{
    if (LayoutObject* layoutObject = this->layoutObject()) {
        if (usesMenuList())
            toLayoutMenuList(layoutObject)->setNeedsLayoutAndPrefWidthsRecalc(LayoutInvalidationReason::MenuOptionsChanged);
    }
}

const HTMLSelectElement::ListItems& HTMLSelectElement::listItems() const
{
    if (m_shouldRecalcListItems) {
        recalcListItems();
    } else {
#if ENABLE(ASSERT)
        HeapVector<Member<HTMLElement>> items = m_listItems;
        recalcListItems();
        ASSERT(items == m_listItems);
#endif
    }

    return m_listItems;
}

void HTMLSelectElement::invalidateSelectedItems()
{
    if (HTMLCollection* collection = cachedCollection<HTMLCollection>(SelectedOptions))
        collection->invalidateCache();
}

void HTMLSelectElement::setRecalcListItems(HTMLElement& subject)
{
    // FIXME: This function does a bunch of confusing things depending on if it
    // is in the document or not.

    bool shouldRecalc = true;
    if (!m_shouldRecalcListItems && !isHTMLOptGroupElement(subject)) {
        if (firstChild() == &subject) {
            // The subject was prepended. This doesn't handle elements in an
            // OPTGROUP.
            DCHECK(m_listItems.size() == 0 || m_listItems[0] != &subject);
            m_listItems.prepend(&subject);
            shouldRecalc = false;
        } else if (lastChild() == &subject) {
            // The subject was appended. This doesn't handle elements in an
            // OPTGROUP.
            DCHECK(m_listItems.size() == 0 || m_listItems.last() != &subject);
            m_listItems.append(&subject);
            shouldRecalc = false;
        } else if (!subject.isDescendantOf(this)) {
            // |subject| was removed from this. This logic works well with
            // SELECT children and OPTGROUP children.

            // m_listItems might be empty, or might not have the OPTION.
            //   1. Remove an OPTGROUP with OPTION children from a SELECT.
            //   2. This function is called for the OPTGROUP removal.
            //   3. m_shouldRecalcListItems becomes true.
            //   4. recalcListItems() happens.  m_listItems has no OPTGROUP and
            //      no its children.  m_shouldRecalcListItems becomes false.
            //   5. This function is called for the removal of an OPTION child
            //      of the OPTGROUP.
            if (m_listItems.size() > 0) {
                size_t index;
                // Avoid Vector::find() in typical cases.
                if (m_listItems.first() == &subject)
                    index = 0;
                else if (m_listItems.last() == &subject)
                    index = m_listItems.size() - 1;
                else
                    index = m_listItems.find(&subject);
                if (index != WTF::kNotFound) {
                    m_listItems.remove(index);
                    shouldRecalc = false;
                }
            }
        }
    }
    m_shouldRecalcListItems = shouldRecalc;

    setOptionsChangedOnLayoutObject();
    if (!inShadowIncludingDocument()) {
        if (HTMLOptionsCollection* collection = cachedCollection<HTMLOptionsCollection>(SelectOptions))
            collection->invalidateCache();
        invalidateSelectedItems();
    }

    if (layoutObject()) {
        if (AXObjectCache* cache = layoutObject()->document().existingAXObjectCache())
            cache->childrenChanged(this);
    }
}

void HTMLSelectElement::recalcListItems() const
{
    TRACE_EVENT0("blink", "HTMLSelectElement::recalcListItems");
    m_listItems.resize(0);

    m_shouldRecalcListItems = false;

    for (Element* currentElement = ElementTraversal::firstWithin(*this); currentElement && m_listItems.size() < maxListItems; ) {
        if (!currentElement->isHTMLElement()) {
            currentElement = ElementTraversal::nextSkippingChildren(*currentElement, this);
            continue;
        }
        HTMLElement& current = toHTMLElement(*currentElement);

        // We should ignore nested optgroup elements. The HTML parser flatten
        // them.  However we need to ignore nested optgroups built by DOM APIs.
        // This behavior matches to IE and Firefox.
        if (isHTMLOptGroupElement(current)) {
            if (current.parentNode() != this) {
                currentElement = ElementTraversal::nextSkippingChildren(current, this);
                continue;
            }
            m_listItems.append(&current);
            if (Element* nextElement = ElementTraversal::firstWithin(current)) {
                currentElement = nextElement;
                continue;
            }
        }

        if (isHTMLOptionElement(current))
            m_listItems.append(&current);

        if (isHTMLHRElement(current))
            m_listItems.append(&current);

        // In conforming HTML code, only <optgroup> and <option> will be found
        // within a <select>. We call NodeTraversal::nextSkippingChildren so
        // that we only step into those tags that we choose to. For web-compat,
        // we should cope with the case where odd tags like a <div> have been
        // added but we handle this because such tags have already been removed
        // from the <select>'s subtree at this point.
        currentElement = ElementTraversal::nextSkippingChildren(*currentElement, this);
    }
}

void HTMLSelectElement::resetToDefaultSelection(ResetReason reason)
{
    // https://html.spec.whatwg.org/multipage/forms.html#ask-for-a-reset
    if (multiple())
        return;
    HTMLOptionElement* firstEnabledOption = nullptr;
    int firstEnabledOptionIndex = -1;
    HTMLOptionElement* lastSelectedOption = nullptr;
    bool didChange = false;
    int optionIndex = 0;
    // We can't use HTMLSelectElement::options here because this function is
    // called in Node::insertedInto and Node::removedFrom before invalidating
    // node collections.
    for (auto& item : listItems()) {
        if (!isHTMLOptionElement(item))
            continue;
        HTMLOptionElement* option = toHTMLOptionElement(item);
        if (option->selected()) {
            if (lastSelectedOption) {
                lastSelectedOption->setSelectedState(false);
                didChange = true;
            }
            lastSelectedOption = option;
        }
        if (!firstEnabledOption && !option->isDisabledFormControl()) {
            firstEnabledOption = option;
            firstEnabledOptionIndex = optionIndex;
            if (reason == ResetReasonSelectedOptionRemoved) {
                // There must be no selected OPTIONs.
                break;
            }
        }
        ++optionIndex;
    }
    if (!lastSelectedOption && m_size <= 1 && firstEnabledOption && !firstEnabledOption->selected()) {
        selectOption(firstEnabledOption, firstEnabledOptionIndex, reason == ResetReasonSelectedOptionRemoved ? 0 : DeselectOtherOptions);
        lastSelectedOption = firstEnabledOption;
        didChange = true;
    }
    if (didChange)
        setNeedsValidityCheck();
    m_lastOnChangeOption = lastSelectedOption;
}

HTMLOptionElement* HTMLSelectElement::selectedOption() const
{
    for (const auto& element : listItems()) {
        if (isHTMLOptionElement(*element) && toHTMLOptionElement(*element).selected())
            return toHTMLOptionElement(element);
    }
    return nullptr;
}

int HTMLSelectElement::selectedIndex() const
{
    unsigned index = 0;

    // Return the number of the first option selected.
    for (auto& element : listItems()) {
        if (!isHTMLOptionElement(*element))
            continue;
        if (toHTMLOptionElement(*element).selected())
            return index;
        ++index;
    }

    return -1;
}

void HTMLSelectElement::setSelectedIndex(int index)
{
    selectOption(index, DeselectOtherOptions | MakeOptionDirty);
}

int HTMLSelectElement::suggestedIndex() const
{
    return m_suggestedIndex;
}

void HTMLSelectElement::setSuggestedIndex(int suggestedIndex)
{
    m_suggestedIndex = suggestedIndex;

    if (LayoutObject* layoutObject = this->layoutObject())  {
        layoutObject->updateFromElement();
        scrollToOption(item(listToOptionIndex(suggestedIndex)));
    }
    if (popupIsVisible())
        m_popup->updateFromElement(PopupMenu::BySelectionChange);
}

void HTMLSelectElement::scrollToOption(HTMLOptionElement* option)
{
    if (!option)
        return;
    if (usesMenuList())
        return;
    bool hasPendingTask = m_optionToScrollTo;
    // We'd like to keep an HTMLOptionElement reference rather than the index of
    // the option because the task should work even if unselected option is
    // inserted before executing scrollToOptionTask().
    m_optionToScrollTo = option;
    if (!hasPendingTask)
        document().postTask(BLINK_FROM_HERE, createSameThreadTask(&HTMLSelectElement::scrollToOptionTask, wrapPersistent(this)));
}

void HTMLSelectElement::scrollToOptionTask()
{
    HTMLOptionElement* option = m_optionToScrollTo.release();
    if (!option || !inShadowIncludingDocument())
        return;
    // optionRemoved() makes sure m_optionToScrollTo doesn't have an option with
    // another owner.
    ASSERT(option->ownerSelectElement() == this);
    document().updateStyleAndLayoutIgnorePendingStylesheets();
    if (!layoutObject() || !layoutObject()->isListBox())
        return;
    LayoutRect bounds = option->boundingBox();
    toLayoutListBox(layoutObject())->scrollToRect(bounds);
}

void HTMLSelectElement::optionSelectionStateChanged(HTMLOptionElement* option, bool optionIsSelected)
{
    ASSERT(option->ownerSelectElement() == this);
    if (optionIsSelected)
        selectOption(option, multiple() ? 0 : DeselectOtherOptions);
    else if (!usesMenuList() || multiple())
        selectOption(nullptr, multiple() ? 0 : DeselectOtherOptions);
    else
        selectOption(nextSelectableOption(nullptr), DeselectOtherOptions);
}

void HTMLSelectElement::optionInserted(HTMLOptionElement& option, bool optionIsSelected)
{
    ASSERT(option.ownerSelectElement() == this);
    setRecalcListItems(option);
    if (optionIsSelected) {
        selectOption(&option, multiple() ? 0 : DeselectOtherOptions);
    } else {
        // No need to reset if we already have a selected option.
        if (!m_lastOnChangeOption)
            resetToDefaultSelection();
    }
    setNeedsValidityCheck();
    m_lastOnChangeSelection.clear();
}

void HTMLSelectElement::optionRemoved(HTMLOptionElement& option)
{
    setRecalcListItems(option);
    if (option.selected())
        resetToDefaultSelection(ResetReasonSelectedOptionRemoved);
    else if (!m_lastOnChangeOption)
        resetToDefaultSelection();
    if (m_lastOnChangeOption == &option)
        m_lastOnChangeOption.clear();
    if (m_optionToScrollTo == &option)
        m_optionToScrollTo.clear();
    if (m_activeSelectionAnchor == &option)
        m_activeSelectionAnchor.clear();
    if (m_activeSelectionEnd == &option)
        m_activeSelectionEnd.clear();
    if (option.selected())
        setAutofilled(false);
    setNeedsValidityCheck();
    m_lastOnChangeSelection.clear();
}

void HTMLSelectElement::optGroupInsertedOrRemoved(HTMLOptGroupElement& optgroup)
{
    setRecalcListItems(optgroup);
    setNeedsValidityCheck();
    m_lastOnChangeSelection.clear();
}

void HTMLSelectElement::hrInsertedOrRemoved(HTMLHRElement& hr)
{
    setRecalcListItems(hr);
    m_lastOnChangeSelection.clear();
}

void HTMLSelectElement::selectOption(int optionIndex, SelectOptionFlags flags)
{
    selectOption(optionIndex < 0 ? nullptr : item(optionIndex), flags);
}

void HTMLSelectElement::selectOption(HTMLOptionElement* option, SelectOptionFlags flags)
{
    selectOption(option, option ? option->index() : -1, flags);
}

// TODO(tkent): This function is not efficient.  It contains multiple O(N)
// operations. crbug.com/577989.
void HTMLSelectElement::selectOption(HTMLOptionElement* element, int optionIndex, SelectOptionFlags flags)
{
    TRACE_EVENT0("blink", "HTMLSelectElement::selectOption");
    ASSERT((!element && optionIndex < 0) || (element && optionIndex >= 0));

    // selectedIndex() is O(N).
    if (isAutofilled() && selectedIndex() != optionIndex)
        setAutofilled(false);

    if (element) {
        element->setSelectedState(true);
        if (flags & MakeOptionDirty)
            element->setDirty(true);
    }

    // deselectItemsWithoutValidation() is O(N).
    if (flags & DeselectOtherOptions)
        deselectItemsWithoutValidation(element);

    // We should update active selection after finishing OPTION state change
    // because setActiveSelectionAnchorIndex() stores OPTION's selection state.
    if (element) {
        // setActiveSelectionAnchor is O(N).
        if (!m_activeSelectionAnchor || !multiple() || flags & DeselectOtherOptions)
            setActiveSelectionAnchor(element);
        if (!m_activeSelectionEnd || !multiple() || flags & DeselectOtherOptions)
            setActiveSelectionEnd(element);
    }

    // For the menu list case, this is what makes the selected element appear.
    if (LayoutObject* layoutObject = this->layoutObject())
        layoutObject->updateFromElement();
    // PopupMenu::updateFromElement() posts an O(N) task.
    if (popupIsVisible())
        m_popup->updateFromElement(PopupMenu::BySelectionChange);

    scrollToSelection();
    setNeedsValidityCheck();

    if (usesMenuList()) {
        if (flags & DispatchInputAndChangeEvent)
            dispatchInputAndChangeEventForMenuList();
        else
            m_lastOnChangeOption = element;
        if (LayoutObject* layoutObject = this->layoutObject()) {
            // Need to check usesMenuList() again because
            // dispatchInputAndChangeEventForMenuList() might change the status.
            if (usesMenuList()) {
                // didSetSelectedIndex() is O(N) because of optionToListIndex.
                toLayoutMenuList(layoutObject)->didSetSelectedIndex(optionIndex);
            }
        }
    }

    notifyFormStateChanged();
}

int HTMLSelectElement::optionToListIndex(int optionIndex) const
{
    const ListItems& items = listItems();
    int listSize = static_cast<int>(items.size());
    if (optionIndex < 0 || optionIndex >= listSize)
        return -1;

    int optionIndex2 = -1;
    for (int listIndex = 0; listIndex < listSize; ++listIndex) {
        if (isHTMLOptionElement(*items[listIndex])) {
            ++optionIndex2;
            if (optionIndex2 == optionIndex)
                return listIndex;
        }
    }

    return -1;
}

int HTMLSelectElement::listToOptionIndex(int listIndex) const
{
    const ListItems& items = listItems();
    if (listIndex < 0 || listIndex >= static_cast<int>(items.size()) || !isHTMLOptionElement(*items[listIndex]))
        return -1;

    // Actual index of option not counting OPTGROUP entries that may be in list.
    int optionIndex = 0;
    for (int i = 0; i < listIndex; ++i) {
        if (isHTMLOptionElement(*items[i]))
            ++optionIndex;
    }

    return optionIndex;
}

void HTMLSelectElement::dispatchFocusEvent(Element* oldFocusedElement, WebFocusType type, InputDeviceCapabilities* sourceCapabilities)
{
    // Save the selection so it can be compared to the new selection when
    // dispatching change events during blur event dispatch.
    if (usesMenuList())
        saveLastSelection();
    HTMLFormControlElementWithState::dispatchFocusEvent(oldFocusedElement, type, sourceCapabilities);
}

void HTMLSelectElement::dispatchBlurEvent(Element* newFocusedElement, WebFocusType type, InputDeviceCapabilities* sourceCapabilities)
{
    m_typeAhead.resetSession();
    // We only need to fire change events here for menu lists, because we fire
    // change events for list boxes whenever the selection change is actually
    // made.  This matches other browsers' behavior.
    if (usesMenuList())
        dispatchInputAndChangeEventForMenuList();
    m_lastOnChangeSelection.clear();
    HTMLFormControlElementWithState::dispatchBlurEvent(newFocusedElement, type, sourceCapabilities);
}

void HTMLSelectElement::deselectItemsWithoutValidation(HTMLElement* excludeElement)
{
    for (auto& element : listItems()) {
        if (element != excludeElement && isHTMLOptionElement(*element))
            toHTMLOptionElement(element)->setSelectedState(false);
    }
}

FormControlState HTMLSelectElement::saveFormControlState() const
{
    const ListItems& items = listItems();
    size_t length = items.size();
    FormControlState state;
    for (unsigned i = 0; i < length; ++i) {
        if (!isHTMLOptionElement(*items[i]))
            continue;
        HTMLOptionElement* option = toHTMLOptionElement(items[i]);
        if (!option->selected())
            continue;
        state.append(option->value());
        state.append(String::number(i));
        if (!multiple())
            break;
    }
    return state;
}

size_t HTMLSelectElement::searchOptionsForValue(const String& value, size_t listIndexStart, size_t listIndexEnd) const
{
    const ListItems& items = listItems();
    size_t loopEndIndex = std::min(items.size(), listIndexEnd);
    for (size_t i = listIndexStart; i < loopEndIndex; ++i) {
        if (!isHTMLOptionElement(items[i]))
            continue;
        if (toHTMLOptionElement(items[i])->value() == value)
            return i;
    }
    return kNotFound;
}

void HTMLSelectElement::restoreFormControlState(const FormControlState& state)
{
    recalcListItems();

    const ListItems& items = listItems();
    size_t itemsSize = items.size();
    if (itemsSize == 0)
        return;

    for (auto& item : items) {
        if (isHTMLOptionElement(item))
            toHTMLOptionElement(item)->setSelectedState(false);
    }

    // The saved state should have at least one value and an index.
    ASSERT(state.valueSize() >= 2);
    if (!multiple()) {
        size_t index = state[1].toUInt();
        if (index < itemsSize && isHTMLOptionElement(items[index]) && toHTMLOptionElement(items[index])->value() == state[0]) {
            toHTMLOptionElement(items[index])->setSelectedState(true);
            toHTMLOptionElement(items[index])->setDirty(true);
        } else {
            size_t foundIndex = searchOptionsForValue(state[0], 0, itemsSize);
            if (foundIndex != kNotFound) {
                toHTMLOptionElement(items[foundIndex])->setSelectedState(true);
                toHTMLOptionElement(items[foundIndex])->setDirty(true);
            }
        }
    } else {
        size_t startIndex = 0;
        for (size_t i = 0; i < state.valueSize(); i+= 2) {
            const String& value = state[i];
            const size_t index = state[i + 1].toUInt();
            if (index < itemsSize && isHTMLOptionElement(items[index]) && toHTMLOptionElement(items[index])->value() == value) {
                toHTMLOptionElement(items[index])->setSelectedState(true);
                toHTMLOptionElement(items[index])->setDirty(true);
                startIndex = index + 1;
            } else {
                size_t foundIndex = searchOptionsForValue(value, startIndex, itemsSize);
                if (foundIndex == kNotFound)
                    foundIndex = searchOptionsForValue(value, 0, startIndex);
                if (foundIndex == kNotFound)
                    continue;
                toHTMLOptionElement(items[foundIndex])->setSelectedState(true);
                toHTMLOptionElement(items[foundIndex])->setDirty(true);
                startIndex = foundIndex + 1;
            }
        }
    }

    setNeedsValidityCheck();
}

void HTMLSelectElement::parseMultipleAttribute(const AtomicString& value)
{
    bool oldMultiple = m_multiple;
    int oldSelectedIndex = selectedIndex();
    m_multiple = !value.isNull();
    setNeedsValidityCheck();
    lazyReattachIfAttached();
    // Restore selectedIndex after changing the multiple flag to preserve
    // selection as single-line and multi-line has different defaults.
    if (oldMultiple != m_multiple) {
        // Preserving the first selection is compatible with Firefox and
        // WebKit. However Edge seems to "ask for a reset" simply.  As of 2016
        // March, the HTML specification says nothing about this.
        if (oldSelectedIndex >= 0)
            selectOption(oldSelectedIndex, DeselectOtherOptions);
        else
            resetToDefaultSelection();
    }
}

void HTMLSelectElement::appendToFormData(FormData& formData)
{
    const AtomicString& name = this->name();
    if (name.isEmpty())
        return;

    for (auto& element : listItems()) {
        if (isHTMLOptionElement(*element) && toHTMLOptionElement(*element).selected() && !toHTMLOptionElement(*element).isDisabledFormControl())
            formData.append(name, toHTMLOptionElement(*element).value());
    }
}

void HTMLSelectElement::resetImpl()
{
    for (auto& item : listItems()) {
        if (!isHTMLOptionElement(item))
            continue;
        HTMLOptionElement* option = toHTMLOptionElement(item);
        option->setSelectedState(option->fastHasAttribute(selectedAttr));
        option->setDirty(false);
    }
    resetToDefaultSelection();
    setNeedsValidityCheck();
}

void HTMLSelectElement::handlePopupOpenKeyboardEvent(Event* event)
{
    focus();
    // Calling focus() may cause us to lose our layoutObject. Return true so
    // that our caller doesn't process the event further, but don't set
    // the event as handled.
    if (!layoutObject() || !layoutObject()->isMenuList() || isDisabledFormControl())
        return;
    // Save the selection so it can be compared to the new selection when
    // dispatching change events during selectOption, which gets called from
    // valueChanged, which gets called after the user makes a selection from the
    // menu.
    saveLastSelection();
    showPopup();
    event->setDefaultHandled();
    return;
}

bool HTMLSelectElement::shouldOpenPopupForKeyDownEvent(KeyboardEvent* keyEvent)
{
    const String& key = keyEvent->key();
    LayoutTheme& layoutTheme = LayoutTheme::theme();

    if (isSpatialNavigationEnabled(document().frame()))
        return false;

    return ((layoutTheme.popsMenuByArrowKeys() && (key == "ArrowDown" || key == "ArrowUp"))
        || (layoutTheme.popsMenuByAltDownUpOrF4Key() && (key == "ArrowDown" || key == "ArrowUp") && keyEvent->altKey())
        || (layoutTheme.popsMenuByAltDownUpOrF4Key() && (!keyEvent->altKey() && !keyEvent->ctrlKey() && key == "F4")));
}

bool HTMLSelectElement::shouldOpenPopupForKeyPressEvent(KeyboardEvent *event)
{
    LayoutTheme& layoutTheme = LayoutTheme::theme();
    int keyCode = event->keyCode();

    return ((layoutTheme.popsMenuBySpaceKey() && event->keyCode() == ' ' && !m_typeAhead.hasActiveSession(event))
        || (layoutTheme.popsMenuByReturnKey() && keyCode == '\r'));
}

void HTMLSelectElement::menuListDefaultEventHandler(Event* event)
{
    if (event->type() == EventTypeNames::keydown) {
        if (!layoutObject() || !event->isKeyboardEvent())
            return;

        KeyboardEvent* keyEvent = toKeyboardEvent(event);
        if (shouldOpenPopupForKeyDownEvent(keyEvent)) {
            handlePopupOpenKeyboardEvent(event);
            return;
        }

        // When using spatial navigation, we want to be able to navigate away
        // from the select element when the user hits any of the arrow keys,
        // instead of changing the selection.
        if (isSpatialNavigationEnabled(document().frame())) {
            if (!m_activeSelectionState)
                return;
        }

        // The key handling below shouldn't be used for non spatial navigation
        // mode Mac
        if (LayoutTheme::theme().popsMenuByArrowKeys() && !isSpatialNavigationEnabled(document().frame()))
            return;

        const String& key = keyEvent->key();
        bool handled = true;
        const ListItems& listItems = this->listItems();
        HTMLOptionElement* option = selectedOption();
        int listIndex = option ? option->listIndex() : -1;

        if (key == "ArrowDown" || key == "ArrowRight")
            option = nextValidOption(listIndex, SkipForwards, 1);
        else if (key == "ArrowUp" || key == "ArrowLeft")
            option = nextValidOption(listIndex, SkipBackwards, 1);
        else if (key == "PageDown")
            option = nextValidOption(listIndex, SkipForwards, 3);
        else if (key == "PageUp")
            option = nextValidOption(listIndex, SkipBackwards, 3);
        else if (key == "Home")
            option = nextValidOption(-1, SkipForwards, 1);
        else if (key == "End")
            option = nextValidOption(listItems.size(), SkipBackwards, 1);
        else
            handled = false;

        if (handled && option)
            selectOption(option, DeselectOtherOptions | MakeOptionDirty | DispatchInputAndChangeEvent);

        if (handled)
            event->setDefaultHandled();
    }

    if (event->type() == EventTypeNames::keypress) {
        if (!layoutObject() || !event->isKeyboardEvent())
            return;

        int keyCode = toKeyboardEvent(event)->keyCode();
        if (keyCode == ' ' && isSpatialNavigationEnabled(document().frame())) {
            // Use space to toggle arrow key handling for selection change or
            // spatial navigation.
            m_activeSelectionState = !m_activeSelectionState;
            event->setDefaultHandled();
            return;
        }

        KeyboardEvent* keyEvent = toKeyboardEvent(event);
        if (shouldOpenPopupForKeyPressEvent(keyEvent)) {
            handlePopupOpenKeyboardEvent(event);
            return;
        }

        if (!LayoutTheme::theme().popsMenuByReturnKey() && keyCode == '\r') {
            if (form())
                form()->submitImplicitly(event, false);
            dispatchInputAndChangeEventForMenuList();
            event->setDefaultHandled();
        }
    }

    if (event->type() == EventTypeNames::mousedown && event->isMouseEvent() && toMouseEvent(event)->button() == LeftButton) {
        InputDeviceCapabilities* sourceCapabilities = toMouseEvent(event)->fromTouch() ? InputDeviceCapabilities::firesTouchEventsSourceCapabilities() : InputDeviceCapabilities::doesntFireTouchEventsSourceCapabilities();
        focus(FocusParams(SelectionBehaviorOnFocus::Restore, WebFocusTypeNone, sourceCapabilities));
        if (layoutObject() && layoutObject()->isMenuList() && !isDisabledFormControl()) {
            if (popupIsVisible()) {
                hidePopup();
            } else {
                // Save the selection so it can be compared to the new selection
                // when we call onChange during selectOption, which gets called
                // from valueChanged, which gets called after the user makes a
                // selection from the menu.
                saveLastSelection();
                // TODO(lanwei): Will check if we need to add
                // InputDeviceCapabilities here when select menu list gets
                // focus, see https://crbug.com/476530.
                showPopup();
            }
        }
        event->setDefaultHandled();
    }

    if (event->type() == EventTypeNames::blur) {
        if (popupIsVisible())
            hidePopup();
    }
}

void HTMLSelectElement::updateSelectedState(HTMLOptionElement* clickedOption, bool multi, bool shift)
{
    ASSERT(clickedOption);
    // Save the selection so it can be compared to the new selection when
    // dispatching change events during mouseup, or after autoscroll finishes.
    saveLastSelection();

    m_activeSelectionState = true;

    bool shiftSelect = m_multiple && shift;
    bool multiSelect = m_multiple && multi && !shift;

    // Keep track of whether an active selection (like during drag selection),
    // should select or deselect.
    if (clickedOption->selected() && multiSelect) {
        m_activeSelectionState = false;
        clickedOption->setSelectedState(false);
        clickedOption->setDirty(true);
    }

    // If we're not in any special multiple selection mode, then deselect all
    // other items, excluding the clicked option. If no option was clicked, then
    // this will deselect all items in the list.
    if (!shiftSelect && !multiSelect)
        deselectItemsWithoutValidation(clickedOption);

    // If the anchor hasn't been set, and we're doing a single selection or a
    // shift selection, then initialize the anchor to the first selected index.
    if (!m_activeSelectionAnchor && !multiSelect)
        setActiveSelectionAnchor(selectedOption());

    // Set the selection state of the clicked option.
    if (!clickedOption->isDisabledFormControl()) {
        clickedOption->setSelectedState(true);
        clickedOption->setDirty(true);
    }

    // If there was no selectedIndex() for the previous initialization, or If
    // we're doing a single selection, or a multiple selection (using cmd or
    // ctrl), then initialize the anchor index to the listIndex that just got
    // clicked.
    if (!m_activeSelectionAnchor || !shiftSelect)
        setActiveSelectionAnchor(clickedOption);

    setActiveSelectionEnd(clickedOption);
    updateListBoxSelection(!multiSelect);
}

HTMLOptionElement* HTMLSelectElement::eventTargetOption(const Event& event)
{
    Node* targetNode = event.target()->toNode();
    if (!targetNode || !isHTMLOptionElement(*targetNode))
        return nullptr;
    return toHTMLOptionElement(targetNode);
}

int HTMLSelectElement::listIndexForOption(const HTMLOptionElement& option)
{
    const ListItems& items = this->listItems();
    size_t length = items.size();
    for (size_t i = 0; i < length; ++i) {
        if (items[i].get() == &option)
            return i;
    }
    return -1;
}

AutoscrollController* HTMLSelectElement::autoscrollController() const
{
    if (Page* page = document().page())
        return &page->autoscrollController();
    return nullptr;
}

void HTMLSelectElement::handleMouseRelease()
{
    // We didn't start this click/drag on any options.
    if (m_lastOnChangeSelection.isEmpty())
        return;
    listBoxOnChange();
}

void HTMLSelectElement::listBoxDefaultEventHandler(Event* event)
{
    if (event->type() == EventTypeNames::gesturetap && event->isGestureEvent()) {
        focus();
        // Calling focus() may cause us to lose our layoutObject or change the
        // layoutObject type, in which case do not want to handle the event.
        if (!layoutObject() || !layoutObject()->isListBox())
            return;

        // Convert to coords relative to the list box if needed.
        GestureEvent& gestureEvent = toGestureEvent(*event);
        if (HTMLOptionElement* option = eventTargetOption(gestureEvent)) {
            if (!isDisabledFormControl()) {
                updateSelectedState(option, true, gestureEvent.shiftKey());
                listBoxOnChange();
            }
            event->setDefaultHandled();
        }

    } else if (event->type() == EventTypeNames::mousedown && event->isMouseEvent() && toMouseEvent(event)->button() == LeftButton) {
        focus();
        // Calling focus() may cause us to lose our layoutObject, in which case
        // do not want to handle the event.
        if (!layoutObject() || !layoutObject()->isListBox() || isDisabledFormControl())
            return;

        // Convert to coords relative to the list box if needed.
        MouseEvent* mouseEvent = toMouseEvent(event);
        if (HTMLOptionElement* option = eventTargetOption(*mouseEvent)) {
            if (!isDisabledFormControl()) {
#if OS(MACOSX)
                updateSelectedState(option, mouseEvent->metaKey(), mouseEvent->shiftKey());
#else
                updateSelectedState(option, mouseEvent->ctrlKey(), mouseEvent->shiftKey());
#endif
            }
            if (LocalFrame* frame = document().frame())
                frame->eventHandler().setMouseDownMayStartAutoscroll();

            event->setDefaultHandled();
        }

    } else if (event->type() == EventTypeNames::mousemove && event->isMouseEvent()) {
        MouseEvent* mouseEvent = toMouseEvent(event);
        if (mouseEvent->button() != LeftButton || !mouseEvent->buttonDown())
            return;

        if (Page* page = document().page())
            page->autoscrollController().startAutoscrollForSelection(layoutObject());
        // Mousedown didn't happen in this element.
        if (m_lastOnChangeSelection.isEmpty())
            return;

        if (HTMLOptionElement* option = eventTargetOption(*mouseEvent)) {
            if (!isDisabledFormControl()) {
                if (m_multiple) {
                    // Only extend selection if there is something selected.
                    if (!m_activeSelectionAnchor)
                        return;

                    setActiveSelectionEnd(option);
                    updateListBoxSelection(false);
                } else {
                    setActiveSelectionAnchor(option);
                    setActiveSelectionEnd(option);
                    updateListBoxSelection(true);
                }
            }
        }

    } else if (event->type() == EventTypeNames::mouseup && event->isMouseEvent() && toMouseEvent(event)->button() == LeftButton && layoutObject()) {
        if (document().page() && document().page()->autoscrollController().autoscrollInProgress(toLayoutBox(layoutObject())))
            document().page()->autoscrollController().stopAutoscroll();
        else
            handleMouseRelease();

    } else if (event->type() == EventTypeNames::keydown) {
        if (!event->isKeyboardEvent())
            return;
        const String& key = toKeyboardEvent(event)->key();

        bool handled = false;
        HTMLOptionElement* endOption = nullptr;
        if (!m_activeSelectionEnd) {
            // Initialize the end index
            if (key == "ArrowDown" || key == "PageDown") {
                HTMLOptionElement* startOption = lastSelectedOption();
                handled = true;
                if (key == "ArrowDown")
                    endOption = nextSelectableOption(startOption);
                else
                    endOption = nextSelectableOptionPageAway(startOption, SkipForwards);
            } else if (key == "ArrowUp" || key == "PageUp") {
                HTMLOptionElement* startOption = selectedOption();
                handled = true;
                if (key == "ArrowUp")
                    endOption = previousSelectableOption(startOption);
                else
                    endOption = nextSelectableOptionPageAway(startOption, SkipBackwards);
            }
        } else {
            // Set the end index based on the current end index.
            if (key == "ArrowDown") {
                endOption = nextSelectableOption(m_activeSelectionEnd.get());
                handled = true;
            } else if (key == "ArrowUp") {
                endOption = previousSelectableOption(m_activeSelectionEnd.get());
                handled = true;
            } else if (key == "PageDown") {
                endOption = nextSelectableOptionPageAway(m_activeSelectionEnd.get(), SkipForwards);
                handled = true;
            } else if (key == "PageUp") {
                endOption = nextSelectableOptionPageAway(m_activeSelectionEnd.get(), SkipBackwards);
                handled = true;
            }
        }
        if (key == "Home") {
            endOption = firstSelectableOption();
            handled = true;
        } else if (key == "End") {
            endOption = lastSelectableOption();
            handled = true;
        }

        if (isSpatialNavigationEnabled(document().frame())) {
            // Check if the selection moves to the boundary.
            if (key == "ArrowLeft" || key == "ArrowRight" || ((key == "ArrowDown" || key == "ArrowUp") && endOption == m_activeSelectionEnd))
                return;
        }

        if (endOption && handled) {
            // Save the selection so it can be compared to the new selection
            // when dispatching change events immediately after making the new
            // selection.
            saveLastSelection();

            setActiveSelectionEnd(endOption);

            bool selectNewItem = !m_multiple || toKeyboardEvent(event)->shiftKey() || !isSpatialNavigationEnabled(document().frame());
            if (selectNewItem)
                m_activeSelectionState = true;
            // If the anchor is unitialized, or if we're going to deselect all
            // other options, then set the anchor index equal to the end index.
            bool deselectOthers = !m_multiple || (!toKeyboardEvent(event)->shiftKey() && selectNewItem);
            if (!m_activeSelectionAnchor || deselectOthers) {
                if (deselectOthers)
                    deselectItemsWithoutValidation();
                setActiveSelectionAnchor(m_activeSelectionEnd.get());
            }

            scrollToOption(endOption);
            if (selectNewItem) {
                updateListBoxSelection(deselectOthers);
                listBoxOnChange();
            } else {
                scrollToSelection();
            }

            event->setDefaultHandled();
        }

    } else if (event->type() == EventTypeNames::keypress) {
        if (!event->isKeyboardEvent())
            return;
        int keyCode = toKeyboardEvent(event)->keyCode();

        if (keyCode == '\r') {
            if (form())
                form()->submitImplicitly(event, false);
            event->setDefaultHandled();
        } else if (m_multiple && keyCode == ' ' && isSpatialNavigationEnabled(document().frame())) {
            // Use space to toggle selection change.
            m_activeSelectionState = !m_activeSelectionState;
            updateSelectedState(m_activeSelectionEnd.get(), true /*multi*/, false /*shift*/);
            listBoxOnChange();
            event->setDefaultHandled();
        }
    }
}

void HTMLSelectElement::defaultEventHandler(Event* event)
{
    if (!layoutObject())
        return;

    if (isDisabledFormControl()) {
        HTMLFormControlElementWithState::defaultEventHandler(event);
        return;
    }

    if (usesMenuList())
        menuListDefaultEventHandler(event);
    else
        listBoxDefaultEventHandler(event);
    if (event->defaultHandled())
        return;

    if (event->type() == EventTypeNames::keypress && event->isKeyboardEvent()) {
        KeyboardEvent* keyboardEvent = toKeyboardEvent(event);
        if (!keyboardEvent->ctrlKey() && !keyboardEvent->altKey() && !keyboardEvent->metaKey() && isPrintableChar(keyboardEvent->charCode())) {
            typeAheadFind(keyboardEvent);
            event->setDefaultHandled();
            return;
        }
    }
    HTMLFormControlElementWithState::defaultEventHandler(event);
}

HTMLOptionElement* HTMLSelectElement::lastSelectedOption() const
{
    const ListItems& items = listItems();
    for (size_t i = items.size(); i;) {
        HTMLElement* element = items[--i];
        if (isHTMLOptionElement(*element) && toHTMLOptionElement(element)->selected())
            return toHTMLOptionElement(element);
    }
    return nullptr;
}

int HTMLSelectElement::indexOfSelectedOption() const
{
    return optionToListIndex(selectedIndex());
}

int HTMLSelectElement::optionCount() const
{
    return listItems().size();
}

String HTMLSelectElement::optionAtIndex(int index) const
{
    const ListItems& items = listItems();
    HTMLElement* element = items[index];
    if (!isHTMLOptionElement(*element) || toHTMLOptionElement(element)->isDisabledFormControl())
        return String();
    return toHTMLOptionElement(element)->displayLabel();
}

void HTMLSelectElement::typeAheadFind(KeyboardEvent* event)
{
    int index = m_typeAhead.handleEvent(event, TypeAhead::MatchPrefix | TypeAhead::CycleFirstChar);
    if (index < 0)
        return;
    selectOption(listToOptionIndex(index), DeselectOtherOptions | MakeOptionDirty | DispatchInputAndChangeEvent);
    if (!usesMenuList())
        listBoxOnChange();
}

void HTMLSelectElement::accessKeySetSelectedIndex(int index)
{
    // First bring into focus the list box.
    if (!focused())
        accessKeyAction(false);

    const ListItems& items = listItems();
    int listIndex = optionToListIndex(index);
    if (listIndex < 0)
        return;
    HTMLElement& element = *items[listIndex];
    if (!isHTMLOptionElement(element))
        return;
    EventQueueScope scope;
    // If this index is already selected, unselect. otherwise update the
    // selected index.
    SelectOptionFlags flags = DispatchInputAndChangeEvent | (multiple() ? 0 : DeselectOtherOptions);
    if (toHTMLOptionElement(element).selected()) {
        if (usesMenuList())
            selectOption(-1, flags);
        else
            toHTMLOptionElement(element).setSelectedState(false);
    } else {
        selectOption(index, flags);
    }
    toHTMLOptionElement(element).setDirty(true);
    if (usesMenuList())
        return;
    listBoxOnChange();
    scrollToSelection();
}

unsigned HTMLSelectElement::length() const
{
    unsigned options = 0;
    for (auto& item : listItems()) {
        if (isHTMLOptionElement(*item))
            ++options;
    }
    return options;
}

void HTMLSelectElement::finishParsingChildren()
{
    HTMLFormControlElementWithState::finishParsingChildren();
    if (usesMenuList())
        return;
    scrollToOption(selectedOption());
    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->listboxActiveIndexChanged(this);
}

bool HTMLSelectElement::anonymousIndexedSetter(unsigned index, HTMLOptionElement* value, ExceptionState& exceptionState)
{
    if (!value) { // undefined or null
        remove(index);
        return true;
    }
    setOption(index, value, exceptionState);
    return true;
}

bool HTMLSelectElement::isInteractiveContent() const
{
    return true;
}

bool HTMLSelectElement::supportsAutofocus() const
{
    return true;
}

void HTMLSelectElement::updateListOnLayoutObject()
{
    setOptionsChangedOnLayoutObject();
}

DEFINE_TRACE(HTMLSelectElement)
{
    visitor->trace(m_listItems);
    visitor->trace(m_lastOnChangeOption);
    visitor->trace(m_activeSelectionAnchor);
    visitor->trace(m_activeSelectionEnd);
    visitor->trace(m_optionToScrollTo);
    visitor->trace(m_popup);
    visitor->trace(m_popupUpdater);
    HTMLFormControlElementWithState::trace(visitor);
}

void HTMLSelectElement::didAddUserAgentShadowRoot(ShadowRoot& root)
{
    HTMLContentElement* content = HTMLContentElement::create(document());
    content->setAttribute(selectAttr, "option,optgroup,hr");
    root.appendChild(content);
}

HTMLOptionElement* HTMLSelectElement::spatialNavigationFocusedOption()
{
    if (!isSpatialNavigationEnabled(document().frame()))
        return nullptr;
    HTMLOptionElement* focusedOption = activeSelectionEnd();
    if (!focusedOption)
        focusedOption = firstSelectableOption();
    return focusedOption;
}

String HTMLSelectElement::itemText(const Element& element) const
{
    String itemString;
    if (isHTMLOptGroupElement(element))
        itemString = toHTMLOptGroupElement(element).groupLabelText();
    else if (isHTMLOptionElement(element))
        itemString = toHTMLOptionElement(element).textIndentedToRespectGroupLabel();

    if (layoutObject())
        applyTextTransform(layoutObject()->style(), itemString, ' ');
    return itemString;
}

bool HTMLSelectElement::itemIsDisplayNone(Element& element) const
{
    if (isHTMLOptionElement(element))
        return toHTMLOptionElement(element).isDisplayNone();
    if (const ComputedStyle* style = itemComputedStyle(element))
        return style->display() == NONE;
    return false;
}

const ComputedStyle* HTMLSelectElement::itemComputedStyle(Element& element) const
{
    return element.computedStyle() ? element.computedStyle() : element.ensureComputedStyle();
}

IntRect HTMLSelectElement::elementRectRelativeToViewport() const
{
    if (!layoutObject())
        return IntRect();
    // Initialize with this frame rectangle relative to the viewport.
    IntRect rect = document().view()->convertToRootFrame(document().view()->boundsRect());
    // We don't use absoluteBoundingBoxRect() because it can return an IntRect
    // larger the actual size by 1px.
    rect.intersect(document().view()->contentsToViewport(roundedIntRect(layoutObject()->absoluteBoundingBoxFloatRect())));
    return rect;
}

LayoutUnit HTMLSelectElement::clientPaddingLeft() const
{
    if (layoutObject() && layoutObject()->isMenuList())
        return toLayoutMenuList(layoutObject())->clientPaddingLeft();
    return LayoutUnit();
}

LayoutUnit HTMLSelectElement::clientPaddingRight() const
{
    if (layoutObject() && layoutObject()->isMenuList())
        return toLayoutMenuList(layoutObject())->clientPaddingRight();
    return LayoutUnit();
}

void HTMLSelectElement::popupDidHide()
{
    m_popupIsVisible = false;
    unobserveTreeMutation();
    if (AXObjectCache* cache = document().existingAXObjectCache()) {
        if (layoutObject() && layoutObject()->isMenuList())
            cache->didHideMenuListPopup(toLayoutMenuList(layoutObject()));
    }
}

void HTMLSelectElement::setIndexToSelectOnCancel(int listIndex)
{
    m_indexToSelectOnCancel = listIndex;
    if (layoutObject())
        layoutObject()->updateFromElement();
}

int HTMLSelectElement::optionIndexToBeShown() const
{
    if (m_indexToSelectOnCancel >= 0)
        return listToOptionIndex(m_indexToSelectOnCancel);
    if (suggestedIndex() >= 0)
        return suggestedIndex();
    return selectedIndex();
}

void HTMLSelectElement::valueChanged(unsigned listIndex)
{
    // Check to ensure a page navigation has not occurred while the popup was
    // up.
    Document& doc = document();
    if (&doc != doc.frame()->document())
        return;

    setIndexToSelectOnCancel(-1);
    optionSelectedByUser(listToOptionIndex(listIndex), true);
}

void HTMLSelectElement::popupDidCancel()
{
    if (m_indexToSelectOnCancel >= 0)
        valueChanged(m_indexToSelectOnCancel);
}

void HTMLSelectElement::provisionalSelectionChanged(unsigned listIndex)
{
    setIndexToSelectOnCancel(listIndex);
}

void HTMLSelectElement::showPopup()
{
    if (popupIsVisible())
        return;
    if (document().frameHost()->chromeClient().hasOpenedPopup())
        return;
    if (!layoutObject() || !layoutObject()->isMenuList())
        return;
    // Disable visibility check on Android.  elementRectRelativeToViewport()
    // doesn't work well on Android WebView.  crbug.com/632561
#if !OS(ANDROID)
    if (elementRectRelativeToViewport().isEmpty())
        return;
#endif

    if (!m_popup)
        m_popup = document().frameHost()->chromeClient().openPopupMenu(*document().frame(), *this);
    m_popupIsVisible = true;
    observeTreeMutation();

    LayoutMenuList* menuList = toLayoutMenuList(layoutObject());
    m_popup->show();
    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->didShowMenuListPopup(menuList);
}

void HTMLSelectElement::hidePopup()
{
    if (m_popup)
        m_popup->hide();
}

void HTMLSelectElement::didRecalcStyle(StyleRecalcChange change)
{
    HTMLFormControlElementWithState::didRecalcStyle(change);
    if (popupIsVisible())
        m_popup->updateFromElement(PopupMenu::ByStyleChange);
}

void HTMLSelectElement::detach(const AttachContext& context)
{
    HTMLFormControlElementWithState::detach(context);
    if (m_popup)
        m_popup->disconnectClient();
    m_popupIsVisible = false;
    m_popup = nullptr;
    unobserveTreeMutation();
}

void HTMLSelectElement::resetTypeAheadSessionForTesting()
{
    m_typeAhead.resetSession();
}

// PopupUpdater notifies updates of the specified SELECT element subtree to
// a PopupMenu object.
class HTMLSelectElement::PopupUpdater : public MutationCallback {
public:
    explicit PopupUpdater(HTMLSelectElement&);
    DECLARE_VIRTUAL_TRACE();

    void dispose()
    {
        m_observer->disconnect();
    }

private:
    void call(const HeapVector<Member<MutationRecord>>&, MutationObserver*) override
    {
        // We disconnect the MutationObserver when a popuup is closed.  However
        // MutationObserver can call back after disconnection.
        if (!m_select->popupIsVisible())
            return;
        m_select->didMutateSubtree();
    }

    ExecutionContext* getExecutionContext() const override
    {
        return &m_select->document();
    }

    Member<HTMLSelectElement> m_select;
    Member<MutationObserver> m_observer;
};

HTMLSelectElement::PopupUpdater::PopupUpdater(HTMLSelectElement& select)
    : m_select(select)
{
    m_observer = MutationObserver::create(this);
    Vector<String> filter;
    filter.reserveCapacity(4);
    // Observe only attributes which affect popup content.
    filter.append(String("disabled"));
    filter.append(String("label"));
    filter.append(String("selected"));
    filter.append(String("value"));
    MutationObserverInit init;
    init.setAttributes(true);
    init.setAttributeFilter(filter);
    init.setCharacterData(true);
    init.setChildList(true);
    init.setSubtree(true);
    m_observer->observe(&select, init, ASSERT_NO_EXCEPTION);
}

DEFINE_TRACE(HTMLSelectElement::PopupUpdater)
{
    visitor->trace(m_select);
    visitor->trace(m_observer);
    MutationCallback::trace(visitor);
}

void HTMLSelectElement::observeTreeMutation()
{
    DCHECK(!m_popupUpdater);
    m_popupUpdater = new PopupUpdater(*this);
}

void HTMLSelectElement::unobserveTreeMutation()
{
    if (!m_popupUpdater)
        return;
    m_popupUpdater->dispose();
    m_popupUpdater = nullptr;
}

void HTMLSelectElement::didMutateSubtree()
{
    DCHECK(popupIsVisible());
    DCHECK(m_popup);
    m_popup->updateFromElement(PopupMenu::ByDOMChange);
}

} // namespace blink
