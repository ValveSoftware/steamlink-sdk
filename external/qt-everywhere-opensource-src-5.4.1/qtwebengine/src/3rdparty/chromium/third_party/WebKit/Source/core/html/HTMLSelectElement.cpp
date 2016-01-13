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

#include "config.h"
#include "core/html/HTMLSelectElement.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/HTMLNames.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/dom/Attribute.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeTraversal.h"
#include "core/events/GestureEvent.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/MouseEvent.h"
#include "core/frame/LocalFrame.h"
#include "core/html/FormDataList.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/forms/FormController.h"
#include "core/page/EventHandler.h"
#include "core/page/SpatialNavigation.h"
#include "core/rendering/RenderListBox.h"
#include "core/rendering/RenderMenuList.h"
#include "core/rendering/RenderTheme.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/text/PlatformLocale.h"

using namespace WTF::Unicode;

namespace WebCore {

using namespace HTMLNames;

// Upper limit agreed upon with representatives of Opera and Mozilla.
static const unsigned maxSelectItems = 10000;

HTMLSelectElement::HTMLSelectElement(Document& document, HTMLFormElement* form)
    : HTMLFormControlElementWithState(selectTag, document, form)
    , m_typeAhead(this)
    , m_size(0)
    , m_lastOnChangeIndex(-1)
    , m_activeSelectionAnchorIndex(-1)
    , m_activeSelectionEndIndex(-1)
    , m_isProcessingUserDrivenChange(false)
    , m_multiple(false)
    , m_activeSelectionState(false)
    , m_shouldRecalcListItems(false)
    , m_suggestedIndex(-1)
{
    ScriptWrappable::init(this);
    setHasCustomStyleCallbacks();
}

PassRefPtrWillBeRawPtr<HTMLSelectElement> HTMLSelectElement::create(Document& document)
{
    return adoptRefWillBeNoop(new HTMLSelectElement(document, 0));
}

PassRefPtrWillBeRawPtr<HTMLSelectElement> HTMLSelectElement::create(Document& document, HTMLFormElement* form)
{
    return adoptRefWillBeNoop(new HTMLSelectElement(document, form));
}

const AtomicString& HTMLSelectElement::formControlType() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, selectMultiple, ("select-multiple", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const AtomicString, selectOne, ("select-one", AtomicString::ConstructFromLiteral));
    return m_multiple ? selectMultiple : selectOne;
}

void HTMLSelectElement::optionSelectedByUser(int optionIndex, bool fireOnChangeNow, bool allowMultipleSelection)
{
    // User interaction such as mousedown events can cause list box select elements to send change events.
    // This produces that same behavior for changes triggered by other code running on behalf of the user.
    if (!usesMenuList()) {
        updateSelectedState(optionToListIndex(optionIndex), allowMultipleSelection, false);
        setNeedsValidityCheck();
        if (fireOnChangeNow)
            listBoxOnChange();
        return;
    }

    // Bail out if this index is already the selected one, to avoid running unnecessary JavaScript that can mess up
    // autofill when there is no actual change (see https://bugs.webkit.org/show_bug.cgi?id=35256 and <rdar://7467917>).
    // The selectOption function does not behave this way, possibly because other callers need a change event even
    // in cases where the selected option is not change.
    if (optionIndex == selectedIndex())
        return;

    selectOption(optionIndex, DeselectOtherOptions | (fireOnChangeNow ? DispatchInputAndChangeEvent : 0) | UserDriven);
}

bool HTMLSelectElement::hasPlaceholderLabelOption() const
{
    // The select element has no placeholder label option if it has an attribute "multiple" specified or a display size of non-1.
    //
    // The condition "size() > 1" is not compliant with the HTML5 spec as of Dec 3, 2010. "size() != 1" is correct.
    // Using "size() > 1" here because size() may be 0 in WebKit.
    // See the discussion at https://bugs.webkit.org/show_bug.cgi?id=43887
    //
    // "0 size()" happens when an attribute "size" is absent or an invalid size attribute is specified.
    // In this case, the display size should be assumed as the default.
    // The default display size is 1 for non-multiple select elements, and 4 for multiple select elements.
    //
    // Finally, if size() == 0 and non-multiple, the display size can be assumed as 1.
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
        return locale().queryString(blink::WebLocalizedString::ValidationValueMissingForSelect);
    return String();
}

bool HTMLSelectElement::valueMissing() const
{
    if (!willValidate())
        return false;

    if (!isRequired())
        return false;

    int firstSelectionIndex = selectedIndex();

    // If a non-placeholer label option is selected (firstSelectionIndex > 0), it's not value-missing.
    return firstSelectionIndex < 0 || (!firstSelectionIndex && hasPlaceholderLabelOption());
}

void HTMLSelectElement::listBoxSelectItem(int listIndex, bool allowMultiplySelections, bool shift, bool fireOnChangeNow)
{
    if (!multiple())
        optionSelectedByUser(listToOptionIndex(listIndex), fireOnChangeNow, false);
    else {
        updateSelectedState(listIndex, allowMultiplySelections, shift);
        setNeedsValidityCheck();
        if (fireOnChangeNow)
            listBoxOnChange();
    }
}

bool HTMLSelectElement::usesMenuList() const
{
    if (RenderTheme::theme().delegatesMenuListRendering())
        return true;

    return !m_multiple && m_size <= 1;
}

int HTMLSelectElement::activeSelectionStartListIndex() const
{
    if (m_activeSelectionAnchorIndex >= 0)
        return m_activeSelectionAnchorIndex;
    return optionToListIndex(selectedIndex());
}

int HTMLSelectElement::activeSelectionEndListIndex() const
{
    if (m_activeSelectionEndIndex >= 0)
        return m_activeSelectionEndIndex;
    return lastSelectedListIndex();
}

void HTMLSelectElement::add(HTMLElement* element, HTMLElement* before, ExceptionState& exceptionState)
{
    // Make sure the element is ref'd and deref'd so we don't leak it.
    RefPtrWillBeRawPtr<HTMLElement> protectNewChild(element);

    if (!element || !(isHTMLOptionElement(element) || isHTMLOptGroupElement(element) || isHTMLHRElement(element)))
        return;

    insertBefore(element, before, exceptionState);
    setNeedsValidityCheck();
}

void HTMLSelectElement::addBeforeOptionAtIndex(HTMLElement* element, int beforeIndex, ExceptionState& exceptionState)
{
    HTMLElement* beforeElement = toHTMLElement(options()->item(beforeIndex));
    add(element, beforeElement, exceptionState);
}

void HTMLSelectElement::remove(int optionIndex)
{
    int listIndex = optionToListIndex(optionIndex);
    if (listIndex < 0)
        return;

    listItems()[listIndex]->remove(IGNORE_EXCEPTION);
}

String HTMLSelectElement::value() const
{
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& items = listItems();
    for (unsigned i = 0; i < items.size(); i++) {
        if (isHTMLOptionElement(items[i]) && toHTMLOptionElement(items[i])->selected())
            return toHTMLOptionElement(items[i])->value();
    }
    return "";
}

void HTMLSelectElement::setValue(const String &value, bool sendEvents)
{
    // We clear the previously selected option(s) when needed, to guarantee calling setSelectedIndex() only once.
    int optionIndex = 0;
    if (value.isNull()) {
        optionIndex = -1;
    } else {
        // Find the option with value() matching the given parameter and make it the current selection.
        const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& items = listItems();
        for (unsigned i = 0; i < items.size(); i++) {
            if (isHTMLOptionElement(items[i])) {
                if (toHTMLOptionElement(items[i])->value() == value)
                    break;
                optionIndex++;
            }
        }
        if (optionIndex >= static_cast<int>(items.size()))
            optionIndex = -1;
    }

    int previousSelectedIndex = selectedIndex();
    setSuggestedIndex(-1);
    setSelectedIndex(optionIndex);

    if (sendEvents && previousSelectedIndex != selectedIndex()) {
        if (usesMenuList())
            dispatchInputAndChangeEventForMenuList(false);
        else
            listBoxOnChange();
    }
}

String HTMLSelectElement::suggestedValue() const
{
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& items = listItems();
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

    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& items = listItems();
    unsigned optionIndex = 0;
    for (unsigned i = 0; i < items.size(); ++i) {
        if (isHTMLOptionElement(items[i])) {
            if (toHTMLOptionElement(items[i])->value() == value) {
                setSuggestedIndex(optionIndex);
                return;
            }
            optionIndex++;
        }
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

void HTMLSelectElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == sizeAttr) {
        int oldSize = m_size;
        // Set the attribute value to a number.
        // This is important since the style rules for this attribute can determine the appearance property.
        int size = value.toInt();
        AtomicString attrSize = AtomicString::number(size);
        if (attrSize != value) {
            // FIXME: This is horribly factored.
            if (Attribute* sizeAttribute = ensureUniqueElementData().findAttributeByName(sizeAttr))
                sizeAttribute->setValue(attrSize);
        }
        size = std::max(size, 1);

        // Ensure that we've determined selectedness of the items at least once prior to changing the size.
        if (oldSize != size)
            updateListItemSelectedStates();

        m_size = size;
        setNeedsValidityCheck();
        if (m_size != oldSize && inActiveDocument()) {
            lazyReattachIfAttached();
            setRecalcListItems();
        }
    } else if (name == multipleAttr)
        parseMultipleAttribute(value);
    else if (name == accesskeyAttr) {
        // FIXME: ignore for the moment.
        //
    } else if (name == disabledAttr) {
        HTMLFormControlElementWithState::parseAttribute(name, value);
        if (renderer() && renderer()->isMenuList()) {
            if (RenderMenuList* menuList = toRenderMenuList(renderer())) {
                if (menuList->popupIsVisible())
                    menuList->hidePopup();
            }
        }

    } else
        HTMLFormControlElementWithState::parseAttribute(name, value);
}

bool HTMLSelectElement::shouldShowFocusRingOnMouseFocus() const
{
    return true;
}

bool HTMLSelectElement::canSelectAll() const
{
    return !usesMenuList();
}

RenderObject* HTMLSelectElement::createRenderer(RenderStyle*)
{
    if (usesMenuList())
        return new RenderMenuList(this);
    return new RenderListBox(this);
}

PassRefPtrWillBeRawPtr<HTMLCollection> HTMLSelectElement::selectedOptions()
{
    updateListItemSelectedStates();
    return ensureCachedHTMLCollection(SelectedOptions);
}

PassRefPtrWillBeRawPtr<HTMLOptionsCollection> HTMLSelectElement::options()
{
    return toHTMLOptionsCollection(ensureCachedHTMLCollection(SelectOptions).get());
}

void HTMLSelectElement::updateListItemSelectedStates()
{
    if (!m_shouldRecalcListItems)
        return;
    recalcListItems();
    setNeedsValidityCheck();
}

void HTMLSelectElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    setRecalcListItems();
    setNeedsValidityCheck();
    m_lastOnChangeSelection.clear();

    HTMLFormControlElementWithState::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
}

void HTMLSelectElement::optionElementChildrenChanged()
{
    setRecalcListItems();
    setNeedsValidityCheck();

    if (renderer()) {
        if (AXObjectCache* cache = renderer()->document().existingAXObjectCache())
            cache->childrenChanged(this);
    }
}

void HTMLSelectElement::accessKeyAction(bool sendMouseEvents)
{
    focus();
    dispatchSimulatedClick(0, sendMouseEvents ? SendMouseUpDownEvents : SendNoEvents);
}

void HTMLSelectElement::setMultiple(bool multiple)
{
    bool oldMultiple = this->multiple();
    int oldSelectedIndex = selectedIndex();
    setAttribute(multipleAttr, multiple ? emptyAtom : nullAtom);

    // Restore selectedIndex after changing the multiple flag to preserve
    // selection as single-line and multi-line has different defaults.
    if (oldMultiple != this->multiple())
        setSelectedIndex(oldSelectedIndex);
}

void HTMLSelectElement::setSize(int size)
{
    setIntegralAttribute(sizeAttr, size);
}

Element* HTMLSelectElement::namedItem(const AtomicString& name)
{
    return options()->namedItem(name);
}

Element* HTMLSelectElement::item(unsigned index)
{
    return options()->item(index);
}

void HTMLSelectElement::setOption(unsigned index, HTMLOptionElement* option, ExceptionState& exceptionState)
{
    if (index > maxSelectItems - 1)
        index = maxSelectItems - 1;
    int diff = index - length();
    RefPtrWillBeRawPtr<HTMLElement> before = nullptr;
    // Out of array bounds? First insert empty dummies.
    if (diff > 0) {
        setLength(index, exceptionState);
        // Replace an existing entry?
    } else if (diff < 0) {
        before = toHTMLElement(options()->item(index+1));
        remove(index);
    }
    // Finally add the new element.
    if (!exceptionState.hadException()) {
        add(option, before.get(), exceptionState);
        if (diff >= 0 && option->selected())
            optionSelectionStateChanged(option, true);
    }
}

void HTMLSelectElement::setLength(unsigned newLen, ExceptionState& exceptionState)
{
    if (newLen > maxSelectItems)
        newLen = maxSelectItems;
    int diff = length() - newLen;

    if (diff < 0) { // Add dummy elements.
        do {
            RefPtrWillBeRawPtr<Element> option = document().createElement(optionTag, false);
            ASSERT(option);
            add(toHTMLElement(option), 0, exceptionState);
            if (exceptionState.hadException())
                break;
        } while (++diff);
    } else {
        const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& items = listItems();

        // Removing children fires mutation events, which might mutate the DOM further, so we first copy out a list
        // of elements that we intend to remove then attempt to remove them one at a time.
        WillBeHeapVector<RefPtrWillBeMember<Element> > itemsToRemove;
        size_t optionIndex = 0;
        for (size_t i = 0; i < items.size(); ++i) {
            Element* item = items[i];
            if (isHTMLOptionElement(items[i]) && optionIndex++ >= newLen) {
                ASSERT(item->parentNode());
                itemsToRemove.append(item);
            }
        }

        for (size_t i = 0; i < itemsToRemove.size(); ++i) {
            Element* item = itemsToRemove[i].get();
            if (item->parentNode())
                item->parentNode()->removeChild(item, exceptionState);
        }
    }
    setNeedsValidityCheck();
}

bool HTMLSelectElement::isRequiredFormControl() const
{
    return isRequired();
}

// Returns the 1st valid item |skip| items from |listIndex| in direction |direction| if there is one.
// Otherwise, it returns the valid item closest to that boundary which is past |listIndex| if there is one.
// Otherwise, it returns |listIndex|.
// Valid means that it is enabled and an option element.
int HTMLSelectElement::nextValidIndex(int listIndex, SkipDirection direction, int skip) const
{
    ASSERT(direction == -1 || direction == 1);
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& listItems = this->listItems();
    int lastGoodIndex = listIndex;
    int size = listItems.size();
    for (listIndex += direction; listIndex >= 0 && listIndex < size; listIndex += direction) {
        --skip;
        HTMLElement* element = listItems[listIndex];
        if (!isHTMLOptionElement(*element) || toHTMLOptionElement(element)->isDisabledFormControl() || toHTMLOptionElement(element)->isDisplayNone())
            continue;
        lastGoodIndex = listIndex;
        if (skip <= 0)
            break;
    }
    return lastGoodIndex;
}

int HTMLSelectElement::nextSelectableListIndex(int startIndex) const
{
    return nextValidIndex(startIndex, SkipForwards, 1);
}

int HTMLSelectElement::previousSelectableListIndex(int startIndex) const
{
    if (startIndex == -1)
        startIndex = listItems().size();
    return nextValidIndex(startIndex, SkipBackwards, 1);
}

int HTMLSelectElement::firstSelectableListIndex() const
{
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& items = listItems();
    int index = nextValidIndex(items.size(), SkipBackwards, INT_MAX);
    if (static_cast<size_t>(index) == items.size())
        return -1;
    return index;
}

int HTMLSelectElement::lastSelectableListIndex() const
{
    return nextValidIndex(-1, SkipForwards, INT_MAX);
}

// Returns the index of the next valid item one page away from |startIndex| in direction |direction|.
int HTMLSelectElement::nextSelectableListIndexPageAway(int startIndex, SkipDirection direction) const
{
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& items = listItems();
    // Can't use m_size because renderer forces a minimum size.
    int pageSize = 0;
    if (renderer()->isListBox())
        pageSize = toRenderListBox(renderer())->size() - 1; // -1 so we still show context.

    // One page away, but not outside valid bounds.
    // If there is a valid option item one page away, the index is chosen.
    // If there is no exact one page away valid option, returns startIndex or the most far index.
    int edgeIndex = (direction == SkipForwards) ? 0 : (items.size() - 1);
    int skipAmount = pageSize + ((direction == SkipForwards) ? startIndex : (edgeIndex - startIndex));
    return nextValidIndex(edgeIndex, direction, skipAmount);
}

void HTMLSelectElement::selectAll()
{
    ASSERT(!usesMenuList());
    if (!renderer() || !m_multiple)
        return;

    // Save the selection so it can be compared to the new selectAll selection
    // when dispatching change events.
    saveLastSelection();

    m_activeSelectionState = true;
    setActiveSelectionAnchorIndex(nextSelectableListIndex(-1));
    setActiveSelectionEndIndex(previousSelectableListIndex(-1));

    updateListBoxSelection(false);
    listBoxOnChange();
    setNeedsValidityCheck();
}

void HTMLSelectElement::saveLastSelection()
{
    if (usesMenuList()) {
        m_lastOnChangeIndex = selectedIndex();
        return;
    }

    m_lastOnChangeSelection.clear();
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& items = listItems();
    for (unsigned i = 0; i < items.size(); ++i) {
        HTMLElement* element = items[i];
        m_lastOnChangeSelection.append(isHTMLOptionElement(*element) && toHTMLOptionElement(element)->selected());
    }
}

void HTMLSelectElement::setActiveSelectionAnchorIndex(int index)
{
    m_activeSelectionAnchorIndex = index;

    // Cache the selection state so we can restore the old selection as the new
    // selection pivots around this anchor index.
    m_cachedStateForActiveSelection.clear();

    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& items = listItems();
    for (unsigned i = 0; i < items.size(); ++i) {
        HTMLElement* element = items[i];
        m_cachedStateForActiveSelection.append(isHTMLOptionElement(*element) && toHTMLOptionElement(element)->selected());
    }
}

void HTMLSelectElement::setActiveSelectionEndIndex(int index)
{
    m_activeSelectionEndIndex = index;
}

void HTMLSelectElement::updateListBoxSelection(bool deselectOtherOptions)
{
    ASSERT(renderer() && (renderer()->isListBox() || m_multiple));
    ASSERT(!listItems().size() || m_activeSelectionAnchorIndex >= 0);

    unsigned start = std::min(m_activeSelectionAnchorIndex, m_activeSelectionEndIndex);
    unsigned end = std::max(m_activeSelectionAnchorIndex, m_activeSelectionEndIndex);

    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& items = listItems();
    for (unsigned i = 0; i < items.size(); ++i) {
        HTMLElement* element = items[i];
        if (!isHTMLOptionElement(*element) || toHTMLOptionElement(element)->isDisabledFormControl() || toHTMLOptionElement(element)->isDisplayNone())
            continue;

        if (i >= start && i <= end)
            toHTMLOptionElement(element)->setSelectedState(m_activeSelectionState);
        else if (deselectOtherOptions || i >= m_cachedStateForActiveSelection.size())
            toHTMLOptionElement(element)->setSelectedState(false);
        else
            toHTMLOptionElement(element)->setSelectedState(m_cachedStateForActiveSelection[i]);
    }

    scrollToSelection();
    setNeedsValidityCheck();
    notifyFormStateChanged();
}

void HTMLSelectElement::listBoxOnChange()
{
    ASSERT(!usesMenuList() || m_multiple);

    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& items = listItems();

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
        RefPtrWillBeRawPtr<HTMLSelectElement> protector(this);
        dispatchInputEvent();
        dispatchFormControlChangeEvent();
    }
}

void HTMLSelectElement::dispatchInputAndChangeEventForMenuList(bool requiresUserGesture)
{
    ASSERT(usesMenuList());

    int selected = selectedIndex();
    if (m_lastOnChangeIndex != selected && (!requiresUserGesture || m_isProcessingUserDrivenChange)) {
        m_lastOnChangeIndex = selected;
        m_isProcessingUserDrivenChange = false;
        RefPtrWillBeRawPtr<HTMLSelectElement> protector(this);
        dispatchInputEvent();
        dispatchFormControlChangeEvent();
    }
}

void HTMLSelectElement::scrollToSelection()
{
    if (usesMenuList())
        return;

    if (RenderObject* renderer = this->renderer())
        toRenderListBox(renderer)->selectionChanged();
}

void HTMLSelectElement::setOptionsChangedOnRenderer()
{
    if (RenderObject* renderer = this->renderer()) {
        if (usesMenuList())
            toRenderMenuList(renderer)->setOptionsChanged(true);
        else
            toRenderListBox(renderer)->setOptionsChanged(true);
    }
}

const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& HTMLSelectElement::listItems() const
{
    if (m_shouldRecalcListItems)
        recalcListItems();
    else {
#if ASSERT_ENABLED
        WillBeHeapVector<RawPtrWillBeMember<HTMLElement> > items = m_listItems;
        recalcListItems(false);
        ASSERT(items == m_listItems);
#endif
    }

    return m_listItems;
}

void HTMLSelectElement::invalidateSelectedItems()
{
    if (HTMLCollection* collection = cachedHTMLCollection(SelectedOptions))
        collection->invalidateCache();
}

void HTMLSelectElement::setRecalcListItems()
{
    // FIXME: This function does a bunch of confusing things depending on if it
    // is in the document or not.

    m_shouldRecalcListItems = true;
    // Manual selection anchor is reset when manipulating the select programmatically.
    m_activeSelectionAnchorIndex = -1;
    setOptionsChangedOnRenderer();
    setNeedsStyleRecalc(SubtreeStyleChange);
    if (!inDocument()) {
        if (HTMLCollection* collection = cachedHTMLCollection(SelectOptions))
            collection->invalidateCache();
    }
    if (!inDocument())
        invalidateSelectedItems();

    if (renderer()) {
        if (AXObjectCache* cache = renderer()->document().existingAXObjectCache())
            cache->childrenChanged(this);
    }
}

void HTMLSelectElement::recalcListItems(bool updateSelectedStates) const
{
    m_listItems.clear();

    m_shouldRecalcListItems = false;

    HTMLOptionElement* foundSelected = 0;
    HTMLOptionElement* firstOption = 0;
    for (Element* currentElement = ElementTraversal::firstWithin(*this); currentElement; ) {
        if (!currentElement->isHTMLElement()) {
            currentElement = ElementTraversal::nextSkippingChildren(*currentElement, this);
            continue;
        }
        HTMLElement& current = toHTMLElement(*currentElement);

        // optgroup tags may not nest. However, both FireFox and IE will
        // flatten the tree automatically, so we follow suit.
        // (http://www.w3.org/TR/html401/interact/forms.html#h-17.6)
        if (isHTMLOptGroupElement(current)) {
            m_listItems.append(&current);
            if (Element* nextElement = ElementTraversal::firstWithin(current)) {
                currentElement = nextElement;
                continue;
            }
        }

        if (isHTMLOptionElement(current)) {
            m_listItems.append(&current);

            if (updateSelectedStates && !m_multiple) {
                HTMLOptionElement& option = toHTMLOptionElement(current);
                if (!firstOption)
                    firstOption = &option;
                if (option.selected()) {
                    if (foundSelected)
                        foundSelected->setSelectedState(false);
                    foundSelected = &option;
                } else if (m_size <= 1 && !foundSelected && !option.isDisabledFormControl()) {
                    foundSelected = &option;
                    foundSelected->setSelectedState(true);
                }
            }
        }

        if (isHTMLHRElement(current))
            m_listItems.append(&current);

        // In conforming HTML code, only <optgroup> and <option> will be found
        // within a <select>. We call NodeTraversal::nextSkippingChildren so that we only step
        // into those tags that we choose to. For web-compat, we should cope
        // with the case where odd tags like a <div> have been added but we
        // handle this because such tags have already been removed from the
        // <select>'s subtree at this point.
        currentElement = ElementTraversal::nextSkippingChildren(*currentElement, this);
    }

    if (!foundSelected && m_size <= 1 && firstOption && !firstOption->selected())
        firstOption->setSelectedState(true);
}

int HTMLSelectElement::selectedIndex() const
{
    unsigned index = 0;

    // Return the number of the first option selected.
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& items = listItems();
    for (size_t i = 0; i < items.size(); ++i) {
        HTMLElement* element = items[i];
        if (isHTMLOptionElement(*element)) {
            if (toHTMLOptionElement(*element).selected())
                return index;
            ++index;
        }
    }

    return -1;
}

void HTMLSelectElement::setSelectedIndex(int index)
{
    selectOption(index, DeselectOtherOptions);
}

int HTMLSelectElement::suggestedIndex() const
{
    return m_suggestedIndex;
}

void HTMLSelectElement::setSuggestedIndex(int suggestedIndex)
{
    m_suggestedIndex = suggestedIndex;

    if (RenderObject* renderer = this->renderer())  {
        renderer->updateFromElement();
        if (renderer->isListBox())
            toRenderListBox(renderer)->scrollToRevealElementAtListIndex(suggestedIndex);
    }
}

void HTMLSelectElement::optionSelectionStateChanged(HTMLOptionElement* option, bool optionIsSelected)
{
    ASSERT(option->ownerSelectElement() == this);
    if (optionIsSelected)
        selectOption(option->index());
    else if (!usesMenuList() || multiple())
        selectOption(-1);
    else
        selectOption(nextSelectableListIndex(-1));
}

void HTMLSelectElement::selectOption(int optionIndex, SelectOptionFlags flags)
{
    bool shouldDeselect = !m_multiple || (flags & DeselectOtherOptions);

    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& items = listItems();
    int listIndex = optionToListIndex(optionIndex);

    HTMLElement* element = 0;
    if (listIndex >= 0) {
        element = items[listIndex];
        if (isHTMLOptionElement(*element)) {
            if (m_activeSelectionAnchorIndex < 0 || shouldDeselect)
                setActiveSelectionAnchorIndex(listIndex);
            if (m_activeSelectionEndIndex < 0 || shouldDeselect)
                setActiveSelectionEndIndex(listIndex);
            toHTMLOptionElement(*element).setSelectedState(true);
        }
    }

    if (shouldDeselect)
        deselectItemsWithoutValidation(element);

    // For the menu list case, this is what makes the selected element appear.
    if (RenderObject* renderer = this->renderer())
        renderer->updateFromElement();

    scrollToSelection();

    setNeedsValidityCheck();

    if (usesMenuList()) {
        m_isProcessingUserDrivenChange = flags & UserDriven;
        if (flags & DispatchInputAndChangeEvent)
            dispatchInputAndChangeEventForMenuList();
        if (RenderObject* renderer = this->renderer()) {
            if (usesMenuList())
                toRenderMenuList(renderer)->didSetSelectedIndex(listIndex);
            else if (renderer->isListBox())
                toRenderListBox(renderer)->selectionChanged();
        }
    }

    notifyFormStateChanged();
}

int HTMLSelectElement::optionToListIndex(int optionIndex) const
{
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& items = listItems();
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
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& items = listItems();
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

void HTMLSelectElement::dispatchFocusEvent(Element* oldFocusedElement, FocusType type)
{
    // Save the selection so it can be compared to the new selection when
    // dispatching change events during blur event dispatch.
    if (usesMenuList())
        saveLastSelection();
    HTMLFormControlElementWithState::dispatchFocusEvent(oldFocusedElement, type);
}

void HTMLSelectElement::dispatchBlurEvent(Element* newFocusedElement)
{
    // We only need to fire change events here for menu lists, because we fire
    // change events for list boxes whenever the selection change is actually made.
    // This matches other browsers' behavior.
    if (usesMenuList())
        dispatchInputAndChangeEventForMenuList();
    HTMLFormControlElementWithState::dispatchBlurEvent(newFocusedElement);
}

void HTMLSelectElement::deselectItemsWithoutValidation(HTMLElement* excludeElement)
{
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& items = listItems();
    for (unsigned i = 0; i < items.size(); ++i) {
        HTMLElement* element = items[i];
        if (element != excludeElement && isHTMLOptionElement(*element))
            toHTMLOptionElement(element)->setSelectedState(false);
    }
}

FormControlState HTMLSelectElement::saveFormControlState() const
{
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& items = listItems();
    size_t length = items.size();
    FormControlState state;
    for (unsigned i = 0; i < length; ++i) {
        if (!isHTMLOptionElement(*items[i]))
            continue;
        HTMLOptionElement* option = toHTMLOptionElement(items[i]);
        if (!option->selected())
            continue;
        state.append(option->value());
        if (!multiple())
            break;
    }
    return state;
}

size_t HTMLSelectElement::searchOptionsForValue(const String& value, size_t listIndexStart, size_t listIndexEnd) const
{
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& items = listItems();
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

    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& items = listItems();
    size_t itemsSize = items.size();
    if (!itemsSize)
        return;

    for (size_t i = 0; i < itemsSize; ++i) {
        if (!isHTMLOptionElement(items[i]))
            continue;
        toHTMLOptionElement(items[i])->setSelectedState(false);
    }

    if (!multiple()) {
        size_t foundIndex = searchOptionsForValue(state[0], 0, itemsSize);
        if (foundIndex != kNotFound)
            toHTMLOptionElement(items[foundIndex])->setSelectedState(true);
    } else {
        size_t startIndex = 0;
        for (size_t i = 0; i < state.valueSize(); ++i) {
            const String& value = state[i];
            size_t foundIndex = searchOptionsForValue(value, startIndex, itemsSize);
            if (foundIndex == kNotFound)
                foundIndex = searchOptionsForValue(value, 0, startIndex);
            if (foundIndex == kNotFound)
                continue;
            toHTMLOptionElement(items[foundIndex])->setSelectedState(true);
            startIndex = foundIndex + 1;
        }
    }

    setOptionsChangedOnRenderer();
    setNeedsValidityCheck();
}

void HTMLSelectElement::parseMultipleAttribute(const AtomicString& value)
{
    bool oldUsesMenuList = usesMenuList();
    m_multiple = !value.isNull();
    setNeedsValidityCheck();
    if (oldUsesMenuList != usesMenuList())
        lazyReattachIfAttached();
}

bool HTMLSelectElement::appendFormData(FormDataList& list, bool)
{
    const AtomicString& name = this->name();
    if (name.isEmpty())
        return false;

    bool successful = false;
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& items = listItems();

    for (unsigned i = 0; i < items.size(); ++i) {
        HTMLElement* element = items[i];
        if (isHTMLOptionElement(*element) && toHTMLOptionElement(*element).selected() && !toHTMLOptionElement(*element).isDisabledFormControl()) {
            list.appendData(name, toHTMLOptionElement(*element).value());
            successful = true;
        }
    }

    // It's possible that this is a menulist with multiple options and nothing
    // will be submitted (!successful). We won't send a unselected non-disabled
    // option as fallback. This behavior matches to other browsers.
    return successful;
}

void HTMLSelectElement::resetImpl()
{
    HTMLOptionElement* firstOption = 0;
    HTMLOptionElement* selectedOption = 0;

    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& items = listItems();
    for (unsigned i = 0; i < items.size(); ++i) {
        HTMLElement* element = items[i];
        if (!isHTMLOptionElement(*element))
            continue;

        if (items[i]->fastHasAttribute(selectedAttr)) {
            if (selectedOption && !m_multiple)
                selectedOption->setSelectedState(false);
            toHTMLOptionElement(element)->setSelectedState(true);
            selectedOption = toHTMLOptionElement(element);
        } else
            toHTMLOptionElement(element)->setSelectedState(false);

        if (!firstOption)
            firstOption = toHTMLOptionElement(element);
    }

    if (!selectedOption && firstOption && !m_multiple && m_size <= 1)
        firstOption->setSelectedState(true);

    setOptionsChangedOnRenderer();
    setNeedsStyleRecalc(SubtreeStyleChange);
    setNeedsValidityCheck();
}

#if !OS(WIN)
bool HTMLSelectElement::platformHandleKeydownEvent(KeyboardEvent* event)
{
    if (!RenderTheme::theme().popsMenuByArrowKeys())
        return false;

    if (!isSpatialNavigationEnabled(document().frame())) {
        if (event->keyIdentifier() == "Down" || event->keyIdentifier() == "Up") {
            focus();
            // Calling focus() may cause us to lose our renderer. Return true so
            // that our caller doesn't process the event further, but don't set
            // the event as handled.
            if (!renderer() || !renderer()->isMenuList() || isDisabledFormControl())
                return true;

            // Save the selection so it can be compared to the new selection
            // when dispatching change events during selectOption, which
            // gets called from RenderMenuList::valueChanged, which gets called
            // after the user makes a selection from the menu.
            saveLastSelection();
            if (RenderMenuList* menuList = toRenderMenuList(renderer()))
                menuList->showPopup();
            event->setDefaultHandled();
        }
        return true;
    }

    return false;
}
#endif

void HTMLSelectElement::menuListDefaultEventHandler(Event* event)
{
    RenderTheme& renderTheme = RenderTheme::theme();

    if (event->type() == EventTypeNames::keydown) {
        if (!renderer() || !event->isKeyboardEvent())
            return;

        if (platformHandleKeydownEvent(toKeyboardEvent(event)))
            return;

        // When using spatial navigation, we want to be able to navigate away
        // from the select element when the user hits any of the arrow keys,
        // instead of changing the selection.
        if (isSpatialNavigationEnabled(document().frame())) {
            if (!m_activeSelectionState)
                return;
        }

        const String& keyIdentifier = toKeyboardEvent(event)->keyIdentifier();
        bool handled = true;
        const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& listItems = this->listItems();
        int listIndex = optionToListIndex(selectedIndex());

        if (keyIdentifier == "Down" || keyIdentifier == "Right")
            listIndex = nextValidIndex(listIndex, SkipForwards, 1);
        else if (keyIdentifier == "Up" || keyIdentifier == "Left")
            listIndex = nextValidIndex(listIndex, SkipBackwards, 1);
        else if (keyIdentifier == "PageDown")
            listIndex = nextValidIndex(listIndex, SkipForwards, 3);
        else if (keyIdentifier == "PageUp")
            listIndex = nextValidIndex(listIndex, SkipBackwards, 3);
        else if (keyIdentifier == "Home")
            listIndex = nextValidIndex(-1, SkipForwards, 1);
        else if (keyIdentifier == "End")
            listIndex = nextValidIndex(listItems.size(), SkipBackwards, 1);
        else
            handled = false;

        if (handled && static_cast<size_t>(listIndex) < listItems.size())
            selectOption(listToOptionIndex(listIndex), DeselectOtherOptions | DispatchInputAndChangeEvent | UserDriven);

        if (handled)
            event->setDefaultHandled();
    }

    // Use key press event here since sending simulated mouse events
    // on key down blocks the proper sending of the key press event.
    if (event->type() == EventTypeNames::keypress) {
        if (!renderer() || !event->isKeyboardEvent())
            return;

        int keyCode = toKeyboardEvent(event)->keyCode();
        bool handled = false;

        if (keyCode == ' ' && isSpatialNavigationEnabled(document().frame())) {
            // Use space to toggle arrow key handling for selection change or spatial navigation.
            m_activeSelectionState = !m_activeSelectionState;
            event->setDefaultHandled();
            return;
        }

        if (renderTheme.popsMenuBySpaceOrReturn()) {
            if (keyCode == ' ' || keyCode == '\r') {
                focus();

                // Calling focus() may remove the renderer or change the
                // renderer type.
                if (!renderer() || !renderer()->isMenuList() || isDisabledFormControl())
                    return;

                // Save the selection so it can be compared to the new selection
                // when dispatching change events during selectOption, which
                // gets called from RenderMenuList::valueChanged, which gets called
                // after the user makes a selection from the menu.
                saveLastSelection();
                if (RenderMenuList* menuList = toRenderMenuList(renderer()))
                    menuList->showPopup();
                handled = true;
            }
        } else if (renderTheme.popsMenuByArrowKeys()) {
            if (keyCode == ' ') {
                focus();

                // Calling focus() may remove the renderer or change the
                // renderer type.
                if (!renderer() || !renderer()->isMenuList() || isDisabledFormControl())
                    return;

                // Save the selection so it can be compared to the new selection
                // when dispatching change events during selectOption, which
                // gets called from RenderMenuList::valueChanged, which gets called
                // after the user makes a selection from the menu.
                saveLastSelection();
                if (RenderMenuList* menuList = toRenderMenuList(renderer()))
                    menuList->showPopup();
                handled = true;
            } else if (keyCode == '\r') {
                if (form())
                    form()->submitImplicitly(event, false);
                dispatchInputAndChangeEventForMenuList();
                handled = true;
            }
        }

        if (handled)
            event->setDefaultHandled();
    }

    if (event->type() == EventTypeNames::mousedown && event->isMouseEvent() && toMouseEvent(event)->button() == LeftButton) {
        focus();
        if (renderer() && renderer()->isMenuList() && !isDisabledFormControl()) {
            if (RenderMenuList* menuList = toRenderMenuList(renderer())) {
                if (menuList->popupIsVisible())
                    menuList->hidePopup();
                else {
                    // Save the selection so it can be compared to the new
                    // selection when we call onChange during selectOption,
                    // which gets called from RenderMenuList::valueChanged,
                    // which gets called after the user makes a selection from
                    // the menu.
                    saveLastSelection();
                    menuList->showPopup();
                }
            }
        }
        event->setDefaultHandled();
    }

    if (event->type() == EventTypeNames::blur) {
        if (RenderMenuList* menuList = toRenderMenuList(renderer())) {
            if (menuList->popupIsVisible())
                menuList->hidePopup();
        }
    }
}

void HTMLSelectElement::updateSelectedState(int listIndex, bool multi, bool shift)
{
    ASSERT(listIndex >= 0);

    HTMLElement* clickedElement = listItems()[listIndex];
    ASSERT(clickedElement);
    if (isHTMLOptGroupElement(clickedElement))
        return;

    // Save the selection so it can be compared to the new selection when
    // dispatching change events during mouseup, or after autoscroll finishes.
    saveLastSelection();

    m_activeSelectionState = true;

    bool shiftSelect = m_multiple && shift;
    bool multiSelect = m_multiple && multi && !shift;

    if (isHTMLOptionElement(*clickedElement)) {
        // Keep track of whether an active selection (like during drag
        // selection), should select or deselect.
        if (toHTMLOptionElement(*clickedElement).selected() && multiSelect)
            m_activeSelectionState = false;
        if (!m_activeSelectionState)
            toHTMLOptionElement(*clickedElement).setSelectedState(false);
    }

    // If we're not in any special multiple selection mode, then deselect all
    // other items, excluding the clicked option. If no option was clicked, then
    // this will deselect all items in the list.
    if (!shiftSelect && !multiSelect)
        deselectItemsWithoutValidation(clickedElement);

    // If the anchor hasn't been set, and we're doing a single selection or a
    // shift selection, then initialize the anchor to the first selected index.
    if (m_activeSelectionAnchorIndex < 0 && !multiSelect)
        setActiveSelectionAnchorIndex(selectedIndex());

    // Set the selection state of the clicked option.
    if (isHTMLOptionElement(*clickedElement) && !toHTMLOptionElement(*clickedElement).isDisabledFormControl())
        toHTMLOptionElement(*clickedElement).setSelectedState(true);

    // If there was no selectedIndex() for the previous initialization, or If
    // we're doing a single selection, or a multiple selection (using cmd or
    // ctrl), then initialize the anchor index to the listIndex that just got
    // clicked.
    if (m_activeSelectionAnchorIndex < 0 || !shiftSelect)
        setActiveSelectionAnchorIndex(listIndex);

    setActiveSelectionEndIndex(listIndex);
    updateListBoxSelection(!multiSelect);
}

void HTMLSelectElement::listBoxDefaultEventHandler(Event* event)
{
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& listItems = this->listItems();
    if (event->type() == EventTypeNames::gesturetap && event->isGestureEvent()) {
        focus();
        // Calling focus() may cause us to lose our renderer or change the render type, in which case do not want to handle the event.
        if (!renderer() || !renderer()->isListBox())
            return;

        // Convert to coords relative to the list box if needed.
        GestureEvent& gestureEvent = toGestureEvent(*event);
        IntPoint localOffset = roundedIntPoint(renderer()->absoluteToLocal(gestureEvent.absoluteLocation(), UseTransforms));
        int listIndex = toRenderListBox(renderer())->listIndexAtOffset(toIntSize(localOffset));
        if (listIndex >= 0) {
            if (!isDisabledFormControl())
                updateSelectedState(listIndex, true, gestureEvent.shiftKey());
            event->setDefaultHandled();
        }
    } else if (event->type() == EventTypeNames::mousedown && event->isMouseEvent() && toMouseEvent(event)->button() == LeftButton) {
        focus();
        // Calling focus() may cause us to lose our renderer, in which case do not want to handle the event.
        if (!renderer() || !renderer()->isListBox() || isDisabledFormControl())
            return;

        // Convert to coords relative to the list box if needed.
        MouseEvent* mouseEvent = toMouseEvent(event);
        IntPoint localOffset = roundedIntPoint(renderer()->absoluteToLocal(mouseEvent->absoluteLocation(), UseTransforms));
        int listIndex = toRenderListBox(renderer())->listIndexAtOffset(toIntSize(localOffset));
        if (listIndex >= 0) {
            if (!isDisabledFormControl()) {
#if OS(MACOSX)
                updateSelectedState(listIndex, mouseEvent->metaKey(), mouseEvent->shiftKey());
#else
                updateSelectedState(listIndex, mouseEvent->ctrlKey(), mouseEvent->shiftKey());
#endif
            }
            if (LocalFrame* frame = document().frame())
                frame->eventHandler().setMouseDownMayStartAutoscroll();

            event->setDefaultHandled();
        }
    } else if (event->type() == EventTypeNames::mousemove && event->isMouseEvent() && !toRenderBox(renderer())->canBeScrolledAndHasScrollableArea()) {
        MouseEvent* mouseEvent = toMouseEvent(event);
        if (mouseEvent->button() != LeftButton || !mouseEvent->buttonDown())
            return;

        IntPoint localOffset = roundedIntPoint(renderer()->absoluteToLocal(mouseEvent->absoluteLocation(), UseTransforms));
        int listIndex = toRenderListBox(renderer())->listIndexAtOffset(toIntSize(localOffset));
        if (listIndex >= 0) {
            if (!isDisabledFormControl()) {
                if (m_multiple) {
                    // Only extend selection if there is something selected.
                    if (m_activeSelectionAnchorIndex < 0)
                        return;

                    setActiveSelectionEndIndex(listIndex);
                    updateListBoxSelection(false);
                } else {
                    setActiveSelectionAnchorIndex(listIndex);
                    setActiveSelectionEndIndex(listIndex);
                    updateListBoxSelection(true);
                }
            }
        }
    } else if (event->type() == EventTypeNames::mouseup && event->isMouseEvent() && toMouseEvent(event)->button() == LeftButton && renderer() && !toRenderBox(renderer())->autoscrollInProgress()) {
        // We didn't start this click/drag on any options.
        if (m_lastOnChangeSelection.isEmpty())
            return;
        listBoxOnChange();
    } else if (event->type() == EventTypeNames::keydown) {
        if (!event->isKeyboardEvent())
            return;
        const String& keyIdentifier = toKeyboardEvent(event)->keyIdentifier();

        bool handled = false;
        int endIndex = 0;
        if (m_activeSelectionEndIndex < 0) {
            // Initialize the end index
            if (keyIdentifier == "Down" || keyIdentifier == "PageDown") {
                int startIndex = lastSelectedListIndex();
                handled = true;
                if (keyIdentifier == "Down")
                    endIndex = nextSelectableListIndex(startIndex);
                else
                    endIndex = nextSelectableListIndexPageAway(startIndex, SkipForwards);
            } else if (keyIdentifier == "Up" || keyIdentifier == "PageUp") {
                int startIndex = optionToListIndex(selectedIndex());
                handled = true;
                if (keyIdentifier == "Up")
                    endIndex = previousSelectableListIndex(startIndex);
                else
                    endIndex = nextSelectableListIndexPageAway(startIndex, SkipBackwards);
            }
        } else {
            // Set the end index based on the current end index.
            if (keyIdentifier == "Down") {
                endIndex = nextSelectableListIndex(m_activeSelectionEndIndex);
                handled = true;
            } else if (keyIdentifier == "Up") {
                endIndex = previousSelectableListIndex(m_activeSelectionEndIndex);
                handled = true;
            } else if (keyIdentifier == "PageDown") {
                endIndex = nextSelectableListIndexPageAway(m_activeSelectionEndIndex, SkipForwards);
                handled = true;
            } else if (keyIdentifier == "PageUp") {
                endIndex = nextSelectableListIndexPageAway(m_activeSelectionEndIndex, SkipBackwards);
                handled = true;
            }
        }
        if (keyIdentifier == "Home") {
            endIndex = firstSelectableListIndex();
            handled = true;
        } else if (keyIdentifier == "End") {
            endIndex = lastSelectableListIndex();
            handled = true;
        }

        if (isSpatialNavigationEnabled(document().frame()))
            // Check if the selection moves to the boundary.
            if (keyIdentifier == "Left" || keyIdentifier == "Right" || ((keyIdentifier == "Down" || keyIdentifier == "Up") && endIndex == m_activeSelectionEndIndex))
                return;

        if (endIndex >= 0 && handled) {
            // Save the selection so it can be compared to the new selection
            // when dispatching change events immediately after making the new
            // selection.
            saveLastSelection();

            ASSERT_UNUSED(listItems, !listItems.size() || static_cast<size_t>(endIndex) < listItems.size());
            setActiveSelectionEndIndex(endIndex);

            bool selectNewItem = !m_multiple || toKeyboardEvent(event)->shiftKey() || !isSpatialNavigationEnabled(document().frame());
            if (selectNewItem)
                m_activeSelectionState = true;
            // If the anchor is unitialized, or if we're going to deselect all
            // other options, then set the anchor index equal to the end index.
            bool deselectOthers = !m_multiple || (!toKeyboardEvent(event)->shiftKey() && selectNewItem);
            if (m_activeSelectionAnchorIndex < 0 || deselectOthers) {
                if (deselectOthers)
                    deselectItemsWithoutValidation();
                setActiveSelectionAnchorIndex(m_activeSelectionEndIndex);
            }

            toRenderListBox(renderer())->scrollToRevealElementAtListIndex(endIndex);
            if (selectNewItem) {
                updateListBoxSelection(deselectOthers);
                listBoxOnChange();
            } else
                scrollToSelection();

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
            updateSelectedState(listToOptionIndex(m_activeSelectionEndIndex), true /*multi*/, false /*shift*/);
            listBoxOnChange();
            event->setDefaultHandled();
        }
    }
}

void HTMLSelectElement::defaultEventHandler(Event* event)
{
    if (!renderer())
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

int HTMLSelectElement::lastSelectedListIndex() const
{
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& items = listItems();
    for (size_t i = items.size(); i;) {
        HTMLElement* element = items[--i];
        if (isHTMLOptionElement(*element) && toHTMLOptionElement(element)->selected())
            return i;
    }
    return -1;
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
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& items = listItems();

    HTMLElement* element = items[index];
    if (!isHTMLOptionElement(*element) || toHTMLOptionElement(element)->isDisabledFormControl())
        return String();
    return toHTMLOptionElement(element)->textIndentedToRespectGroupLabel();
}

void HTMLSelectElement::typeAheadFind(KeyboardEvent* event)
{
    int index = m_typeAhead.handleEvent(event, TypeAhead::MatchPrefix | TypeAhead::CycleFirstChar);
    if (index < 0)
        return;
    selectOption(listToOptionIndex(index), DeselectOtherOptions | DispatchInputAndChangeEvent | UserDriven);
    if (!usesMenuList())
        listBoxOnChange();
}

Node::InsertionNotificationRequest HTMLSelectElement::insertedInto(ContainerNode* insertionPoint)
{
    // When the element is created during document parsing, it won't have any
    // items yet - but for innerHTML and related methods, this method is called
    // after the whole subtree is constructed.
    recalcListItems();
    HTMLFormControlElementWithState::insertedInto(insertionPoint);
    return InsertionDone;
}

void HTMLSelectElement::accessKeySetSelectedIndex(int index)
{
    // First bring into focus the list box.
    if (!focused())
        accessKeyAction(false);

    // If this index is already selected, unselect. otherwise update the selected index.
    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& items = listItems();
    int listIndex = optionToListIndex(index);
    if (listIndex >= 0) {
        HTMLElement* element = items[listIndex];
        if (isHTMLOptionElement(*element)) {
            if (toHTMLOptionElement(*element).selected())
                toHTMLOptionElement(*element).setSelectedState(false);
            else
                selectOption(index, DispatchInputAndChangeEvent | UserDriven);
        }
    }

    if (usesMenuList())
        dispatchInputAndChangeEventForMenuList();
    else
        listBoxOnChange();

    scrollToSelection();
}

unsigned HTMLSelectElement::length() const
{
    unsigned options = 0;

    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& items = listItems();
    for (unsigned i = 0; i < items.size(); ++i) {
        if (isHTMLOptionElement(*items[i]))
            ++options;
    }

    return options;
}

void HTMLSelectElement::finishParsingChildren()
{
    HTMLFormControlElementWithState::finishParsingChildren();
    updateListItemSelectedStates();
}

bool HTMLSelectElement::anonymousIndexedSetter(unsigned index, PassRefPtrWillBeRawPtr<HTMLOptionElement> value, ExceptionState& exceptionState)
{
    if (!value) { // undefined or null
        remove(index);
        return true;
    }
    setOption(index, value.get(), exceptionState);
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

void HTMLSelectElement::updateListOnRenderer()
{
    setOptionsChangedOnRenderer();
}

void HTMLSelectElement::trace(Visitor* visitor)
{
#if ENABLE(OILPAN)
    visitor->trace(m_listItems);
#endif
    HTMLFormControlElementWithState::trace(visitor);
}

} // namespace
