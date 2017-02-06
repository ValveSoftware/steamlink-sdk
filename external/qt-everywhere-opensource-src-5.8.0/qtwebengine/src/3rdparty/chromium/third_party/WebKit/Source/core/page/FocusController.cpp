/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nuanti Ltd.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/page/FocusController.h"

#include "core/HTMLNames.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/Range.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/dom/shadow/SlotScopedTraversal.h"
#include "core/editing/EditingUtilities.h" // For firstPositionInOrBeforeNode
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/events/Event.h"
#include "core/frame/FrameClient.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/RemoteFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLAreaElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/html/HTMLShadowElement.h"
#include "core/html/HTMLSlotElement.h"
#include "core/html/HTMLTextFormControlElement.h"
#include "core/input/EventHandler.h"
#include "core/page/ChromeClient.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/layout/HitTestResult.h"
#include "core/page/SpatialNavigation.h"
#include <limits>

namespace blink {

using namespace HTMLNames;

namespace {

inline bool isShadowInsertionPointFocusScopeOwner(Element& element)
{
    return isActiveShadowInsertionPoint(element) && toHTMLShadowElement(element).olderShadowRoot();
}

class ScopedFocusNavigation {
    STACK_ALLOCATED();
public:
    Element* currentElement() const;
    void setCurrentElement(Element*);
    void moveToNext();
    void moveToPrevious();
    void moveToFirst();
    void moveToLast();
    Element* owner() const;
    static ScopedFocusNavigation createFor(const Element&);
    static ScopedFocusNavigation createForDocument(Document&);
    static ScopedFocusNavigation ownedByNonFocusableFocusScopeOwner(Element&);
    static ScopedFocusNavigation ownedByShadowHost(const Element&);
    static ScopedFocusNavigation ownedByShadowInsertionPoint(HTMLShadowElement&);
    static ScopedFocusNavigation ownedByHTMLSlotElement(const HTMLSlotElement&);
    static ScopedFocusNavigation ownedByIFrame(const HTMLFrameOwnerElement&);
    static HTMLSlotElement* findFallbackScopeOwnerSlot(const Element&);
    static bool isSlotFallbackScoped(const Element&);
    static bool isSlotFallbackScopedForThisSlot(const HTMLSlotElement&, const Element&);

private:
    ScopedFocusNavigation(TreeScope&, const Element*);
    ScopedFocusNavigation(HTMLSlotElement&, const Element*);
    Member<ContainerNode> m_rootNode;
    Member<HTMLSlotElement> m_rootSlot;
    Member<Element> m_current;
    bool m_slotFallbackTraversal;
};

ScopedFocusNavigation::ScopedFocusNavigation(TreeScope& treeScope, const Element* current)
    : m_rootNode(treeScope.rootNode())
    , m_rootSlot(nullptr)
    , m_current(const_cast<Element*>(current))
{
}

ScopedFocusNavigation::ScopedFocusNavigation(HTMLSlotElement& slot, const Element* current)
    : m_rootNode(nullptr)
    , m_rootSlot(&slot)
    , m_current(const_cast<Element*>(current))
    , m_slotFallbackTraversal(slot.assignedNodes().isEmpty())
{
}

Element* ScopedFocusNavigation::currentElement() const
{
    return m_current;
}

void ScopedFocusNavigation::setCurrentElement(Element* element)
{
    m_current = element;
}

void ScopedFocusNavigation::moveToNext()
{
    DCHECK(m_current);
    if (m_rootSlot) {
        if (m_slotFallbackTraversal) {
            m_current = ElementTraversal::next(*m_current, m_rootSlot);
            while (m_current && !ScopedFocusNavigation::isSlotFallbackScopedForThisSlot(*m_rootSlot, *m_current))
                m_current = ElementTraversal::next(*m_current, m_rootSlot);
        } else {
            m_current = SlotScopedTraversal::next(*m_current);
        }
    } else {
        m_current = ElementTraversal::next(*m_current);
        while (m_current && (SlotScopedTraversal::isSlotScoped(*m_current) || ScopedFocusNavigation::isSlotFallbackScoped(*m_current)))
            m_current = ElementTraversal::next(*m_current);
    }
}

void ScopedFocusNavigation::moveToPrevious()
{
    DCHECK(m_current);
    if (m_rootSlot) {
        if (m_slotFallbackTraversal) {
            m_current = ElementTraversal::previous(*m_current, m_rootSlot);
            if (m_current == m_rootSlot)
                m_current = nullptr;
            while (m_current && !ScopedFocusNavigation::isSlotFallbackScopedForThisSlot(*m_rootSlot, *m_current))
                m_current = ElementTraversal::previous(*m_current);
        } else {
            m_current = SlotScopedTraversal::previous(*m_current);
        }
    } else {
        m_current = ElementTraversal::previous(*m_current);
        while (m_current && (SlotScopedTraversal::isSlotScoped(*m_current) || ScopedFocusNavigation::isSlotFallbackScoped(*m_current)))
            m_current = ElementTraversal::previous(*m_current);
    }
}

void ScopedFocusNavigation::moveToFirst()
{
    if (m_rootSlot) {
        if (!m_slotFallbackTraversal) {
            HeapVector<Member<Node>> assignedNodes = m_rootSlot->assignedNodes();
            for (auto assignedNode : assignedNodes) {
                if (assignedNode->isElementNode()) {
                    m_current = toElement(assignedNode);
                    break;
                }
            }
        } else {
            Element* first = ElementTraversal::firstChild(*m_rootSlot);
            while (first && !ScopedFocusNavigation::isSlotFallbackScopedForThisSlot(*m_rootSlot, *first))
                first = ElementTraversal::next(*first, m_rootSlot);
            m_current = first;
        }
    } else {
        Element* first = m_rootNode->isElementNode() ? &toElement(*m_rootNode) : ElementTraversal::next(*m_rootNode);
        while (first && (SlotScopedTraversal::isSlotScoped(*first) || ScopedFocusNavigation::isSlotFallbackScoped(*first)))
            first = ElementTraversal::next(*first, m_rootNode);
        m_current = first;
    }

}

void ScopedFocusNavigation::moveToLast()
{
    if (m_rootSlot) {
        if (!m_slotFallbackTraversal) {
            HeapVector<Member<Node>> assignedNodes = m_rootSlot->assignedNodes();
            for (auto assignedNode = assignedNodes.rbegin(); assignedNode != assignedNodes.rend(); ++assignedNode) {
                if ((*assignedNode)->isElementNode()) {
                    m_current = ElementTraversal::lastWithinOrSelf(*toElement(*assignedNode));
                    break;
                }
            }
        } else {
            Element* last = ElementTraversal::lastWithin(*m_rootSlot);
            while (last && !ScopedFocusNavigation::isSlotFallbackScopedForThisSlot(*m_rootSlot, *last))
                last = ElementTraversal::previous(*last, m_rootSlot);
            m_current = last;
        }
    } else {
        Element* last = ElementTraversal::lastWithin(*m_rootNode);
        while (last && (SlotScopedTraversal::isSlotScoped(*last) || ScopedFocusNavigation::isSlotFallbackScoped(*last)))
            last = ElementTraversal::previous(*last, m_rootNode);
        m_current = last;
    }
}

Element* ScopedFocusNavigation::owner() const
{
    if (m_rootSlot)
        return m_rootSlot;
    DCHECK(m_rootNode);
    if (m_rootNode->isShadowRoot()) {
        ShadowRoot& shadowRoot = toShadowRoot(*m_rootNode);
        return shadowRoot.isYoungest() ? &shadowRoot.host() : shadowRoot.shadowInsertionPointOfYoungerShadowRoot();
    }
    // FIXME: Figure out the right thing for OOPI here.
    if (Frame* frame = m_rootNode->document().frame())
        return frame->deprecatedLocalOwner();
    return nullptr;
}

ScopedFocusNavigation ScopedFocusNavigation::createFor(const Element& current)
{
    if (HTMLSlotElement* slot = SlotScopedTraversal::findScopeOwnerSlot(current))
        return ScopedFocusNavigation(*slot, &current);
    if (HTMLSlotElement* slot = ScopedFocusNavigation::findFallbackScopeOwnerSlot(current))
        return ScopedFocusNavigation(*slot, &current);
    return ScopedFocusNavigation(current.containingTreeScope(), &current);
}

ScopedFocusNavigation ScopedFocusNavigation::createForDocument(Document& document)
{
    return ScopedFocusNavigation(document, nullptr);
}

ScopedFocusNavigation ScopedFocusNavigation::ownedByNonFocusableFocusScopeOwner(Element& element)
{
    if (isShadowHost(element))
        return ScopedFocusNavigation::ownedByShadowHost(element);
    if (isShadowInsertionPointFocusScopeOwner(element))
        return ScopedFocusNavigation::ownedByShadowInsertionPoint(toHTMLShadowElement(element));
    DCHECK(isHTMLSlotElement(element));
    return ScopedFocusNavigation::ownedByHTMLSlotElement(toHTMLSlotElement(element));
}

ScopedFocusNavigation ScopedFocusNavigation::ownedByShadowHost(const Element& element)
{
    DCHECK(isShadowHost(element));
    return ScopedFocusNavigation(*&element.shadow()->youngestShadowRoot(), nullptr);
}

ScopedFocusNavigation ScopedFocusNavigation::ownedByIFrame(const HTMLFrameOwnerElement& frame)
{
    DCHECK(frame.contentFrame());
    DCHECK(frame.contentFrame()->isLocalFrame());
    toLocalFrame(frame.contentFrame())->document()->updateDistribution();
    return ScopedFocusNavigation(*toLocalFrame(frame.contentFrame())->document(), nullptr);
}

ScopedFocusNavigation ScopedFocusNavigation::ownedByShadowInsertionPoint(HTMLShadowElement& shadowInsertionPoint)
{
    DCHECK(isShadowInsertionPointFocusScopeOwner(shadowInsertionPoint));
    return ScopedFocusNavigation(*shadowInsertionPoint.olderShadowRoot(), nullptr);
}

ScopedFocusNavigation ScopedFocusNavigation::ownedByHTMLSlotElement(const HTMLSlotElement& element)
{
    return ScopedFocusNavigation(const_cast<HTMLSlotElement&>(element), nullptr);
}

HTMLSlotElement* ScopedFocusNavigation::findFallbackScopeOwnerSlot(const Element& element)
{
    Element* parent = const_cast<Element*>(element.parentElement());
    while (parent) {
        if (isHTMLSlotElement(parent))
            return toHTMLSlotElement(parent)->assignedNodes().isEmpty() ? toHTMLSlotElement(parent) : nullptr;
        parent = parent->parentElement();
    }
    return nullptr;
}

bool ScopedFocusNavigation::isSlotFallbackScoped(const Element& element)
{
    return ScopedFocusNavigation::findFallbackScopeOwnerSlot(element);
}

bool ScopedFocusNavigation::isSlotFallbackScopedForThisSlot(const HTMLSlotElement& slot, const Element& current)
{
    Element* parent = current.parentElement();
    while (parent) {
        if (isHTMLSlotElement(parent) && toHTMLSlotElement(parent)->assignedNodes().isEmpty())
            return !SlotScopedTraversal::isSlotScoped(current) && toHTMLSlotElement(parent) == slot;
        parent = parent->parentElement();
    }
    return false;
}

inline void dispatchBlurEvent(const Document& document, Element& focusedElement)
{
    focusedElement.dispatchBlurEvent(nullptr, WebFocusTypePage);
    if (focusedElement == document.focusedElement()) {
        focusedElement.dispatchFocusOutEvent(EventTypeNames::focusout, nullptr);
        if (focusedElement == document.focusedElement())
            focusedElement.dispatchFocusOutEvent(EventTypeNames::DOMFocusOut, nullptr);
    }
}

inline void dispatchFocusEvent(const Document& document, Element& focusedElement)
{
    focusedElement.dispatchFocusEvent(0, WebFocusTypePage);
    if (focusedElement == document.focusedElement()) {
        focusedElement.dispatchFocusInEvent(EventTypeNames::focusin, nullptr, WebFocusTypePage);
        if (focusedElement == document.focusedElement())
            focusedElement.dispatchFocusInEvent(EventTypeNames::DOMFocusIn, nullptr, WebFocusTypePage);
    }
}

inline void dispatchEventsOnWindowAndFocusedElement(Document* document, bool focused)
{
    DCHECK(document);
    // If we have a focused element we should dispatch blur on it before we blur the window.
    // If we have a focused element we should dispatch focus on it after we focus the window.
    // https://bugs.webkit.org/show_bug.cgi?id=27105

    // Do not fire events while modal dialogs are up.  See https://bugs.webkit.org/show_bug.cgi?id=33962
    if (Page* page = document->page()) {
        if (page->defersLoading())
            return;
    }

    if (!focused && document->focusedElement()) {
        Element* focusedElement = document->focusedElement();
        focusedElement->setFocus(false);
        dispatchBlurEvent(*document, *focusedElement);
    }

    if (LocalDOMWindow* window = document->domWindow())
        window->dispatchEvent(Event::create(focused ? EventTypeNames::focus : EventTypeNames::blur));
    if (focused && document->focusedElement()) {
        Element* focusedElement(document->focusedElement());
        focusedElement->setFocus(true);
        dispatchFocusEvent(*document, *focusedElement);
    }
}

inline bool hasCustomFocusLogic(const Element& element)
{
    return element.isHTMLElement() && toHTMLElement(element).hasCustomFocusLogic();
}

inline bool isShadowHostWithoutCustomFocusLogic(const Element& element)
{
    return isShadowHost(element) && !hasCustomFocusLogic(element);
}

inline bool isNonKeyboardFocusableShadowHost(const Element& element)
{
    return isShadowHostWithoutCustomFocusLogic(element) && !(element.shadowRootIfV1() ? element.isFocusable() : element.isKeyboardFocusable());
}

inline bool isKeyboardFocusableShadowHost(const Element& element)
{
    return isShadowHostWithoutCustomFocusLogic(element) && element.isKeyboardFocusable();
}

inline bool isNonFocusableFocusScopeOwner(Element& element)
{
    return isNonKeyboardFocusableShadowHost(element) || isShadowInsertionPointFocusScopeOwner(element) || isHTMLSlotElement(element);
}

inline bool isShadowHostDelegatesFocus(const Element& element)
{
    return element.authorShadowRoot() && element.authorShadowRoot()->delegatesFocus();
}

inline int adjustedTabIndex(Element& element)
{
    return (isNonKeyboardFocusableShadowHost(element) || isShadowInsertionPointFocusScopeOwner(element)) ? 0 : element.tabIndex();
}

inline bool shouldVisit(Element& element)
{
    return element.isKeyboardFocusable() || isNonFocusableFocusScopeOwner(element);
}

Element* findElementWithExactTabIndex(ScopedFocusNavigation& scope, int tabIndex, WebFocusType type)
{
    // Search is inclusive of start
    for (; scope.currentElement(); type == WebFocusTypeForward ? scope.moveToNext() : scope.moveToPrevious()) {
        Element* current = scope.currentElement();
        if (shouldVisit(*current) && adjustedTabIndex(*current) == tabIndex)
            return current;
    }
    return nullptr;
}

Element* nextElementWithGreaterTabIndex(ScopedFocusNavigation& scope, int tabIndex)
{
    // Search is inclusive of start
    int winningTabIndex = std::numeric_limits<short>::max() + 1;
    Element* winner = nullptr;
    for (; scope.currentElement(); scope.moveToNext()) {
        Element* current = scope.currentElement();
        int currentTabIndex = adjustedTabIndex(*current);
        if (shouldVisit(*current) && currentTabIndex > tabIndex && currentTabIndex < winningTabIndex) {
            winner = current;
            winningTabIndex = currentTabIndex;
        }
    }
    return winner;
}

Element* previousElementWithLowerTabIndex(ScopedFocusNavigation& scope, int tabIndex)
{
    // Search is inclusive of start
    int winningTabIndex = 0;
    Element* winner = nullptr;
    for (; scope.currentElement(); scope.moveToPrevious()) {
        Element* current = scope.currentElement();
        int currentTabIndex = adjustedTabIndex(*current);
        if (shouldVisit(*current) && currentTabIndex < tabIndex && currentTabIndex > winningTabIndex) {
            winner = current;
            winningTabIndex = currentTabIndex;
        }
    }
    return winner;
}

Element* nextFocusableElement(ScopedFocusNavigation& scope)
{
    Element* current = scope.currentElement();
    if (current) {
        int tabIndex = adjustedTabIndex(*current);
        // If an element is excluded from the normal tabbing cycle, the next focusable element is determined by tree order
        if (tabIndex < 0) {
            for (scope.moveToNext(); scope.currentElement(); scope.moveToNext()) {
                current = scope.currentElement();
                if (shouldVisit(*current) && adjustedTabIndex(*current) >= 0)
                    return current;
            }
        } else {
            // First try to find an element with the same tabindex as start that comes after start in the scope.
            scope.moveToNext();
            if (Element* winner = findElementWithExactTabIndex(scope, tabIndex, WebFocusTypeForward))
                return winner;
        }
        if (!tabIndex) {
            // We've reached the last element in the document with a tabindex of 0. This is the end of the tabbing order.
            return nullptr;
        }
    }

    // Look for the first element in the scope that:
    // 1) has the lowest tabindex that is higher than start's tabindex (or 0, if start is null), and
    // 2) comes first in the scope, if there's a tie.
    scope.moveToFirst();
    if (Element* winner = nextElementWithGreaterTabIndex(scope, current ? adjustedTabIndex(*current) : 0)) {
        return winner;
    }

    // There are no elements with a tabindex greater than start's tabindex,
    // so find the first element with a tabindex of 0.
    scope.moveToFirst();
    return findElementWithExactTabIndex(scope, 0, WebFocusTypeForward);
}

Element* previousFocusableElement(ScopedFocusNavigation& scope)
{
    // First try to find the last element in the scope that comes before start and has the same tabindex as start.
    // If start is null, find the last element in the scope with a tabindex of 0.
    int tabIndex;
    Element* current = scope.currentElement();
    if (current) {
        scope.moveToPrevious();
        tabIndex = adjustedTabIndex(*current);
    } else {
        scope.moveToLast();
        tabIndex = 0;
    }

    // However, if an element is excluded from the normal tabbing cycle, the previous focusable element is determined by tree order
    if (tabIndex < 0) {
        for (; scope.currentElement(); scope.moveToPrevious()) {
            current = scope.currentElement();
            if (shouldVisit(*current) && adjustedTabIndex(*current) >= 0)
                return current;
        }
    } else {
        if (Element* winner = findElementWithExactTabIndex(scope, tabIndex, WebFocusTypeBackward))
            return winner;
    }

    // There are no elements before start with the same tabindex as start, so look for an element that:
    // 1) has the highest non-zero tabindex (that is less than start's tabindex), and
    // 2) comes last in the scope, if there's a tie.
    tabIndex = (current && tabIndex) ? tabIndex : std::numeric_limits<short>::max();
    scope.moveToLast();
    return previousElementWithLowerTabIndex(scope, tabIndex);
}

// Searches through the given tree scope, starting from start element, for the next/previous
// selectable element that comes after/before start element.
// The order followed is as specified in the HTML spec[1], which is elements with tab indexes
// first (from lowest to highest), and then elements without tab indexes (in document order).
// The search algorithm also conforms the Shadow DOM spec[2], which inserts sequence in a shadow
// tree into its host.
//
// @param start The element from which to start searching. The element after this will be focused.
//        May be null.
// @return The focus element that comes after/before start element.
//
// [1] https://html.spec.whatwg.org/multipage/interaction.html#sequential-focus-navigation
// [2] https://w3c.github.io/webcomponents/spec/shadow/#focus-navigation
inline Element* findFocusableElementInternal(WebFocusType type, ScopedFocusNavigation& scope)
{
    Element* found = (type == WebFocusTypeForward) ? nextFocusableElement(scope) : previousFocusableElement(scope);
    return found;
}

Element* findFocusableElementRecursivelyForward(ScopedFocusNavigation& scope)
{
    // Starting element is exclusive.
    Element* found = findFocusableElementInternal(WebFocusTypeForward, scope);
    while (found) {
        if (isShadowHostDelegatesFocus(*found)) {
            // If tabindex is positive, find focusable element inside its shadow tree.
            if (found->tabIndex() >= 0 && isShadowHostWithoutCustomFocusLogic(*found)) {
                ScopedFocusNavigation innerScope = ScopedFocusNavigation::ownedByShadowHost(*found);
                if (Element* foundInInnerFocusScope = findFocusableElementRecursivelyForward(innerScope))
                    return foundInInnerFocusScope;
            }
            // Skip to the next element in the same scope.
            found = findFocusableElementInternal(WebFocusTypeForward, scope);
            continue;
        }
        if (!isNonFocusableFocusScopeOwner(*found))
            return found;

        // Now |found| is on a non focusable scope owner (either shadow host or <shadow> or slot)
        // Find inside the inward scope and return it if found. Otherwise continue searching in the same
        // scope.
        ScopedFocusNavigation innerScope = ScopedFocusNavigation::ownedByNonFocusableFocusScopeOwner(*found);
        if (Element* foundInInnerFocusScope = findFocusableElementRecursivelyForward(innerScope))
            return foundInInnerFocusScope;

        scope.setCurrentElement(found);
        found = findFocusableElementInternal(WebFocusTypeForward, scope);
    }
    return nullptr;
}

Element* findFocusableElementRecursivelyBackward(ScopedFocusNavigation& scope)
{
    // Starting element is exclusive.
    Element* found = findFocusableElementInternal(WebFocusTypeBackward, scope);

    while (found) {
        // Now |found| is on a focusable shadow host.
        // Find inside shadow backwards. If any focusable element is found, return it, otherwise return
        // the host itself.
        if (isKeyboardFocusableShadowHost(*found)) {
            ScopedFocusNavigation innerScope = ScopedFocusNavigation::ownedByShadowHost(*found);
            Element* foundInInnerFocusScope = findFocusableElementRecursivelyBackward(innerScope);
            if (foundInInnerFocusScope)
                return foundInInnerFocusScope;
            if (isShadowHostDelegatesFocus(*found)) {
                found = findFocusableElementInternal(WebFocusTypeBackward, scope);
                continue;
            }
            return found;
        }

        // If delegatesFocus is true and tabindex is negative, skip the whole shadow tree under the
        // shadow host.
        if (isShadowHostDelegatesFocus(*found) && found->tabIndex() < 0) {
            found = findFocusableElementInternal(WebFocusTypeBackward, scope);
            continue;
        }

        // Now |found| is on a non focusable scope owner (either shadow host or <shadow> or slot).
        // Find focusable element in descendant scope. If not found, find next focusable element within the
        // current scope.
        if (isNonFocusableFocusScopeOwner(*found)) {
            ScopedFocusNavigation innerScope = ScopedFocusNavigation::ownedByNonFocusableFocusScopeOwner(*found);
            Element* foundInInnerFocusScope = findFocusableElementRecursivelyBackward(innerScope);

            if (foundInInnerFocusScope)
                return foundInInnerFocusScope;
            found = findFocusableElementInternal(WebFocusTypeBackward, scope);
            continue;
        }
        if (!isShadowHostDelegatesFocus(*found))
            return found;

        scope.setCurrentElement(found);
        found = findFocusableElementInternal(WebFocusTypeBackward, scope);
    }
    return nullptr;
}

Element* findFocusableElementRecursively(WebFocusType type, ScopedFocusNavigation& scope)
{
    return (type == WebFocusTypeForward) ?
        findFocusableElementRecursivelyForward(scope) :
        findFocusableElementRecursivelyBackward(scope);
}

Element* findFocusableElementDescendingDownIntoFrameDocument(WebFocusType type, Element* element)
{
    // The element we found might be a HTMLFrameOwnerElement, so descend down the tree until we find either:
    // 1) a focusable element, or
    // 2) the deepest-nested HTMLFrameOwnerElement.
    while (element && element->isFrameOwnerElement()) {
        HTMLFrameOwnerElement& owner = toHTMLFrameOwnerElement(*element);
        if (!owner.contentFrame() || !owner.contentFrame()->isLocalFrame())
            break;
        toLocalFrame(owner.contentFrame())->document()->updateStyleAndLayoutIgnorePendingStylesheets();
        ScopedFocusNavigation scope = ScopedFocusNavigation::ownedByIFrame(owner);
        Element* foundElement = findFocusableElementRecursively(type, scope);
        if (!foundElement)
            break;
        DCHECK_NE(element, foundElement);
        element = foundElement;
    }
    return element;
}

Element* findFocusableElementAcrossFocusScopesForward(ScopedFocusNavigation& scope)
{
    Element* current = scope.currentElement();
    Element* found;
    if (current && isShadowHostWithoutCustomFocusLogic(*current)) {
        ScopedFocusNavigation innerScope = ScopedFocusNavigation::ownedByShadowHost(*current);
        Element* foundInInnerFocusScope = findFocusableElementRecursivelyForward(innerScope);
        found = foundInInnerFocusScope ? foundInInnerFocusScope : findFocusableElementRecursivelyForward(scope);
    } else {
        found = findFocusableElementRecursivelyForward(scope);
    }

    // If there's no focusable element to advance to, move up the focus scopes until we find one.
    ScopedFocusNavigation currentScope = scope;
    while (!found) {
        Element* owner = currentScope.owner();
        if (!owner)
            break;
        currentScope = ScopedFocusNavigation::createFor(*owner);
        found = findFocusableElementRecursivelyForward(currentScope);
    }
    return findFocusableElementDescendingDownIntoFrameDocument(WebFocusTypeForward, found);
}

Element* findFocusableElementAcrossFocusScopesBackward(ScopedFocusNavigation& scope)
{
    Element* found = findFocusableElementRecursivelyBackward(scope);

    // If there's no focusable element to advance to, move up the focus scopes until we find one.
    ScopedFocusNavigation currentScope = scope;
    while (!found) {
        Element* owner = currentScope.owner();
        if (!owner)
            break;
        currentScope = ScopedFocusNavigation::createFor(*owner);
        if (isKeyboardFocusableShadowHost(*owner) && !isShadowHostDelegatesFocus(*owner)) {
            found = owner;
            break;
        }
        found = findFocusableElementRecursivelyBackward(currentScope);
    }
    return findFocusableElementDescendingDownIntoFrameDocument(WebFocusTypeBackward, found);
}

Element* findFocusableElementAcrossFocusScopes(WebFocusType type, ScopedFocusNavigation& scope)
{
    return (type == WebFocusTypeForward) ?
        findFocusableElementAcrossFocusScopesForward(scope) :
        findFocusableElementAcrossFocusScopesBackward(scope);
}

inline Element* adjustToElement(Node* node, WebFocusType type)
{
    DCHECK(type == WebFocusTypeForward || type == WebFocusTypeBackward);
    if (!node)
        return nullptr;
    if (node->isElementNode())
        return toElement(node);
    // The returned element is used as an *exclusive* start element. Thus, we should return the result of ElementTraversal::previous(*node),
    // instead of ElementTraversal::next(*node), if type == WebFocusTypeForward, and vice-versa.
    // The caller will call ElementTraversal::{next/previous} for the returned value and get the {next|previous} element of the |node|.

    // TODO(yuzus) Use ScopedFocusNavigation traversal here.
    return (type == WebFocusTypeForward) ? ElementTraversal::previous(*node) : ElementTraversal::next(*node);
}

} // anonymous namespace

FocusController::FocusController(Page* page)
    : m_page(page)
    , m_isActive(false)
    , m_isFocused(false)
    , m_isChangingFocusedFrame(false)
{
}

FocusController* FocusController::create(Page* page)
{
    return new FocusController(page);
}

void FocusController::setFocusedFrame(Frame* frame, bool notifyEmbedder)
{
    DCHECK(!frame || frame->page() == m_page);
    if (m_focusedFrame == frame || (m_isChangingFocusedFrame && frame))
        return;

    m_isChangingFocusedFrame = true;

    LocalFrame* oldFrame = (m_focusedFrame && m_focusedFrame->isLocalFrame()) ? toLocalFrame(m_focusedFrame.get()) : nullptr;

    LocalFrame* newFrame = (frame && frame->isLocalFrame()) ? toLocalFrame(frame) : nullptr;

    m_focusedFrame = frame;

    // Now that the frame is updated, fire events and update the selection focused states of both frames.
    if (oldFrame && oldFrame->view()) {
        oldFrame->selection().setFocused(false);
        oldFrame->domWindow()->dispatchEvent(Event::create(EventTypeNames::blur));
    }

    if (newFrame && newFrame->view() && isFocused()) {
        newFrame->selection().setFocused(true);
        newFrame->domWindow()->dispatchEvent(Event::create(EventTypeNames::focus));
    }

    m_isChangingFocusedFrame = false;

    // Checking client() is necessary, as the frame might have been detached as
    // part of dispatching the focus event above. See https://crbug.com/570874.
    if (m_focusedFrame && m_focusedFrame->client() && notifyEmbedder)
        m_focusedFrame->client()->frameFocused();
}

void FocusController::focusDocumentView(Frame* frame, bool notifyEmbedder)
{
    DCHECK(!frame || frame->page() == m_page);
    if (m_focusedFrame == frame)
        return;

    LocalFrame* focusedFrame = (m_focusedFrame && m_focusedFrame->isLocalFrame()) ? toLocalFrame(m_focusedFrame.get()) : nullptr;
    if (focusedFrame && focusedFrame->view()) {
        Document* document = focusedFrame->document();
        Element* focusedElement = document ? document->focusedElement() : nullptr;
        if (focusedElement)
            dispatchBlurEvent(*document, *focusedElement);
    }

    LocalFrame* newFocusedFrame = (frame && frame->isLocalFrame()) ? toLocalFrame(frame) : nullptr;
    if (newFocusedFrame && newFocusedFrame->view()) {
        Document* document = newFocusedFrame->document();
        Element* focusedElement = document ? document->focusedElement() : nullptr;
        if (focusedElement)
            dispatchFocusEvent(*document, *focusedElement);
    }

    setFocusedFrame(frame, notifyEmbedder);
}

LocalFrame* FocusController::focusedFrame() const
{
    // TODO(alexmos): Strengthen this to DCHECK that whoever called this really
    // expected a LocalFrame. Refactor call sites so that the rare cases that
    // need to know about focused RemoteFrames use a separate accessor (to be
    // added).
    if (m_focusedFrame && m_focusedFrame->isRemoteFrame())
        return nullptr;
    return toLocalFrame(m_focusedFrame.get());
}

Frame* FocusController::focusedOrMainFrame() const
{
    if (LocalFrame* frame = focusedFrame())
        return frame;

    // FIXME: This is a temporary hack to ensure that we return a LocalFrame, even when the mainFrame is remote.
    // FocusController needs to be refactored to deal with RemoteFrames cross-process focus transfers.
    for (Frame* frame = m_page->mainFrame()->tree().top(); frame; frame = frame->tree().traverseNext()) {
        if (frame->isLocalRoot())
            return frame;
    }

    return m_page->mainFrame();
}

HTMLFrameOwnerElement* FocusController::focusedFrameOwnerElement(LocalFrame& currentFrame) const
{
    Frame* focusedFrame = m_focusedFrame.get();
    for (; focusedFrame; focusedFrame = focusedFrame->tree().parent()) {
        if (focusedFrame->tree().parent() == &currentFrame) {
            DCHECK(focusedFrame->owner()->isLocal());
            return focusedFrame->deprecatedLocalOwner();
        }
    }
    return nullptr;
}

bool FocusController::isDocumentFocused(const Document& document) const
{
    if (!isActive() || !isFocused())
        return false;

    return m_focusedFrame && m_focusedFrame->tree().isDescendantOf(document.frame());
}

void FocusController::setFocused(bool focused)
{
    if (isFocused() == focused)
        return;

    m_isFocused = focused;

    if (!m_isFocused && focusedOrMainFrame()->isLocalFrame())
        toLocalFrame(focusedOrMainFrame())->eventHandler().stopAutoscroll();

    if (!m_focusedFrame)
        setFocusedFrame(m_page->mainFrame());

    // setFocusedFrame above might reject to update m_focusedFrame, or
    // m_focusedFrame might be changed by blur/focus event handlers.
    if (m_focusedFrame && m_focusedFrame->isLocalFrame() && toLocalFrame(m_focusedFrame.get())->view()) {
        toLocalFrame(m_focusedFrame.get())->selection().setFocused(focused);
        dispatchEventsOnWindowAndFocusedElement(toLocalFrame(m_focusedFrame.get())->document(), focused);
    }
}

bool FocusController::setInitialFocus(WebFocusType type)
{
    bool didAdvanceFocus = advanceFocus(type, true);

    // If focus is being set initially, accessibility needs to be informed that system focus has moved
    // into the web area again, even if focus did not change within WebCore. PostNotification is called instead
    // of handleFocusedUIElementChanged, because this will send the notification even if the element is the same.
    if (focusedOrMainFrame()->isLocalFrame()) {
        Document* document = toLocalFrame(focusedOrMainFrame())->document();
        if (AXObjectCache* cache = document->existingAXObjectCache())
            cache->handleInitialFocus();
    }

    return didAdvanceFocus;
}

bool FocusController::advanceFocus(WebFocusType type, bool initialFocus, InputDeviceCapabilities* sourceCapabilities)
{
    switch (type) {
    case WebFocusTypeForward:
    case WebFocusTypeBackward: {
        // We should never hit this when a RemoteFrame is focused, since the key
        // event that initiated focus advancement should've been routed to that
        // frame's process from the beginning.
        LocalFrame* startingFrame = toLocalFrame(focusedOrMainFrame());
        return advanceFocusInDocumentOrder(startingFrame, nullptr, type, initialFocus, sourceCapabilities);
    }
    case WebFocusTypeLeft:
    case WebFocusTypeRight:
    case WebFocusTypeUp:
    case WebFocusTypeDown:
        return advanceFocusDirectionally(type);
    default:
        NOTREACHED();
    }

    return false;
}

bool FocusController::advanceFocusAcrossFrames(WebFocusType type, RemoteFrame* from, LocalFrame* to, InputDeviceCapabilities* sourceCapabilities)
{
    // If we are shifting focus from a child frame to its parent, the
    // child frame has no more focusable elements, and we should continue
    // looking for focusable elements in the parent, starting from the <iframe>
    // element of the child frame.
    Element* start = nullptr;
    if (from->tree().parent() == to) {
        DCHECK(from->owner()->isLocal());
        start = toHTMLFrameOwnerElement(from->owner());
    }

    return advanceFocusInDocumentOrder(to, start, type, false, sourceCapabilities);
}


#if DCHECK_IS_ON()
inline bool isNonFocusableShadowHost(const Element& element)
{
    return isShadowHostWithoutCustomFocusLogic(element) && !element.isFocusable();
}
#endif

bool FocusController::advanceFocusInDocumentOrder(LocalFrame* frame, Element* start, WebFocusType type, bool initialFocus, InputDeviceCapabilities* sourceCapabilities)
{
    DCHECK(frame);
    Document* document = frame->document();
    document->updateDistribution();

    Element* current = start;
#if DCHECK_IS_ON()
    DCHECK(!current || !isNonFocusableShadowHost(*current));
#endif
    if (!current && !initialFocus)
        current = document->sequentialFocusNavigationStartingPoint(type);

    // FIXME: Not quite correct when it comes to focus transitions leaving/entering the WebView itself
    bool caretBrowsing = frame->settings() && frame->settings()->caretBrowsingEnabled();

    if (caretBrowsing && !current)
        current = adjustToElement(frame->selection().start().anchorNode(), type);

    document->updateStyleAndLayoutIgnorePendingStylesheets();
    ScopedFocusNavigation scope = current ? ScopedFocusNavigation::createFor(*current) : ScopedFocusNavigation::createForDocument(*document);
    Element* element = findFocusableElementAcrossFocusScopes(type, scope);
    if (!element) {
        // If there's a RemoteFrame on the ancestor chain, we need to continue
        // searching for focusable elements there.
        if (frame->localFrameRoot() != frame->tree().top()) {
            document->clearFocusedElement();
            document->setSequentialFocusNavigationStartingPoint(nullptr);
            toRemoteFrame(frame->localFrameRoot()->tree().parent())->advanceFocus(type, frame->localFrameRoot());
            return true;
        }

        // We didn't find an element to focus, so we should try to pass focus to Chrome.
        if (!initialFocus && m_page->chromeClient().canTakeFocus(type)) {
            document->clearFocusedElement();
            document->setSequentialFocusNavigationStartingPoint(nullptr);
            setFocusedFrame(nullptr);
            m_page->chromeClient().takeFocus(type);
            return true;
        }

        // Chrome doesn't want focus, so we should wrap focus.
        ScopedFocusNavigation scope = ScopedFocusNavigation::createForDocument(*toLocalFrame(m_page->mainFrame())->document());
        element = findFocusableElementRecursively(type, scope);
        element = findFocusableElementDescendingDownIntoFrameDocument(type, element);

        if (!element)
            return false;
    }

    if (element == document->focusedElement()) {
        // Focus wrapped around to the same element.
        return true;
    }

    if (element->isFrameOwnerElement() && (!isHTMLPlugInElement(*element) || !element->isKeyboardFocusable())) {
        // We focus frames rather than frame owners.
        // FIXME: We should not focus frames that have no scrollbars, as focusing them isn't useful to the user.
        HTMLFrameOwnerElement* owner = toHTMLFrameOwnerElement(element);
        if (!owner->contentFrame())
            return false;

        document->clearFocusedElement();
        setFocusedFrame(owner->contentFrame());

        // If contentFrame is remote, continue the search for focusable
        // elements in that frame's process.
        // clearFocusedElement() fires events that might detach the
        // contentFrame, hence the need to null-check it again.
        if (owner->contentFrame() && owner->contentFrame()->isRemoteFrame())
            toRemoteFrame(owner->contentFrame())->advanceFocus(type, frame);

        return true;
    }

    DCHECK(element->isFocusable());

    // FIXME: It would be nice to just be able to call setFocusedElement(element)
    // here, but we can't do that because some elements (e.g. HTMLInputElement
    // and HTMLTextAreaElement) do extra work in their focus() methods.
    Document& newDocument = element->document();

    if (&newDocument != document) {
        // Focus is going away from this document, so clear the focused element.
        document->clearFocusedElement();
    }

    setFocusedFrame(newDocument.frame());

    if (caretBrowsing) {
        Position position = firstPositionInOrBeforeNode(element);
        VisibleSelection newSelection(position, position);
        frame->selection().setSelection(newSelection);
    }

    element->focus(FocusParams(SelectionBehaviorOnFocus::Reset, type, sourceCapabilities));
    return true;
}

Element* FocusController::findFocusableElement(WebFocusType type, Element& element)
{
    // FIXME: No spacial navigation code yet.
    DCHECK(type == WebFocusTypeForward || type == WebFocusTypeBackward);
    ScopedFocusNavigation scope = ScopedFocusNavigation::createFor(element);
    return findFocusableElementAcrossFocusScopes(type, scope);
}

Element* FocusController::findFocusableElementInShadowHost(const Element& shadowHost)
{
    DCHECK(shadowHost.authorShadowRoot());
    ScopedFocusNavigation scope = ScopedFocusNavigation::ownedByShadowHost(shadowHost);
    return findFocusableElementAcrossFocusScopes(WebFocusTypeForward, scope);
}

static bool relinquishesEditingFocus(const Element& element)
{
    DCHECK(element.hasEditableStyle());
    return element.document().frame() && element.rootEditableElement();
}

static void clearSelectionIfNeeded(LocalFrame* oldFocusedFrame, LocalFrame* newFocusedFrame, Element* newFocusedElement)
{
    if (!oldFocusedFrame || !newFocusedFrame)
        return;

    if (oldFocusedFrame->document() != newFocusedFrame->document())
        return;

    FrameSelection& selection = oldFocusedFrame->selection();
    if (selection.isNone())
        return;

    bool caretBrowsing = oldFocusedFrame->settings()->caretBrowsingEnabled();
    if (caretBrowsing)
        return;

    Node* selectionStartNode = selection.selection().start().anchorNode();
    if (selectionStartNode == newFocusedElement || selectionStartNode->isDescendantOf(newFocusedElement))
        return;

    if (!enclosingTextFormControl(selectionStartNode))
        return;

    if (selectionStartNode->isInShadowTree() && selectionStartNode->shadowHost() == newFocusedElement)
        return;

    selection.clear();
}

bool FocusController::setFocusedElement(Element* element, Frame* newFocusedFrame)
{
    return setFocusedElement(element, newFocusedFrame, FocusParams(SelectionBehaviorOnFocus::None, WebFocusTypeNone, nullptr));
}

bool FocusController::setFocusedElement(Element* element, Frame* newFocusedFrame, const FocusParams& params)
{
    LocalFrame* oldFocusedFrame = focusedFrame();
    Document* oldDocument = oldFocusedFrame ? oldFocusedFrame->document() : nullptr;

    Element* oldFocusedElement = oldDocument ? oldDocument->focusedElement() : nullptr;
    if (element && oldFocusedElement == element)
        return true;

    // FIXME: Might want to disable this check for caretBrowsing
    if (oldFocusedElement && oldFocusedElement->isRootEditableElement() && !relinquishesEditingFocus(*oldFocusedElement))
        return false;

    m_page->chromeClient().willSetInputMethodState();

    Document* newDocument = nullptr;
    if (element)
        newDocument = &element->document();
    else if (newFocusedFrame && newFocusedFrame->isLocalFrame())
        newDocument = toLocalFrame(newFocusedFrame)->document();

    if (newDocument && oldDocument == newDocument && newDocument->focusedElement() == element)
        return true;

    if (newFocusedFrame && newFocusedFrame->isLocalFrame())
        clearSelectionIfNeeded(oldFocusedFrame, toLocalFrame(newFocusedFrame), element);

    if (oldDocument && oldDocument != newDocument)
        oldDocument->clearFocusedElement();

    if (newFocusedFrame && !newFocusedFrame->page()) {
        setFocusedFrame(nullptr);
        return false;
    }
    setFocusedFrame(newFocusedFrame);

    if (newDocument) {
        bool successfullyFocused = newDocument->setFocusedElement(element, params);
        if (!successfullyFocused)
            return false;
    }

    return true;
}

void FocusController::setActive(bool active)
{
    if (m_isActive == active)
        return;

    m_isActive = active;

    Frame* frame = focusedOrMainFrame();
    if (frame->isLocalFrame()) {
        // Invalidate all custom scrollbars because they support the CSS
        // window-active attribute. This should be applied to the entire page so
        // we invalidate from the root FrameView instead of just the focused.
        if (FrameView* view = toLocalFrame(frame)->localFrameRoot()->document()->view())
            view->invalidateAllCustomScrollbarsOnActiveChanged();
        toLocalFrame(frame)->selection().pageActivationChanged();
    }
}

static void updateFocusCandidateIfNeeded(WebFocusType type, const FocusCandidate& current, FocusCandidate& candidate, FocusCandidate& closest)
{
    DCHECK(candidate.visibleNode->isElementNode());
    DCHECK(candidate.visibleNode->layoutObject());

    // Ignore iframes that don't have a src attribute
    if (frameOwnerElement(candidate) && (!frameOwnerElement(candidate)->contentFrame() || candidate.rect.isEmpty()))
        return;

    // Ignore off screen child nodes of containers that do not scroll (overflow:hidden)
    if (candidate.isOffscreen && !canBeScrolledIntoView(type, candidate))
        return;

    distanceDataForNode(type, current, candidate);
    if (candidate.distance == maxDistance())
        return;

    if (candidate.isOffscreenAfterScrolling)
        return;

    if (closest.isNull()) {
        closest = candidate;
        return;
    }

    LayoutRect intersectionRect = intersection(candidate.rect, closest.rect);
    if (!intersectionRect.isEmpty() && !areElementsOnSameLine(closest, candidate)
        && intersectionRect == candidate.rect) {
        // If 2 nodes are intersecting, do hit test to find which node in on top.
        LayoutUnit x = intersectionRect.x() + intersectionRect.width() / 2;
        LayoutUnit y = intersectionRect.y() + intersectionRect.height() / 2;
        if (!candidate.visibleNode->document().page()->mainFrame()->isLocalFrame())
            return;
        HitTestResult result = candidate.visibleNode->document().page()->deprecatedLocalMainFrame()->eventHandler().hitTestResultAtPoint(IntPoint(x, y), HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::IgnoreClipping);
        if (candidate.visibleNode->contains(result.innerNode())) {
            closest = candidate;
            return;
        }
        if (closest.visibleNode->contains(result.innerNode()))
            return;
    }

    if (candidate.distance < closest.distance)
        closest = candidate;
}

void FocusController::findFocusCandidateInContainer(Node& container, const LayoutRect& startingRect, WebFocusType type, FocusCandidate& closest)
{
    Element* focusedElement = (focusedFrame() && focusedFrame()->document()) ? focusedFrame()->document()->focusedElement() : nullptr;

    Element* element = ElementTraversal::firstWithin(container);
    FocusCandidate current;
    current.rect = startingRect;
    current.focusableNode = focusedElement;
    current.visibleNode = focusedElement;

    for (; element; element = (element->isFrameOwnerElement() || canScrollInDirection(element, type))
        ? ElementTraversal::nextSkippingChildren(*element, &container)
        : ElementTraversal::next(*element, &container)) {
        if (element == focusedElement)
            continue;

        if (!element->isKeyboardFocusable() && !element->isFrameOwnerElement() && !canScrollInDirection(element, type))
            continue;

        FocusCandidate candidate = FocusCandidate(element, type);
        if (candidate.isNull())
            continue;

        candidate.enclosingScrollableBox = &container;
        updateFocusCandidateIfNeeded(type, current, candidate, closest);
    }
}

bool FocusController::advanceFocusDirectionallyInContainer(Node* container, const LayoutRect& startingRect, WebFocusType type)
{
    if (!container)
        return false;

    LayoutRect newStartingRect = startingRect;

    if (startingRect.isEmpty())
        newStartingRect = virtualRectForDirection(type, nodeRectInAbsoluteCoordinates(container));

    // Find the closest node within current container in the direction of the navigation.
    FocusCandidate focusCandidate;
    findFocusCandidateInContainer(*container, newStartingRect, type, focusCandidate);

    if (focusCandidate.isNull()) {
        // Nothing to focus, scroll if possible.
        // NOTE: If no scrolling is performed (i.e. scrollInDirection returns false), the
        // spatial navigation algorithm will skip this container.
        return scrollInDirection(container, type);
    }

    HTMLFrameOwnerElement* frameElement = frameOwnerElement(focusCandidate);
    // If we have an iframe without the src attribute, it will not have a contentFrame().
    // We DCHECK here to make sure that
    // updateFocusCandidateIfNeeded() will never consider such an iframe as a candidate.
    DCHECK(!frameElement || frameElement->contentFrame());
    if (frameElement && frameElement->contentFrame()->isLocalFrame()) {
        if (focusCandidate.isOffscreenAfterScrolling) {
            scrollInDirection(&focusCandidate.visibleNode->document(), type);
            return true;
        }
        // Navigate into a new frame.
        LayoutRect rect;
        Element* focusedElement = toLocalFrame(focusedOrMainFrame())->document()->focusedElement();
        if (focusedElement && !hasOffscreenRect(focusedElement))
            rect = nodeRectInAbsoluteCoordinates(focusedElement, true /* ignore border */);
        toLocalFrame(frameElement->contentFrame())->document()->updateStyleAndLayoutIgnorePendingStylesheets();
        if (!advanceFocusDirectionallyInContainer(toLocalFrame(frameElement->contentFrame())->document(), rect, type)) {
            // The new frame had nothing interesting, need to find another candidate.
            return advanceFocusDirectionallyInContainer(container, nodeRectInAbsoluteCoordinates(focusCandidate.visibleNode, true), type);
        }
        return true;
    }

    if (canScrollInDirection(focusCandidate.visibleNode, type)) {
        if (focusCandidate.isOffscreenAfterScrolling) {
            scrollInDirection(focusCandidate.visibleNode, type);
            return true;
        }
        // Navigate into a new scrollable container.
        LayoutRect startingRect;
        Element* focusedElement = toLocalFrame(focusedOrMainFrame())->document()->focusedElement();
        if (focusedElement && !hasOffscreenRect(focusedElement))
            startingRect = nodeRectInAbsoluteCoordinates(focusedElement, true);
        return advanceFocusDirectionallyInContainer(focusCandidate.visibleNode, startingRect, type);
    }
    if (focusCandidate.isOffscreenAfterScrolling) {
        Node* container = focusCandidate.enclosingScrollableBox;
        scrollInDirection(container, type);
        return true;
    }

    // We found a new focus node, navigate to it.
    Element* element = toElement(focusCandidate.focusableNode);
    DCHECK(element);

    element->focus(FocusParams(SelectionBehaviorOnFocus::Reset, type, nullptr));
    return true;
}

bool FocusController::advanceFocusDirectionally(WebFocusType type)
{
    // FIXME: Directional focus changes don't yet work with RemoteFrames.
    if (!focusedOrMainFrame()->isLocalFrame())
        return false;
    LocalFrame* curFrame = toLocalFrame(focusedOrMainFrame());
    DCHECK(curFrame);

    Document* focusedDocument = curFrame->document();
    if (!focusedDocument)
        return false;

    Element* focusedElement = focusedDocument->focusedElement();
    Node* container = focusedDocument;

    if (container->isDocumentNode())
        toDocument(container)->updateStyleAndLayoutIgnorePendingStylesheets();

    // Figure out the starting rect.
    LayoutRect startingRect;
    if (focusedElement) {
        if (!hasOffscreenRect(focusedElement)) {
            container = scrollableEnclosingBoxOrParentFrameForNodeInDirection(type, focusedElement);
            startingRect = nodeRectInAbsoluteCoordinates(focusedElement, true /* ignore border */);
        } else if (isHTMLAreaElement(*focusedElement)) {
            HTMLAreaElement& area = toHTMLAreaElement(*focusedElement);
            container = scrollableEnclosingBoxOrParentFrameForNodeInDirection(type, area.imageElement());
            startingRect = virtualRectForAreaElementAndDirection(area, type);
        }
    }

    bool consumed = false;
    do {
        consumed = advanceFocusDirectionallyInContainer(container, startingRect, type);
        startingRect = nodeRectInAbsoluteCoordinates(container, true /* ignore border */);
        container = scrollableEnclosingBoxOrParentFrameForNodeInDirection(type, container);
        if (container && container->isDocumentNode())
            toDocument(container)->updateStyleAndLayoutIgnorePendingStylesheets();
    } while (!consumed && container);

    return consumed;
}

DEFINE_TRACE(FocusController)
{
    visitor->trace(m_page);
    visitor->trace(m_focusedFrame);
}

} // namespace blink
