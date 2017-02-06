/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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
 */

#include "core/css/SelectorChecker.h"

#include "core/HTMLNames.h"
#include "core/css/CSSSelectorList.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/Fullscreen.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/NthIndexCache.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/FlatTreeTraversal.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/html/HTMLSlotElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/track/vtt/VTTElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutScrollbar.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/style/ComputedStyle.h"
#include "platform/scroll/ScrollableArea.h"
#include "platform/scroll/ScrollbarTheme.h"

namespace blink {

using namespace HTMLNames;

static bool isFrameFocused(const Element& element)
{
    return element.document().frame() && element.document().frame()->selection().isFocusedAndActive();
}

static bool matchesSpatialNavigationFocusPseudoClass(const Element& element)
{
    return isHTMLOptionElement(element) && toHTMLOptionElement(element).spatialNavigationFocused() && isFrameFocused(element);
}

static bool matchesListBoxPseudoClass(const Element& element)
{
    return isHTMLSelectElement(element) && !toHTMLSelectElement(element).usesMenuList();
}

static bool matchesTagName(const Element& element, const QualifiedName& tagQName)
{
    if (tagQName == anyQName())
        return true;
    const AtomicString& localName = tagQName.localName();
    if (localName != starAtom && localName != element.localName()) {
        if (element.isHTMLElement() || !element.document().isHTMLDocument())
            return false;
        // Non-html elements in html documents are normalized to their camel-cased
        // version during parsing if applicable. Yet, type selectors are lower-cased
        // for selectors in html documents. Compare the upper case converted names
        // instead to allow matching SVG elements like foreignObject.
        if (element.tagQName().localNameUpper() != tagQName.localNameUpper())
            return false;
    }
    const AtomicString& namespaceURI = tagQName.namespaceURI();
    return namespaceURI == starAtom || namespaceURI == element.namespaceURI();
}

static Element* parentElement(const SelectorChecker::SelectorCheckingContext& context)
{
    // - If context.scope is a shadow root, we should walk up to its shadow host.
    // - If context.scope is some element in some shadow tree and querySelector initialized the context,
    //   e.g. shadowRoot.querySelector(':host *'),
    //   (a) context.element has the same treescope as context.scope, need to walk up to its shadow host.
    //   (b) Otherwise, should not walk up from a shadow root to a shadow host.
    if (context.scope && (context.scope == context.element->containingShadowRoot() || context.scope->treeScope() == context.element->treeScope()))
        return context.element->parentOrShadowHostElement();
    return context.element->parentElement();
}

static const HTMLSlotElement* findSlotElementInScope(const SelectorChecker::SelectorCheckingContext& context)
{
    if (!context.scope)
        return nullptr;

    const HTMLSlotElement* slot = context.element->assignedSlot();
    while (slot) {
        if (slot->treeScope() == context.scope->treeScope())
            return slot;
        slot = slot->assignedSlot();
    }
    return nullptr;
}

static bool scopeContainsLastMatchedElement(const SelectorChecker::SelectorCheckingContext& context)
{
    // If this context isn't scoped, skip checking.
    if (!context.scope)
        return true;

    if (context.scope->treeScope() == context.element->treeScope())
        return true;

    // Because Blink treats a shadow host's TreeScope as a separate one from its descendent shadow roots,
    // if the last matched element is a shadow host, the condition above isn't met, even though it
    // should be.
    return context.element == context.scope->shadowHost() && (!context.previousElement || context.previousElement->isInDescendantTreeOf(context.element));
}

static inline bool nextSelectorExceedsScope(const SelectorChecker::SelectorCheckingContext& context)
{
    if (context.scope && context.scope->isInShadowTree())
        return context.element == context.scope->shadowHost();

    return false;
}

static bool shouldMatchHoverOrActive(const SelectorChecker::SelectorCheckingContext& context)
{
    // If we're in quirks mode, then :hover and :active should never match anchors with no
    // href and *:hover and *:active should not match anything. This is specified in
    // https://quirks.spec.whatwg.org/#the-:active-and-:hover-quirk
    if (!context.element->document().inQuirksMode())
        return true;
    if (context.isSubSelector)
        return true;
    if (context.element->isLink())
        return true;
    const CSSSelector* selector = context.selector;
    while (selector->relation() == CSSSelector::SubSelector && selector->tagHistory()) {
        selector = selector->tagHistory();
        if (selector->match() != CSSSelector::PseudoClass)
            return true;
        if (selector->getPseudoType() != CSSSelector::PseudoHover && selector->getPseudoType() != CSSSelector::PseudoActive)
            return true;
    }
    return false;
}

static bool isFirstChild(Element& element)
{
    return !ElementTraversal::previousSibling(element);
}

static bool isLastChild(Element& element)
{
    return !ElementTraversal::nextSibling(element);
}

static bool isFirstOfType(Element& element, const QualifiedName& type)
{
    return !ElementTraversal::previousSibling(element, HasTagName(type));
}

static bool isLastOfType(Element& element, const QualifiedName& type)
{
    return !ElementTraversal::nextSibling(element, HasTagName(type));
}

// Recursive check of selectors and combinators
// It can return 4 different values:
// * SelectorMatches          - the selector matches the element e
// * SelectorFailsLocally     - the selector fails for the element e
// * SelectorFailsAllSiblings - the selector fails for e and any sibling of e
// * SelectorFailsCompletely  - the selector fails for e and any sibling or ancestor of e
SelectorChecker::Match SelectorChecker::matchSelector(const SelectorCheckingContext& context, MatchResult& result) const
{
    MatchResult subResult;
    if (!checkOne(context, subResult))
        return SelectorFailsLocally;

    if (subResult.dynamicPseudo != PseudoIdNone)
        result.dynamicPseudo = subResult.dynamicPseudo;

    if (context.selector->isLastInTagHistory()) {
        if (scopeContainsLastMatchedElement(context)) {
            result.specificity += subResult.specificity;
            return SelectorMatches;
        }
        return SelectorFailsLocally;
    }

    Match match;
    if (context.selector->relation() != CSSSelector::SubSelector) {
        if (nextSelectorExceedsScope(context))
            return SelectorFailsCompletely;

        if (context.pseudoId != PseudoIdNone && context.pseudoId != result.dynamicPseudo)
            return SelectorFailsCompletely;

        TemporaryChange<PseudoId> dynamicPseudoScope(result.dynamicPseudo, PseudoIdNone);
        match = matchForRelation(context, result);
    } else {
        match = matchForSubSelector(context, result);
    }
    if (match == SelectorMatches)
        result.specificity += subResult.specificity;
    return match;
}

static inline SelectorChecker::SelectorCheckingContext prepareNextContextForRelation(const SelectorChecker::SelectorCheckingContext& context)
{
    SelectorChecker::SelectorCheckingContext nextContext(context);
    ASSERT(context.selector->tagHistory());
    nextContext.selector = context.selector->tagHistory();
    return nextContext;
}

SelectorChecker::Match SelectorChecker::matchForSubSelector(const SelectorCheckingContext& context, MatchResult& result) const
{
    SelectorCheckingContext nextContext = prepareNextContextForRelation(context);

    PseudoId dynamicPseudo = result.dynamicPseudo;
    nextContext.hasScrollbarPseudo = dynamicPseudo != PseudoIdNone && (m_scrollbar || dynamicPseudo == PseudoIdScrollbarCorner || dynamicPseudo == PseudoIdResizer);
    nextContext.hasSelectionPseudo = dynamicPseudo == PseudoIdSelection;
    nextContext.isSubSelector = true;
    return matchSelector(nextContext, result);
}

static inline bool isV0ShadowRoot(const Node* node)
{
    return node && node->isShadowRoot() && toShadowRoot(node)->type() == ShadowRootType::V0;
}

SelectorChecker::Match SelectorChecker::matchForPseudoShadow(const SelectorCheckingContext& context, const ContainerNode* node, MatchResult& result) const
{
    if (!isV0ShadowRoot(node))
        return SelectorFailsCompletely;
    if (!context.previousElement)
        return SelectorFailsCompletely;
    return matchSelector(context, result);
}

static inline Element* parentOrV0ShadowHostElement(const Element& element)
{
    if (element.parentNode() && element.parentNode()->isShadowRoot()) {
        if (toShadowRoot(element.parentNode())->type() != ShadowRootType::V0)
            return nullptr;
    }
    return element.parentOrShadowHostElement();
}

SelectorChecker::Match SelectorChecker::matchForRelation(const SelectorCheckingContext& context, MatchResult& result) const
{
    SelectorCheckingContext nextContext = prepareNextContextForRelation(context);

    CSSSelector::RelationType relation = context.selector->relation();

    // Disable :visited matching when we see the first link or try to match anything else than an ancestors.
    if (!context.isSubSelector && (context.element->isLink() || (relation != CSSSelector::Descendant && relation != CSSSelector::Child)))
        nextContext.visitedMatchType = VisitedMatchDisabled;

    nextContext.inRightmostCompound = false;
    nextContext.isSubSelector = false;
    nextContext.previousElement = context.element;
    nextContext.pseudoId = PseudoIdNone;

    switch (relation) {
    case CSSSelector::Descendant:
        if (context.selector->relationIsAffectedByPseudoContent()) {
            for (Element* element = context.element; element; element = element->parentElement()) {
                if (matchForPseudoContent(nextContext, *element, result) == SelectorMatches)
                    return SelectorMatches;
            }
            return SelectorFailsCompletely;
        }

        if (nextContext.selector->getPseudoType() == CSSSelector::PseudoShadow)
            return matchForPseudoShadow(nextContext, context.element->containingShadowRoot(), result);

        for (nextContext.element = parentElement(context); nextContext.element; nextContext.element = parentElement(nextContext)) {
            Match match = matchSelector(nextContext, result);
            if (match == SelectorMatches || match == SelectorFailsCompletely)
                return match;
            if (nextSelectorExceedsScope(nextContext))
                return SelectorFailsCompletely;
        }
        return SelectorFailsCompletely;
    case CSSSelector::Child:
        {
            if (context.selector->relationIsAffectedByPseudoContent())
                return matchForPseudoContent(nextContext, *context.element, result);

            if (nextContext.selector->getPseudoType() == CSSSelector::PseudoShadow)
                return matchForPseudoShadow(nextContext, context.element->parentNode(), result);

            nextContext.element = parentElement(context);
            if (!nextContext.element)
                return SelectorFailsCompletely;
            return matchSelector(nextContext, result);
        }
    case CSSSelector::DirectAdjacent:
        // Shadow roots can't have sibling elements
        if (nextContext.selector->getPseudoType() == CSSSelector::PseudoShadow)
            return SelectorFailsCompletely;

        if (m_mode == ResolvingStyle) {
            if (ContainerNode* parent = context.element->parentElementOrShadowRoot())
                parent->setChildrenAffectedByDirectAdjacentRules();
        }
        nextContext.element = ElementTraversal::previousSibling(*context.element);
        if (!nextContext.element)
            return SelectorFailsAllSiblings;
        return matchSelector(nextContext, result);

    case CSSSelector::IndirectAdjacent:
        // Shadow roots can't have sibling elements
        if (nextContext.selector->getPseudoType() == CSSSelector::PseudoShadow)
            return SelectorFailsCompletely;

        if (m_mode == ResolvingStyle) {
            if (ContainerNode* parent = context.element->parentElementOrShadowRoot())
                parent->setChildrenAffectedByIndirectAdjacentRules();
        }
        nextContext.element = ElementTraversal::previousSibling(*context.element);
        for (; nextContext.element; nextContext.element = ElementTraversal::previousSibling(*nextContext.element)) {
            Match match = matchSelector(nextContext, result);
            if (match == SelectorMatches || match == SelectorFailsAllSiblings || match == SelectorFailsCompletely)
                return match;
        }
        return SelectorFailsAllSiblings;

    case CSSSelector::ShadowPseudo:
        {
            if (!m_isUARule && !m_isQuerySelector && context.selector->getPseudoType() == CSSSelector::PseudoShadow)
                Deprecation::countDeprecation(context.element->document(), UseCounter::CSSSelectorPseudoShadow);
            // If we're in the same tree-scope as the scoping element, then following a shadow descendant combinator would escape that and thus the scope.
            if (context.scope && context.scope->shadowHost() && context.scope->shadowHost()->treeScope() == context.element->treeScope())
                return SelectorFailsCompletely;

            Element* shadowHost = context.element->shadowHost();
            if (!shadowHost)
                return SelectorFailsCompletely;
            nextContext.element = shadowHost;
            return matchSelector(nextContext, result);
        }

    case CSSSelector::ShadowDeep:
        {
            if (!m_isUARule && !m_isQuerySelector)
                Deprecation::countDeprecation(context.element->document(), UseCounter::CSSDeepCombinator);
            if (ShadowRoot* root = context.element->containingShadowRoot()) {
                if (root->type() == ShadowRootType::UserAgent)
                    return SelectorFailsCompletely;
            }


            if (context.selector->relationIsAffectedByPseudoContent()) {
                // TODO(kochi): closed mode tree should be handled as well for ::content.
                for (Element* element = context.element; element; element = element->parentOrShadowHostElement()) {
                    if (matchForPseudoContent(nextContext, *element, result) == SelectorMatches) {
                        if (context.element->isInShadowTree())
                            UseCounter::count(context.element->document(), UseCounter::CSSDeepCombinatorAndShadow);
                        return SelectorMatches;
                    }
                }
                return SelectorFailsCompletely;
            }

            for (nextContext.element = parentOrV0ShadowHostElement(*context.element); nextContext.element; nextContext.element = parentOrV0ShadowHostElement(*nextContext.element)) {
                Match match = matchSelector(nextContext, result);
                if (match == SelectorMatches && context.element->isInShadowTree())
                    UseCounter::count(context.element->document(), UseCounter::CSSDeepCombinatorAndShadow);
                if (match == SelectorMatches || match == SelectorFailsCompletely)
                    return match;
                if (nextSelectorExceedsScope(nextContext))
                    return SelectorFailsCompletely;
            }
            return SelectorFailsCompletely;
        }

    case CSSSelector::ShadowSlot:
        {
            const HTMLSlotElement* slot = findSlotElementInScope(context);
            if (!slot)
                return SelectorFailsCompletely;

            nextContext.element = const_cast<HTMLSlotElement*>(slot);
            return matchSelector(nextContext, result);
        }

    case CSSSelector::SubSelector:
        break;
    }
    ASSERT_NOT_REACHED();
    return SelectorFailsCompletely;
}

SelectorChecker::Match SelectorChecker::matchForPseudoContent(const SelectorCheckingContext& context, const Element& element, MatchResult& result) const
{
    HeapVector<Member<InsertionPoint>, 8> insertionPoints;
    collectDestinationInsertionPoints(element, insertionPoints);
    SelectorCheckingContext nextContext(context);
    for (const auto& insertionPoint : insertionPoints) {
        nextContext.element = insertionPoint;
        // TODO(esprehn): Why does SharingRules have a special case?
        if (m_mode == SharingRules)
            nextContext.scope = insertionPoint->containingShadowRoot();
        if (match(nextContext, result))
            return SelectorMatches;
    }
    return SelectorFailsLocally;
}

static bool attributeValueMatches(const Attribute& attributeItem, CSSSelector::MatchType match, const AtomicString& selectorValue, TextCaseSensitivity caseSensitivity)
{
    // TODO(esprehn): How do we get here with a null value?
    const AtomicString& value = attributeItem.value();
    if (value.isNull())
        return false;

    switch (match) {
    case CSSSelector::AttributeExact:
        if (caseSensitivity == TextCaseSensitive)
            return selectorValue == value;
        return equalIgnoringASCIICase(selectorValue, value);
    case CSSSelector::AttributeSet:
        return true;
    case CSSSelector::AttributeList:
        {
            // Ignore empty selectors or selectors containing HTML spaces
            if (selectorValue.isEmpty() || selectorValue.find(&isHTMLSpace<UChar>) != kNotFound)
                return false;

            unsigned startSearchAt = 0;
            while (true) {
                size_t foundPos = value.find(selectorValue, startSearchAt, caseSensitivity);
                if (foundPos == kNotFound)
                    return false;
                if (!foundPos || isHTMLSpace<UChar>(value[foundPos - 1])) {
                    unsigned endStr = foundPos + selectorValue.length();
                    if (endStr == value.length() || isHTMLSpace<UChar>(value[endStr]))
                        break; // We found a match.
                }

                // No match. Keep looking.
                startSearchAt = foundPos + 1;
            }
            return true;
        }
    case CSSSelector::AttributeContain:
        if (selectorValue.isEmpty())
            return false;
        return value.contains(selectorValue, caseSensitivity);
    case CSSSelector::AttributeBegin:
        if (selectorValue.isEmpty())
            return false;
        return value.startsWith(selectorValue, caseSensitivity);
    case CSSSelector::AttributeEnd:
        if (selectorValue.isEmpty())
            return false;
        return value.endsWith(selectorValue, caseSensitivity);
    case CSSSelector::AttributeHyphen:
        if (value.length() < selectorValue.length())
            return false;
        if (!value.startsWith(selectorValue, caseSensitivity))
            return false;
        // It they start the same, check for exact match or following '-':
        if (value.length() != selectorValue.length() && value[selectorValue.length()] != '-')
            return false;
        return true;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

static bool anyAttributeMatches(Element& element, CSSSelector::MatchType match, const CSSSelector& selector)
{
    const QualifiedName& selectorAttr = selector.attribute();
    ASSERT(selectorAttr.localName() != starAtom); // Should not be possible from the CSS grammar.

    // Synchronize the attribute in case it is lazy-computed.
    // Currently all lazy properties have a null namespace, so only pass localName().
    element.synchronizeAttribute(selectorAttr.localName());

    const AtomicString& selectorValue = selector.value();
    TextCaseSensitivity caseSensitivity = (selector.attributeMatch() == CSSSelector::CaseInsensitive) ? TextCaseASCIIInsensitive : TextCaseSensitive;

    AttributeCollection attributes = element.attributesWithoutUpdate();
    for (const auto& attributeItem: attributes) {
        if (!attributeItem.matches(selectorAttr))
            continue;

        if (attributeValueMatches(attributeItem, match, selectorValue, caseSensitivity))
            return true;

        if (caseSensitivity == TextCaseASCIIInsensitive) {
            if (selectorAttr.namespaceURI() != starAtom)
                return false;
            continue;
        }

        // Legacy dictates that values of some attributes should be compared in
        // a case-insensitive manner regardless of whether the case insensitive
        // flag is set or not.
        bool legacyCaseInsensitive = element.document().isHTMLDocument() && !HTMLDocument::isCaseSensitiveAttribute(selectorAttr);

        // If case-insensitive, re-check, and count if result differs.
        // See http://code.google.com/p/chromium/issues/detail?id=327060
        if (legacyCaseInsensitive && attributeValueMatches(attributeItem, match, selectorValue, TextCaseASCIIInsensitive)) {
            UseCounter::count(element.document(), UseCounter::CaseInsensitiveAttrSelectorMatch);
            return true;
        }
        if (selectorAttr.namespaceURI() != starAtom)
            return false;
    }

    return false;
}

bool SelectorChecker::checkOne(const SelectorCheckingContext& context, MatchResult& result) const
{
    ASSERT(context.element);
    Element& element = *context.element;
    ASSERT(context.selector);
    const CSSSelector& selector = *context.selector;

    // Only :host and :host-context() should match the host: http://drafts.csswg.org/css-scoping/#host-element
    if (context.scope && context.scope->shadowHost() == element && (!selector.isHostPseudoClass()
        && !context.treatShadowHostAsNormalScope
        && selector.match() != CSSSelector::PseudoElement))
            return false;

    switch (selector.match()) {
    case CSSSelector::Tag:
        return matchesTagName(element, selector.tagQName());
    case CSSSelector::Class:
        return element.hasClass() && element.classNames().contains(selector.value());
    case CSSSelector::Id:
        return element.hasID() && element.idForStyleResolution() == selector.value();

    // Attribute selectors
    case CSSSelector::AttributeExact:
    case CSSSelector::AttributeSet:
    case CSSSelector::AttributeHyphen:
    case CSSSelector::AttributeList:
    case CSSSelector::AttributeContain:
    case CSSSelector::AttributeBegin:
    case CSSSelector::AttributeEnd:
        return anyAttributeMatches(element, selector.match(), selector);

    case CSSSelector::PseudoClass:
        return checkPseudoClass(context, result);
    case CSSSelector::PseudoElement:
        return checkPseudoElement(context, result);

    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool SelectorChecker::checkPseudoNot(const SelectorCheckingContext& context, MatchResult& result) const
{
    const CSSSelector& selector = *context.selector;

    SelectorCheckingContext subContext(context);
    subContext.isSubSelector = true;
    ASSERT(selector.selectorList());
    for (subContext.selector = selector.selectorList()->first(); subContext.selector; subContext.selector = subContext.selector->tagHistory()) {
        // :not cannot nest. I don't really know why this is a
        // restriction in CSS3, but it is, so let's honor it.
        // the parser enforces that this never occurs
        ASSERT(subContext.selector->getPseudoType() != CSSSelector::PseudoNot);
        // We select between :visited and :link when applying. We don't know which one applied (or not) yet.
        if (subContext.selector->getPseudoType() == CSSSelector::PseudoVisited || (subContext.selector->getPseudoType() == CSSSelector::PseudoLink && subContext.visitedMatchType == VisitedMatchEnabled))
            return true;
        // context.scope is not available if m_mode == SharingRules.
        // We cannot determine whether :host or :scope matches a given element or not.
        if (m_mode == SharingRules && (subContext.selector->isHostPseudoClass() || subContext.selector->getPseudoType() == CSSSelector::PseudoScope))
            return true;
        if (!checkOne(subContext, result))
            return true;
    }
    return false;
}

bool SelectorChecker::checkPseudoClass(const SelectorCheckingContext& context, MatchResult& result) const
{
    Element& element = *context.element;
    const CSSSelector& selector = *context.selector;

    if (context.hasScrollbarPseudo) {
        // CSS scrollbars match a specific subset of pseudo classes, and they have specialized rules for each
        // (since there are no elements involved).
        return checkScrollbarPseudoClass(context, result);
    }

    switch (selector.getPseudoType()) {
    case CSSSelector::PseudoNot:
        return checkPseudoNot(context, result);
    case CSSSelector::PseudoEmpty:
        {
            bool result = true;
            for (Node* n = element.firstChild(); n; n = n->nextSibling()) {
                if (n->isElementNode()) {
                    result = false;
                    break;
                }
                if (n->isTextNode()) {
                    Text* textNode = toText(n);
                    if (!textNode->data().isEmpty()) {
                        result = false;
                        break;
                    }
                }
            }
            if (m_mode == ResolvingStyle) {
                element.setStyleAffectedByEmpty();
                if (context.inRightmostCompound)
                    m_elementStyle->setEmptyState(result);
                else if (element.computedStyle() && (element.document().styleEngine().usesSiblingRules() || element.computedStyle()->unique()))
                    element.mutableComputedStyle()->setEmptyState(result);
            }
            return result;
        }
    case CSSSelector::PseudoFirstChild:
        if (ContainerNode* parent = element.parentElementOrDocumentFragment()) {
            if (m_mode == ResolvingStyle) {
                parent->setChildrenAffectedByFirstChildRules();
                element.setAffectedByFirstChildRules();
            }
            return isFirstChild(element);
        }
        break;
    case CSSSelector::PseudoFirstOfType:
        if (ContainerNode* parent = element.parentElementOrDocumentFragment()) {
            if (m_mode == ResolvingStyle)
                parent->setChildrenAffectedByForwardPositionalRules();
            return isFirstOfType(element, element.tagQName());
        }
        break;
    case CSSSelector::PseudoLastChild:
        if (ContainerNode* parent = element.parentElementOrDocumentFragment()) {
            if (m_mode == ResolvingStyle) {
                parent->setChildrenAffectedByLastChildRules();
                element.setAffectedByLastChildRules();
            }
            if (!parent->isFinishedParsingChildren())
                return false;
            return isLastChild(element);
        }
        break;
    case CSSSelector::PseudoLastOfType:
        if (ContainerNode* parent = element.parentElementOrDocumentFragment()) {
            if (m_mode == ResolvingStyle)
                parent->setChildrenAffectedByBackwardPositionalRules();
            if (!parent->isFinishedParsingChildren())
                return false;
            return isLastOfType(element, element.tagQName());
        }
        break;
    case CSSSelector::PseudoOnlyChild:
        if (ContainerNode* parent = element.parentElementOrDocumentFragment()) {
            if (m_mode == ResolvingStyle) {
                parent->setChildrenAffectedByFirstChildRules();
                parent->setChildrenAffectedByLastChildRules();
                element.setAffectedByFirstChildRules();
                element.setAffectedByLastChildRules();
            }
            if (!parent->isFinishedParsingChildren())
                return false;
            return isFirstChild(element) && isLastChild(element);
        }
        break;
    case CSSSelector::PseudoOnlyOfType:
        // FIXME: This selector is very slow.
        if (ContainerNode* parent = element.parentElementOrDocumentFragment()) {
            if (m_mode == ResolvingStyle) {
                parent->setChildrenAffectedByForwardPositionalRules();
                parent->setChildrenAffectedByBackwardPositionalRules();
            }
            if (!parent->isFinishedParsingChildren())
                return false;
            return isFirstOfType(element, element.tagQName()) && isLastOfType(element, element.tagQName());
        }
        break;
    case CSSSelector::PseudoPlaceholderShown:
        if (isHTMLTextFormControlElement(element))
            return toHTMLTextFormControlElement(element).isPlaceholderVisible();
        break;
    case CSSSelector::PseudoNthChild:
        if (ContainerNode* parent = element.parentElementOrDocumentFragment()) {
            if (m_mode == ResolvingStyle)
                parent->setChildrenAffectedByForwardPositionalRules();
            return selector.matchNth(NthIndexCache::nthChildIndex(element));
        }
        break;
    case CSSSelector::PseudoNthOfType:
        if (ContainerNode* parent = element.parentElementOrDocumentFragment()) {
            if (m_mode == ResolvingStyle)
                parent->setChildrenAffectedByForwardPositionalRules();
            return selector.matchNth(NthIndexCache::nthOfTypeIndex(element));
        }
        break;
    case CSSSelector::PseudoNthLastChild:
        if (ContainerNode* parent = element.parentElementOrDocumentFragment()) {
            if (m_mode == ResolvingStyle)
                parent->setChildrenAffectedByBackwardPositionalRules();
            if (!parent->isFinishedParsingChildren())
                return false;
            return selector.matchNth(NthIndexCache::nthLastChildIndex(element));
        }
        break;
    case CSSSelector::PseudoNthLastOfType:
        if (ContainerNode* parent = element.parentElementOrDocumentFragment()) {
            if (m_mode == ResolvingStyle)
                parent->setChildrenAffectedByBackwardPositionalRules();
            if (!parent->isFinishedParsingChildren())
                return false;
            return selector.matchNth(NthIndexCache::nthLastOfTypeIndex(element));
        }
        break;
    case CSSSelector::PseudoTarget:
        return element == element.document().cssTarget();
    case CSSSelector::PseudoAny:
        {
            SelectorCheckingContext subContext(context);
            subContext.isSubSelector = true;
            ASSERT(selector.selectorList());
            for (subContext.selector = selector.selectorList()->first(); subContext.selector; subContext.selector = CSSSelectorList::next(*subContext.selector)) {
                if (match(subContext))
                    return true;
            }
        }
        break;
    case CSSSelector::PseudoAutofill:
        return element.isFormControlElement() && toHTMLFormControlElement(element).isAutofilled();
    case CSSSelector::PseudoAnyLink:
    case CSSSelector::PseudoLink:
        return element.isLink();
    case CSSSelector::PseudoVisited:
        return element.isLink() && context.visitedMatchType == VisitedMatchEnabled;
    case CSSSelector::PseudoDrag:
        if (m_mode == ResolvingStyle) {
            if (context.inRightmostCompound) {
                m_elementStyle->setAffectedByDrag();
            } else {
                m_elementStyle->setUnique();
                element.setChildrenOrSiblingsAffectedByDrag();
            }
        }
        return element.layoutObject() && element.layoutObject()->isDragging();
    case CSSSelector::PseudoFocus:
        if (m_mode == ResolvingStyle) {
            if (context.inRightmostCompound) {
                m_elementStyle->setAffectedByFocus();
            } else {
                m_elementStyle->setUnique();
                element.setChildrenOrSiblingsAffectedByFocus();
            }
        }
        return matchesFocusPseudoClass(element);
    case CSSSelector::PseudoHover:
        if (m_mode == ResolvingStyle) {
            if (context.inRightmostCompound) {
                m_elementStyle->setAffectedByHover();
            } else {
                m_elementStyle->setUnique();
                element.setChildrenOrSiblingsAffectedByHover();
            }
        }
        if (!shouldMatchHoverOrActive(context))
            return false;
        if (InspectorInstrumentation::forcePseudoState(&element, CSSSelector::PseudoHover))
            return true;
        return element.hovered();
    case CSSSelector::PseudoActive:
        if (m_mode == ResolvingStyle) {
            if (context.inRightmostCompound) {
                m_elementStyle->setAffectedByActive();
            } else {
                m_elementStyle->setUnique();
                element.setChildrenOrSiblingsAffectedByActive();
            }
        }
        if (!shouldMatchHoverOrActive(context))
            return false;
        if (InspectorInstrumentation::forcePseudoState(&element, CSSSelector::PseudoActive))
            return true;
        return element.active();
    case CSSSelector::PseudoEnabled:
        return element.matchesEnabledPseudoClass();
    case CSSSelector::PseudoFullPageMedia:
        return element.document().isMediaDocument();
    case CSSSelector::PseudoDefault:
        return element.matchesDefaultPseudoClass();
    case CSSSelector::PseudoDisabled:
        return element.isDisabledFormControl();
    case CSSSelector::PseudoReadOnly:
        return element.matchesReadOnlyPseudoClass();
    case CSSSelector::PseudoReadWrite:
        return element.matchesReadWritePseudoClass();
    case CSSSelector::PseudoOptional:
        return element.isOptionalFormControl();
    case CSSSelector::PseudoRequired:
        return element.isRequiredFormControl();
    case CSSSelector::PseudoValid:
        if (m_mode == ResolvingStyle)
            element.document().setContainsValidityStyleRules();
        return element.matchesValidityPseudoClasses() && element.isValidElement();
    case CSSSelector::PseudoInvalid:
        if (m_mode == ResolvingStyle)
            element.document().setContainsValidityStyleRules();
        return element.matchesValidityPseudoClasses() && !element.isValidElement();
    case CSSSelector::PseudoChecked:
        {
            if (isHTMLInputElement(element)) {
                HTMLInputElement& inputElement = toHTMLInputElement(element);
                // Even though WinIE allows checked and indeterminate to
                // co-exist, the CSS selector spec says that you can't be
                // both checked and indeterminate. We will behave like WinIE
                // behind the scenes and just obey the CSS spec here in the
                // test for matching the pseudo.
                if (inputElement.shouldAppearChecked() && !inputElement.shouldAppearIndeterminate())
                    return true;
            } else if (isHTMLOptionElement(element) && toHTMLOptionElement(element).selected()) {
                return true;
            }
            break;
        }
    case CSSSelector::PseudoIndeterminate:
        return element.shouldAppearIndeterminate();
    case CSSSelector::PseudoRoot:
        return element == element.document().documentElement();
    case CSSSelector::PseudoLang:
        {
            AtomicString value;
            if (element.isVTTElement())
                value = toVTTElement(element).language();
            else
                value = element.computeInheritedLanguage();
            const AtomicString& argument = selector.argument();
            if (value.isEmpty() || !value.startsWith(argument, TextCaseASCIIInsensitive))
                break;
            if (value.length() != argument.length() && value[argument.length()] != '-')
                break;
            return true;
        }
    case CSSSelector::PseudoFullScreen:
        // While a Document is in the fullscreen state, and the document's current fullscreen
        // element is an element in the document, the 'full-screen' pseudoclass applies to
        // that element. Also, an <iframe>, <object> or <embed> element whose child browsing
        // context's Document is in the fullscreen state has the 'full-screen' pseudoclass applied.
        if (isHTMLFrameElementBase(element) && element.containsFullScreenElement())
            return true;
        return Fullscreen::isActiveFullScreenElement(element);
    case CSSSelector::PseudoFullScreenAncestor:
        return element.containsFullScreenElement();
    case CSSSelector::PseudoInRange:
        if (m_mode == ResolvingStyle)
            element.document().setContainsValidityStyleRules();
        return element.isInRange();
    case CSSSelector::PseudoOutOfRange:
        if (m_mode == ResolvingStyle)
            element.document().setContainsValidityStyleRules();
        return element.isOutOfRange();
    case CSSSelector::PseudoFutureCue:
        return element.isVTTElement() && !toVTTElement(element).isPastNode();
    case CSSSelector::PseudoPastCue:
        return element.isVTTElement() && toVTTElement(element).isPastNode();
    case CSSSelector::PseudoScope:
        if (m_mode == SharingRules)
            return true;
        if (context.scope == &element.document())
            return element == element.document().documentElement();
        return context.scope == &element;
    case CSSSelector::PseudoUnresolved:
        return element.isUnresolvedV0CustomElement();
    case CSSSelector::PseudoDefined:
        DCHECK(RuntimeEnabledFeatures::customElementsV1Enabled());
        return element.isDefined();
    case CSSSelector::PseudoHost:
    case CSSSelector::PseudoHostContext:
        return checkPseudoHost(context, result);
    case CSSSelector::PseudoSpatialNavigationFocus:
        return m_isUARule && matchesSpatialNavigationFocusPseudoClass(element);
    case CSSSelector::PseudoListBox:
        return m_isUARule && matchesListBoxPseudoClass(element);
    case CSSSelector::PseudoHostHasAppearance:
        if (!m_isUARule)
            return false;
        if (ShadowRoot* root = element.containingShadowRoot()) {
            if (root->type() != ShadowRootType::UserAgent)
                return false;
            const ComputedStyle* style = root->host().computedStyle();
            return style && style->hasAppearance();
        }
        return false;
    case CSSSelector::PseudoWindowInactive:
        if (!context.hasSelectionPseudo)
            return false;
        return !element.document().page()->focusController().isActive();
    case CSSSelector::PseudoHorizontal:
    case CSSSelector::PseudoVertical:
    case CSSSelector::PseudoDecrement:
    case CSSSelector::PseudoIncrement:
    case CSSSelector::PseudoStart:
    case CSSSelector::PseudoEnd:
    case CSSSelector::PseudoDoubleButton:
    case CSSSelector::PseudoSingleButton:
    case CSSSelector::PseudoNoButton:
    case CSSSelector::PseudoCornerPresent:
        return false;
    case CSSSelector::PseudoUnknown:
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return false;
}

bool SelectorChecker::checkPseudoElement(const SelectorCheckingContext& context, MatchResult& result) const
{
    const CSSSelector& selector = *context.selector;
    Element& element = *context.element;

    switch (selector.getPseudoType()) {
    case CSSSelector::PseudoCue:
        {
            SelectorCheckingContext subContext(context);
            subContext.isSubSelector = true;
            subContext.scope = nullptr;
            subContext.treatShadowHostAsNormalScope = false;

            for (subContext.selector = selector.selectorList()->first(); subContext.selector; subContext.selector = CSSSelectorList::next(*subContext.selector)) {
                if (match(subContext))
                    return true;
            }
            return false;
        }
    case CSSSelector::PseudoWebKitCustomElement:
        {
            if (ShadowRoot* root = element.containingShadowRoot())
                return root->type() == ShadowRootType::UserAgent && element.shadowPseudoId() == selector.value();
            return false;
        }
    case CSSSelector::PseudoBlinkInternalElement:
        if (!m_isUARule)
            return false;
        if (ShadowRoot* root = element.containingShadowRoot())
            return root->type() == ShadowRootType::UserAgent && element.shadowPseudoId() == selector.value();
        return false;
    case CSSSelector::PseudoSlotted:
        {
            SelectorCheckingContext subContext(context);
            subContext.isSubSelector = true;
            subContext.scope = nullptr;
            subContext.treatShadowHostAsNormalScope = false;

            // ::slotted() only allows one compound selector.
            ASSERT(selector.selectorList()->first());
            ASSERT(!CSSSelectorList::next(*selector.selectorList()->first()));
            subContext.selector = selector.selectorList()->first();
            return match(subContext);
        }
    case CSSSelector::PseudoContent:
        return element.isInShadowTree() && element.isInsertionPoint();
    case CSSSelector::PseudoShadow:
        return element.isInShadowTree() && context.previousElement;
    default:
        if (m_mode == SharingRules)
            return true;
        ASSERT(m_mode != QueryingRules);
        result.dynamicPseudo = CSSSelector::pseudoId(selector.getPseudoType());
        ASSERT(result.dynamicPseudo != PseudoIdNone);
        return true;
    }
}

bool SelectorChecker::checkPseudoHost(const SelectorCheckingContext& context, MatchResult& result) const
{
    const CSSSelector& selector = *context.selector;
    Element& element = *context.element;

    if (m_mode == SharingRules)
        return true;
    // :host only matches a shadow host when :host is in a shadow tree of the shadow host.
    if (!context.scope)
        return false;
    const ContainerNode* shadowHost = context.scope->shadowHost();
    if (!shadowHost || shadowHost != element)
        return false;
    ASSERT(element.shadow());

    // For the case with no parameters, i.e. just :host.
    if (!selector.selectorList())
        return true;

    SelectorCheckingContext subContext(context);
    subContext.isSubSelector = true;

    bool matched = false;
    unsigned maxSpecificity = 0;

    // If one of simple selectors matches an element, returns SelectorMatches. Just "OR".
    for (subContext.selector = selector.selectorList()->first(); subContext.selector; subContext.selector = CSSSelectorList::next(*subContext.selector)) {
        subContext.treatShadowHostAsNormalScope = true;
        subContext.scope = context.scope;
        // Use FlatTreeTraversal to traverse a composed ancestor list of a given element.
        Element* nextElement = &element;
        SelectorCheckingContext hostContext(subContext);
        do {
            MatchResult subResult;
            hostContext.element = nextElement;
            if (match(hostContext, subResult)) {
                matched = true;
                // Consider div:host(div:host(div:host(div:host...))).
                maxSpecificity = std::max(maxSpecificity, hostContext.selector->specificity() + subResult.specificity);
                break;
            }
            hostContext.treatShadowHostAsNormalScope = false;
            hostContext.scope = nullptr;

            if (selector.getPseudoType() == CSSSelector::PseudoHost)
                break;

            hostContext.inRightmostCompound = false;
            nextElement = FlatTreeTraversal::parentElement(*nextElement);
        } while (nextElement);
    }
    if (matched) {
        result.specificity += maxSpecificity;
        return true;
    }

    // FIXME: this was a fallthrough condition.
    return false;
}

bool SelectorChecker::checkScrollbarPseudoClass(const SelectorCheckingContext& context, MatchResult& result) const
{
    const CSSSelector& selector = *context.selector;

    if (selector.getPseudoType() == CSSSelector::PseudoNot)
        return checkPseudoNot(context, result);

    // FIXME: This is a temporary hack for resizers and scrollbar corners. Eventually :window-inactive should become a real
    // pseudo class and just apply to everything.
    if (selector.getPseudoType() == CSSSelector::PseudoWindowInactive)
        return !context.element->document().page()->focusController().isActive();

    if (!m_scrollbar)
        return false;

    switch (selector.getPseudoType()) {
    case CSSSelector::PseudoEnabled:
        return m_scrollbar->enabled();
    case CSSSelector::PseudoDisabled:
        return !m_scrollbar->enabled();
    case CSSSelector::PseudoHover:
        {
            ScrollbarPart hoveredPart = m_scrollbar->hoveredPart();
            if (m_scrollbarPart == ScrollbarBGPart)
                return hoveredPart != NoPart;
            if (m_scrollbarPart == TrackBGPart)
                return hoveredPart == BackTrackPart || hoveredPart == ForwardTrackPart || hoveredPart == ThumbPart;
            return m_scrollbarPart == hoveredPart;
        }
    case CSSSelector::PseudoActive:
        {
            ScrollbarPart pressedPart = m_scrollbar->pressedPart();
            if (m_scrollbarPart == ScrollbarBGPart)
                return pressedPart != NoPart;
            if (m_scrollbarPart == TrackBGPart)
                return pressedPart == BackTrackPart || pressedPart == ForwardTrackPart || pressedPart == ThumbPart;
            return m_scrollbarPart == pressedPart;
        }
    case CSSSelector::PseudoHorizontal:
        return m_scrollbar->orientation() == HorizontalScrollbar;
    case CSSSelector::PseudoVertical:
        return m_scrollbar->orientation() == VerticalScrollbar;
    case CSSSelector::PseudoDecrement:
        return m_scrollbarPart == BackButtonStartPart || m_scrollbarPart == BackButtonEndPart || m_scrollbarPart == BackTrackPart;
    case CSSSelector::PseudoIncrement:
        return m_scrollbarPart == ForwardButtonStartPart || m_scrollbarPart == ForwardButtonEndPart || m_scrollbarPart == ForwardTrackPart;
    case CSSSelector::PseudoStart:
        return m_scrollbarPart == BackButtonStartPart || m_scrollbarPart == ForwardButtonStartPart || m_scrollbarPart == BackTrackPart;
    case CSSSelector::PseudoEnd:
        return m_scrollbarPart == BackButtonEndPart || m_scrollbarPart == ForwardButtonEndPart || m_scrollbarPart == ForwardTrackPart;
    case CSSSelector::PseudoDoubleButton:
        {
            WebScrollbarButtonsPlacement buttonsPlacement = m_scrollbar->theme().buttonsPlacement();
            if (m_scrollbarPart == BackButtonStartPart || m_scrollbarPart == ForwardButtonStartPart || m_scrollbarPart == BackTrackPart)
                return buttonsPlacement == WebScrollbarButtonsPlacementDoubleStart || buttonsPlacement == WebScrollbarButtonsPlacementDoubleBoth;
            if (m_scrollbarPart == BackButtonEndPart || m_scrollbarPart == ForwardButtonEndPart || m_scrollbarPart == ForwardTrackPart)
                return buttonsPlacement == WebScrollbarButtonsPlacementDoubleEnd || buttonsPlacement == WebScrollbarButtonsPlacementDoubleBoth;
            return false;
        }
    case CSSSelector::PseudoSingleButton:
        {
            WebScrollbarButtonsPlacement buttonsPlacement = m_scrollbar->theme().buttonsPlacement();
            if (m_scrollbarPart == BackButtonStartPart || m_scrollbarPart == ForwardButtonEndPart || m_scrollbarPart == BackTrackPart || m_scrollbarPart == ForwardTrackPart)
                return buttonsPlacement == WebScrollbarButtonsPlacementSingle;
            return false;
        }
    case CSSSelector::PseudoNoButton:
        {
            WebScrollbarButtonsPlacement buttonsPlacement = m_scrollbar->theme().buttonsPlacement();
            if (m_scrollbarPart == BackTrackPart)
                return buttonsPlacement == WebScrollbarButtonsPlacementNone || buttonsPlacement == WebScrollbarButtonsPlacementDoubleEnd;
            if (m_scrollbarPart == ForwardTrackPart)
                return buttonsPlacement == WebScrollbarButtonsPlacementNone || buttonsPlacement == WebScrollbarButtonsPlacementDoubleStart;
            return false;
        }
    case CSSSelector::PseudoCornerPresent:
        return m_scrollbar->getScrollableArea() && m_scrollbar->getScrollableArea()->isScrollCornerVisible();
    default:
        return false;
    }
}

bool SelectorChecker::matchesFocusPseudoClass(const Element& element)
{
    if (InspectorInstrumentation::forcePseudoState(const_cast<Element*>(&element), CSSSelector::PseudoFocus))
        return true;
    return element.focused() && isFrameFocused(element);
}

} // namespace blink
