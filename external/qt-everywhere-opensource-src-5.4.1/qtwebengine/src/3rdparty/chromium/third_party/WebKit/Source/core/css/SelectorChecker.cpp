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

#include "config.h"
#include "core/css/SelectorChecker.h"

#include "core/HTMLNames.h"
#include "core/css/CSSSelectorList.h"
#include "core/css/SiblingTraversalStrategies.h"
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/FullscreenElementStack.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/track/vtt/VTTElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/page/FocusController.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/RenderScrollbar.h"
#include "core/rendering/style/RenderStyle.h"
#include "platform/scroll/ScrollableArea.h"
#include "platform/scroll/ScrollbarTheme.h"

namespace WebCore {

using namespace HTMLNames;

SelectorChecker::SelectorChecker(Document& document, Mode mode)
    : m_strictParsing(!document.inQuirksMode())
    , m_documentIsHTML(document.isHTMLDocument())
    , m_mode(mode)
{
}

static bool matchesCustomPseudoElement(const Element* element, const CSSSelector& selector)
{
    ShadowRoot* root = element->containingShadowRoot();
    if (!root || root->type() != ShadowRoot::UserAgentShadowRoot)
        return false;

    if (element->shadowPseudoId() != selector.value())
        return false;

    return true;
}

Element* SelectorChecker::parentElement(const SelectorCheckingContext& context, bool allowToCrossBoundary) const
{
    // CrossesBoundary means we don't care any context.scope. So we can walk up from a shadow root to its shadow host.
    if (allowToCrossBoundary)
        return context.element->parentOrShadowHostElement();

    // If context.scope is a shadow root, we should walk up to its shadow host.
    if ((context.behaviorAtBoundary & SelectorChecker::ScopeIsShadowRoot) && context.scope == context.element->containingShadowRoot())
        return context.element->parentOrShadowHostElement();

    if ((context.behaviorAtBoundary & SelectorChecker::BoundaryBehaviorMask) != SelectorChecker::StaysWithinTreeScope)
        return context.element->parentElement();

    // If context.scope is some element in some shadow tree and querySelector initialized the context,
    // e.g. shadowRoot.querySelector(':host *'),
    // (a) context.element has the same treescope as context.scope, need to walk up to its shadow host.
    // (b) Otherwise, should not walk up from a shadow root to a shadow host.
    if (context.scope && context.scope->treeScope() == context.element->treeScope())
        return context.element->parentOrShadowHostElement();

    return context.element->parentElement();
}

bool SelectorChecker::scopeContainsLastMatchedElement(const SelectorCheckingContext& context) const
{
    if (!(context.behaviorAtBoundary & SelectorChecker::ScopeContainsLastMatchedElement))
        return true;

    ASSERT(context.scope);
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

// Recursive check of selectors and combinators
// It can return 4 different values:
// * SelectorMatches          - the selector matches the element e
// * SelectorFailsLocally     - the selector fails for the element e
// * SelectorFailsAllSiblings - the selector fails for e and any sibling of e
// * SelectorFailsCompletely  - the selector fails for e and any sibling or ancestor of e
template<typename SiblingTraversalStrategy>
SelectorChecker::Match SelectorChecker::match(const SelectorCheckingContext& context, const SiblingTraversalStrategy& siblingTraversalStrategy, MatchResult* result) const
{
    // first selector has to match
    unsigned specificity = 0;
    if (!checkOne(context, siblingTraversalStrategy, &specificity))
        return SelectorFailsLocally;

    if (context.selector->match() == CSSSelector::PseudoElement) {
        if (context.selector->isCustomPseudoElement()) {
            if (!matchesCustomPseudoElement(context.element, *context.selector))
                return SelectorFailsLocally;
        } else if (context.selector->isContentPseudoElement()) {
            if (!context.element->isInShadowTree() || !context.element->isInsertionPoint())
                return SelectorFailsLocally;
        } else if (context.selector->isShadowPseudoElement()) {
            if (!context.element->isInShadowTree() || !context.previousElement)
                return SelectorFailsCompletely;
        } else {
            if ((!context.elementStyle && m_mode == ResolvingStyle) || m_mode == QueryingRules)
                return SelectorFailsLocally;

            PseudoId pseudoId = CSSSelector::pseudoId(context.selector->pseudoType());
            if (pseudoId == FIRST_LETTER)
                context.element->document().styleEngine()->setUsesFirstLetterRules(true);
            if (pseudoId != NOPSEUDO && m_mode != SharingRules && result)
                result->dynamicPseudo = pseudoId;
        }
    }

    // Prepare next selector
    if (context.selector->isLastInTagHistory()) {
        if (scopeContainsLastMatchedElement(context)) {
            if (result)
                result->specificity += specificity;
            return SelectorMatches;
        }
        return SelectorFailsLocally;
    }

    Match match;
    if (context.selector->relation() != CSSSelector::SubSelector) {
        // Abort if the next selector would exceed the scope.
        if (nextSelectorExceedsScope(context))
            return SelectorFailsCompletely;

        // Bail-out if this selector is irrelevant for the pseudoId
        if (context.pseudoId != NOPSEUDO && (!result || context.pseudoId != result->dynamicPseudo))
            return SelectorFailsCompletely;

        if (result) {
            TemporaryChange<PseudoId> dynamicPseudoScope(result->dynamicPseudo, NOPSEUDO);
            match = matchForRelation(context, siblingTraversalStrategy, result);
        } else {
            return matchForRelation(context, siblingTraversalStrategy, 0);
        }
    } else {
        match = matchForSubSelector(context, siblingTraversalStrategy, result);
    }
    if (match != SelectorMatches || !result)
        return match;

    result->specificity += specificity;
    return SelectorMatches;
}

static inline SelectorChecker::SelectorCheckingContext prepareNextContextForRelation(const SelectorChecker::SelectorCheckingContext& context)
{
    SelectorChecker::SelectorCheckingContext nextContext(context);
    ASSERT(context.selector->tagHistory());
    nextContext.selector = context.selector->tagHistory();
    return nextContext;
}

static inline bool isAuthorShadowRoot(const Node* node)
{
    return node && node->isShadowRoot() && toShadowRoot(node)->type() == ShadowRoot::AuthorShadowRoot;
}

template<typename SiblingTraversalStrategy>
SelectorChecker::Match SelectorChecker::matchForSubSelector(const SelectorCheckingContext& context, const SiblingTraversalStrategy& siblingTraversalStrategy, MatchResult* result) const
{
    SelectorCheckingContext nextContext = prepareNextContextForRelation(context);

    PseudoId dynamicPseudo = result ? result->dynamicPseudo : NOPSEUDO;
    // a selector is invalid if something follows a pseudo-element
    // We make an exception for scrollbar pseudo elements and allow a set of pseudo classes (but nothing else)
    // to follow the pseudo elements.
    nextContext.hasScrollbarPseudo = dynamicPseudo != NOPSEUDO && (context.scrollbar || dynamicPseudo == SCROLLBAR_CORNER || dynamicPseudo == RESIZER);
    nextContext.hasSelectionPseudo = dynamicPseudo == SELECTION;
    if ((context.elementStyle || m_mode == CollectingCSSRules || m_mode == CollectingStyleRules || m_mode == QueryingRules) && dynamicPseudo != NOPSEUDO
        && !nextContext.hasSelectionPseudo
        && !(nextContext.hasScrollbarPseudo && nextContext.selector->match() == CSSSelector::PseudoClass))
        return SelectorFailsCompletely;

    nextContext.isSubSelector = true;
    return match(nextContext, siblingTraversalStrategy, result);
}

static bool selectorMatchesShadowRoot(const CSSSelector* selector)
{
    return selector && selector->isShadowPseudoElement();
}

template<typename SiblingTraversalStrategy>
SelectorChecker::Match SelectorChecker::matchForPseudoShadow(const ContainerNode* node, const SelectorCheckingContext& context, const SiblingTraversalStrategy& siblingTraversalStrategy, MatchResult* result) const
{
    if (!isAuthorShadowRoot(node))
        return SelectorFailsCompletely;
    return match(context, siblingTraversalStrategy, result);
}

template<typename SiblingTraversalStrategy>
SelectorChecker::Match SelectorChecker::matchForRelation(const SelectorCheckingContext& context, const SiblingTraversalStrategy& siblingTraversalStrategy, MatchResult* result) const
{
    SelectorCheckingContext nextContext = prepareNextContextForRelation(context);
    nextContext.previousElement = context.element;

    CSSSelector::Relation relation = context.selector->relation();

    // Disable :visited matching when we see the first link or try to match anything else than an ancestors.
    if (!context.isSubSelector && (context.element->isLink() || (relation != CSSSelector::Descendant && relation != CSSSelector::Child)))
        nextContext.visitedMatchType = VisitedMatchDisabled;

    nextContext.pseudoId = NOPSEUDO;

    switch (relation) {
    case CSSSelector::Descendant:
        if (context.selector->relationIsAffectedByPseudoContent()) {
            for (Element* element = context.element; element; element = element->parentElement()) {
                if (matchForShadowDistributed(element, siblingTraversalStrategy, nextContext, result) == SelectorMatches)
                    return SelectorMatches;
            }
            return SelectorFailsCompletely;
        }
        nextContext.isSubSelector = false;
        nextContext.elementStyle = 0;

        if (selectorMatchesShadowRoot(nextContext.selector))
            return matchForPseudoShadow(context.element->containingShadowRoot(), nextContext, siblingTraversalStrategy, result);

        for (nextContext.element = parentElement(context); nextContext.element; nextContext.element = parentElement(nextContext)) {
            Match match = this->match(nextContext, siblingTraversalStrategy, result);
            if (match == SelectorMatches || match == SelectorFailsCompletely)
                return match;
            if (nextSelectorExceedsScope(nextContext))
                return SelectorFailsCompletely;
        }
        return SelectorFailsCompletely;
    case CSSSelector::Child:
        {
            if (context.selector->relationIsAffectedByPseudoContent())
                return matchForShadowDistributed(context.element, siblingTraversalStrategy, nextContext, result);

            nextContext.isSubSelector = false;
            nextContext.elementStyle = 0;

            if (selectorMatchesShadowRoot(nextContext.selector))
                return matchForPseudoShadow(context.element->parentNode(), nextContext, siblingTraversalStrategy, result);

            nextContext.element = parentElement(context);
            if (!nextContext.element)
                return SelectorFailsCompletely;
            return match(nextContext, siblingTraversalStrategy, result);
        }
    case CSSSelector::DirectAdjacent:
        // Shadow roots can't have sibling elements
        if (selectorMatchesShadowRoot(nextContext.selector))
            return SelectorFailsCompletely;

        if (m_mode == ResolvingStyle) {
            if (ContainerNode* parent = context.element->parentElementOrShadowRoot())
                parent->setChildrenAffectedByDirectAdjacentRules();
        }
        nextContext.element = ElementTraversal::previousSibling(*context.element);
        if (!nextContext.element)
            return SelectorFailsAllSiblings;
        nextContext.isSubSelector = false;
        nextContext.elementStyle = 0;
        return match(nextContext, siblingTraversalStrategy, result);

    case CSSSelector::IndirectAdjacent:
        // Shadow roots can't have sibling elements
        if (selectorMatchesShadowRoot(nextContext.selector))
            return SelectorFailsCompletely;

        if (m_mode == ResolvingStyle) {
            if (ContainerNode* parent = context.element->parentElementOrShadowRoot())
                parent->setChildrenAffectedByIndirectAdjacentRules();
        }
        nextContext.element = ElementTraversal::previousSibling(*context.element);
        nextContext.isSubSelector = false;
        nextContext.elementStyle = 0;
        for (; nextContext.element; nextContext.element = ElementTraversal::previousSibling(*nextContext.element)) {
            Match match = this->match(nextContext, siblingTraversalStrategy, result);
            if (match == SelectorMatches || match == SelectorFailsAllSiblings || match == SelectorFailsCompletely)
                return match;
        };
        return SelectorFailsAllSiblings;

    case CSSSelector::ShadowPseudo:
        {
            // If we're in the same tree-scope as the scoping element, then following a shadow descendant combinator would escape that and thus the scope.
            if (context.scope && context.scope->shadowHost() && context.scope->shadowHost()->treeScope() == context.element->treeScope() && (context.behaviorAtBoundary & BoundaryBehaviorMask) != StaysWithinTreeScope)
                return SelectorFailsCompletely;

            Element* shadowHost = context.element->shadowHost();
            if (!shadowHost)
                return SelectorFailsCompletely;
            nextContext.element = shadowHost;
            nextContext.isSubSelector = false;
            nextContext.elementStyle = 0;
            return this->match(nextContext, siblingTraversalStrategy, result);
        }

    case CSSSelector::ShadowDeep:
        {
            nextContext.isSubSelector = false;
            nextContext.elementStyle = 0;
            for (nextContext.element = parentElement(context, true); nextContext.element; nextContext.element = parentElement(nextContext, true)) {
                Match match = this->match(nextContext, siblingTraversalStrategy, result);
                if (match == SelectorMatches || match == SelectorFailsCompletely)
                    return match;
                if (nextSelectorExceedsScope(nextContext))
                    return SelectorFailsCompletely;
            }
            return SelectorFailsCompletely;
        }

    case CSSSelector::SubSelector:
        ASSERT_NOT_REACHED();
    }

    ASSERT_NOT_REACHED();
    return SelectorFailsCompletely;
}

template<typename SiblingTraversalStrategy>
SelectorChecker::Match SelectorChecker::matchForShadowDistributed(const Element* element, const SiblingTraversalStrategy& siblingTraversalStrategy, SelectorCheckingContext& nextContext, MatchResult* result) const
{
    ASSERT(element);
    WillBeHeapVector<RawPtrWillBeMember<InsertionPoint>, 8> insertionPoints;

    const ContainerNode* scope = nextContext.scope;
    BehaviorAtBoundary behaviorAtBoundary = nextContext.behaviorAtBoundary;

    collectDestinationInsertionPoints(*element, insertionPoints);
    for (size_t i = 0; i < insertionPoints.size(); ++i) {
        nextContext.element = insertionPoints[i];

        // If a given scope is a shadow host of an insertion point but behaviorAtBoundary doesn't have ScopeIsShadowRoot,
        // we need to update behaviorAtBoundary to make selectors like ":host > ::content" work correctly.
        if (m_mode == SharingRules) {
            nextContext.behaviorAtBoundary = static_cast<BehaviorAtBoundary>(behaviorAtBoundary | ScopeIsShadowRoot);
            nextContext.scope = insertionPoints[i]->containingShadowRoot();
        } else if (scope == insertionPoints[i]->containingShadowRoot() && !(behaviorAtBoundary & ScopeIsShadowRoot)) {
            nextContext.behaviorAtBoundary = static_cast<BehaviorAtBoundary>(behaviorAtBoundary | ScopeIsShadowRoot);
        } else {
            nextContext.behaviorAtBoundary = behaviorAtBoundary;
        }

        nextContext.isSubSelector = false;
        nextContext.elementStyle = 0;
        if (match(nextContext, siblingTraversalStrategy, result) == SelectorMatches)
            return SelectorMatches;
    }
    return SelectorFailsLocally;
}

template<typename CharType>
static inline bool containsHTMLSpaceTemplate(const CharType* string, unsigned length)
{
    for (unsigned i = 0; i < length; ++i)
        if (isHTMLSpace<CharType>(string[i]))
            return true;
    return false;
}

static inline bool containsHTMLSpace(const AtomicString& string)
{
    if (LIKELY(string.is8Bit()))
        return containsHTMLSpaceTemplate<LChar>(string.characters8(), string.length());
    return containsHTMLSpaceTemplate<UChar>(string.characters16(), string.length());
}

static bool attributeValueMatches(const Attribute& attributeItem, CSSSelector::Match match, const AtomicString& selectorValue, bool caseSensitive)
{
    const AtomicString& value = attributeItem.value();
    if (value.isNull())
        return false;

    switch (match) {
    case CSSSelector::Exact:
        if (caseSensitive ? selectorValue != value : !equalIgnoringCase(selectorValue, value))
            return false;
        break;
    case CSSSelector::List:
        {
            // Ignore empty selectors or selectors containing HTML spaces
            if (selectorValue.isEmpty() || containsHTMLSpace(selectorValue))
                return false;

            unsigned startSearchAt = 0;
            while (true) {
                size_t foundPos = value.find(selectorValue, startSearchAt, caseSensitive);
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
            break;
        }
    case CSSSelector::Contain:
        if (!value.contains(selectorValue, caseSensitive) || selectorValue.isEmpty())
            return false;
        break;
    case CSSSelector::Begin:
        if (!value.startsWith(selectorValue, caseSensitive) || selectorValue.isEmpty())
            return false;
        break;
    case CSSSelector::End:
        if (!value.endsWith(selectorValue, caseSensitive) || selectorValue.isEmpty())
            return false;
        break;
    case CSSSelector::Hyphen:
        if (value.length() < selectorValue.length())
            return false;
        if (!value.startsWith(selectorValue, caseSensitive))
            return false;
        // It they start the same, check for exact match or following '-':
        if (value.length() != selectorValue.length() && value[selectorValue.length()] != '-')
            return false;
        break;
    case CSSSelector::PseudoClass:
    case CSSSelector::PseudoElement:
    default:
        break;
    }

    return true;
}

static bool anyAttributeMatches(Element& element, CSSSelector::Match match, const CSSSelector& selector)
{
    const QualifiedName& selectorAttr = selector.attribute();
    ASSERT(selectorAttr.localName() != starAtom); // Should not be possible from the CSS grammar.

    // Synchronize the attribute in case it is lazy-computed.
    // Currently all lazy properties have a null namespace, so only pass localName().
    element.synchronizeAttribute(selectorAttr.localName());

    if (!element.hasAttributesWithoutUpdate())
        return false;

    const AtomicString& selectorValue =  selector.value();

    AttributeCollection attributes = element.attributes();
    AttributeCollection::const_iterator end = attributes.end();
    for (AttributeCollection::const_iterator it = attributes.begin(); it != end; ++it) {
        const Attribute& attributeItem = *it;

        if (!attributeItem.matches(selectorAttr))
            continue;

        if (attributeValueMatches(attributeItem, match, selectorValue, true))
            return true;

        // Case sensitivity for attribute matching is looser than hasAttribute or
        // Element::shouldIgnoreAttributeCase() for now. Unclear if that's correct.
        bool caseSensitive = !element.document().isHTMLDocument() || HTMLDocument::isCaseSensitiveAttribute(selectorAttr);

        // If case-insensitive, re-check, and count if result differs.
        // See http://code.google.com/p/chromium/issues/detail?id=327060
        if (!caseSensitive && attributeValueMatches(attributeItem, match, selectorValue, false)) {
            UseCounter::count(element.document(), UseCounter::CaseInsensitiveAttrSelectorMatch);
            return true;
        }
    }

    return false;
}

template<typename SiblingTraversalStrategy>
bool SelectorChecker::checkOne(const SelectorCheckingContext& context, const SiblingTraversalStrategy& siblingTraversalStrategy, unsigned* specificity) const
{
    ASSERT(context.element);
    Element& element = *context.element;
    ASSERT(context.selector);
    const CSSSelector& selector = *context.selector;

    bool elementIsHostInItsShadowTree = isHostInItsShadowTree(element, context.behaviorAtBoundary, context.scope);

    // Only :host and :ancestor should match the host: http://drafts.csswg.org/css-scoping/#host-element
    if (elementIsHostInItsShadowTree && !selector.isHostPseudoClass()
        && !(context.behaviorAtBoundary & TreatShadowHostAsNormalScope))
        return false;

    if (selector.match() == CSSSelector::Tag)
        return SelectorChecker::tagMatches(element, selector.tagQName());

    if (selector.match() == CSSSelector::Class)
        return element.hasClass() && element.classNames().contains(selector.value());

    if (selector.match() == CSSSelector::Id)
        return element.hasID() && element.idForStyleResolution() == selector.value();

    if (selector.isAttributeSelector())
        return anyAttributeMatches(element, selector.match(), selector);

    if (selector.match() == CSSSelector::PseudoClass) {
        // Handle :not up front.
        if (selector.pseudoType() == CSSSelector::PseudoNot) {
            SelectorCheckingContext subContext(context);
            subContext.isSubSelector = true;
            ASSERT(selector.selectorList());
            for (subContext.selector = selector.selectorList()->first(); subContext.selector; subContext.selector = subContext.selector->tagHistory()) {
                // :not cannot nest. I don't really know why this is a
                // restriction in CSS3, but it is, so let's honor it.
                // the parser enforces that this never occurs
                ASSERT(subContext.selector->pseudoType() != CSSSelector::PseudoNot);
                // We select between :visited and :link when applying. We don't know which one applied (or not) yet.
                if (subContext.selector->pseudoType() == CSSSelector::PseudoVisited || (subContext.selector->pseudoType() == CSSSelector::PseudoLink && subContext.visitedMatchType == VisitedMatchEnabled))
                    return true;
                // context.scope is not available if m_mode == SharingRules.
                // We cannot determine whether :host or :scope matches a given element or not.
                if (m_mode == SharingRules && (subContext.selector->isHostPseudoClass() || subContext.selector->pseudoType() == CSSSelector::PseudoScope))
                    return true;
                if (!checkOne(subContext, DOMSiblingTraversalStrategy()))
                    return true;
            }
        } else if (context.hasScrollbarPseudo) {
            // CSS scrollbars match a specific subset of pseudo classes, and they have specialized rules for each
            // (since there are no elements involved).
            return checkScrollbarPseudoClass(context, &element.document(), selector);
        } else if (context.hasSelectionPseudo) {
            if (selector.pseudoType() == CSSSelector::PseudoWindowInactive)
                return !element.document().page()->focusController().isActive();
        }

        // Normal element pseudo class checking.
        switch (selector.pseudoType()) {
            // Pseudo classes:
        case CSSSelector::PseudoNot:
            break; // Already handled up above.
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
                    if (context.elementStyle)
                        context.elementStyle->setEmptyState(result);
                    else if (element.renderStyle() && (element.document().styleEngine()->usesSiblingRules() || element.renderStyle()->unique()))
                        element.renderStyle()->setEmptyState(result);
                }
                return result;
            }
        case CSSSelector::PseudoFirstChild:
            // first-child matches the first child that is an element
            if (ContainerNode* parent = element.parentElementOrDocumentFragment()) {
                bool result = siblingTraversalStrategy.isFirstChild(element);
                if (m_mode == ResolvingStyle) {
                    RenderStyle* childStyle = context.elementStyle ? context.elementStyle : element.renderStyle();
                    parent->setChildrenAffectedByFirstChildRules();
                    if (result && childStyle)
                        childStyle->setFirstChildState();
                }
                return result;
            }
            break;
        case CSSSelector::PseudoFirstOfType:
            // first-of-type matches the first element of its type
            if (ContainerNode* parent = element.parentElementOrDocumentFragment()) {
                bool result = siblingTraversalStrategy.isFirstOfType(element, element.tagQName());
                if (m_mode == ResolvingStyle)
                    parent->setChildrenAffectedByForwardPositionalRules();
                return result;
            }
            break;
        case CSSSelector::PseudoLastChild:
            // last-child matches the last child that is an element
            if (ContainerNode* parent = element.parentElementOrDocumentFragment()) {
                bool result = parent->isFinishedParsingChildren() && siblingTraversalStrategy.isLastChild(element);
                if (m_mode == ResolvingStyle) {
                    RenderStyle* childStyle = context.elementStyle ? context.elementStyle : element.renderStyle();
                    parent->setChildrenAffectedByLastChildRules();
                    if (result && childStyle)
                        childStyle->setLastChildState();
                }
                return result;
            }
            break;
        case CSSSelector::PseudoLastOfType:
            // last-of-type matches the last element of its type
            if (ContainerNode* parent = element.parentElementOrDocumentFragment()) {
                if (m_mode == ResolvingStyle)
                    parent->setChildrenAffectedByBackwardPositionalRules();
                if (!parent->isFinishedParsingChildren())
                    return false;
                return siblingTraversalStrategy.isLastOfType(element, element.tagQName());
            }
            break;
        case CSSSelector::PseudoOnlyChild:
            if (ContainerNode* parent = element.parentElementOrDocumentFragment()) {
                bool firstChild = siblingTraversalStrategy.isFirstChild(element);
                bool onlyChild = firstChild && parent->isFinishedParsingChildren() && siblingTraversalStrategy.isLastChild(element);
                if (m_mode == ResolvingStyle) {
                    RenderStyle* childStyle = context.elementStyle ? context.elementStyle : element.renderStyle();
                    parent->setChildrenAffectedByFirstChildRules();
                    parent->setChildrenAffectedByLastChildRules();
                    if (firstChild && childStyle)
                        childStyle->setFirstChildState();
                    if (onlyChild && childStyle)
                        childStyle->setLastChildState();
                }
                return onlyChild;
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
                return siblingTraversalStrategy.isFirstOfType(element, element.tagQName()) && siblingTraversalStrategy.isLastOfType(element, element.tagQName());
            }
            break;
        case CSSSelector::PseudoNthChild:
            if (!selector.parseNth())
                break;
            if (ContainerNode* parent = element.parentElementOrDocumentFragment()) {
                int count = 1 + siblingTraversalStrategy.countElementsBefore(element);
                if (m_mode == ResolvingStyle) {
                    RenderStyle* childStyle = context.elementStyle ? context.elementStyle : element.renderStyle();
                    if (childStyle)
                        childStyle->setUnique();
                    parent->setChildrenAffectedByForwardPositionalRules();
                }

                if (selector.matchNth(count))
                    return true;
            }
            break;
        case CSSSelector::PseudoNthOfType:
            if (!selector.parseNth())
                break;
            if (ContainerNode* parent = element.parentElementOrDocumentFragment()) {
                int count = 1 + siblingTraversalStrategy.countElementsOfTypeBefore(element, element.tagQName());
                if (m_mode == ResolvingStyle)
                    parent->setChildrenAffectedByForwardPositionalRules();

                if (selector.matchNth(count))
                    return true;
            }
            break;
        case CSSSelector::PseudoNthLastChild:
            if (!selector.parseNth())
                break;
            if (ContainerNode* parent = element.parentElementOrDocumentFragment()) {
                if (m_mode == ResolvingStyle)
                    parent->setChildrenAffectedByBackwardPositionalRules();
                if (!parent->isFinishedParsingChildren())
                    return false;
                int count = 1 + siblingTraversalStrategy.countElementsAfter(element);
                if (selector.matchNth(count))
                    return true;
            }
            break;
        case CSSSelector::PseudoNthLastOfType:
            if (!selector.parseNth())
                break;
            if (ContainerNode* parent = element.parentElementOrDocumentFragment()) {
                if (m_mode == ResolvingStyle)
                    parent->setChildrenAffectedByBackwardPositionalRules();
                if (!parent->isFinishedParsingChildren())
                    return false;

                int count = 1 + siblingTraversalStrategy.countElementsOfTypeAfter(element, element.tagQName());
                if (selector.matchNth(count))
                    return true;
            }
            break;
        case CSSSelector::PseudoTarget:
            if (element == element.document().cssTarget())
                return true;
            break;
        case CSSSelector::PseudoAny:
            {
                SelectorCheckingContext subContext(context);
                subContext.isSubSelector = true;
                ASSERT(selector.selectorList());
                for (subContext.selector = selector.selectorList()->first(); subContext.selector; subContext.selector = CSSSelectorList::next(*subContext.selector)) {
                    if (match(subContext, siblingTraversalStrategy) == SelectorMatches)
                        return true;
                }
            }
            break;
        case CSSSelector::PseudoAutofill:
            if (!element.isFormControlElement())
                break;
            return toHTMLFormControlElement(element).isAutofilled();
        case CSSSelector::PseudoAnyLink:
        case CSSSelector::PseudoLink:
            // :visited and :link matches are separated later when applying the style. Here both classes match all links...
            return element.isLink();
        case CSSSelector::PseudoVisited:
            // ...except if :visited matching is disabled for ancestor/sibling matching.
            return element.isLink() && context.visitedMatchType == VisitedMatchEnabled;
        case CSSSelector::PseudoDrag:
            if (m_mode == ResolvingStyle) {
                if (context.elementStyle)
                    context.elementStyle->setAffectedByDrag();
                else
                    element.setChildrenOrSiblingsAffectedByDrag();
            }
            if (element.renderer() && element.renderer()->isDragging())
                return true;
            break;
        case CSSSelector::PseudoFocus:
            if (m_mode == ResolvingStyle) {
                if (context.elementStyle)
                    context.elementStyle->setAffectedByFocus();
                else
                    element.setChildrenOrSiblingsAffectedByFocus();
            }
            return matchesFocusPseudoClass(element);
        case CSSSelector::PseudoHover:
            // If we're in quirks mode, then hover should never match anchors with no
            // href and *:hover should not match anything. This is important for sites like wsj.com.
            if (m_strictParsing || context.isSubSelector || element.isLink()) {
                if (m_mode == ResolvingStyle) {
                    if (context.elementStyle)
                        context.elementStyle->setAffectedByHover();
                    else
                        element.setChildrenOrSiblingsAffectedByHover();
                }
                if (element.hovered() || InspectorInstrumentation::forcePseudoState(&element, CSSSelector::PseudoHover))
                    return true;
            }
            break;
        case CSSSelector::PseudoActive:
            // If we're in quirks mode, then :active should never match anchors with no
            // href and *:active should not match anything.
            if (m_strictParsing || context.isSubSelector || element.isLink()) {
                if (m_mode == ResolvingStyle) {
                    if (context.elementStyle)
                        context.elementStyle->setAffectedByActive();
                    else
                        element.setChildrenOrSiblingsAffectedByActive();
                }
                if (element.active() || InspectorInstrumentation::forcePseudoState(&element, CSSSelector::PseudoActive))
                    return true;
            }
            break;
        case CSSSelector::PseudoEnabled:
            if (element.isFormControlElement() || isHTMLOptionElement(element) || isHTMLOptGroupElement(element))
                return !element.isDisabledFormControl();
            break;
        case CSSSelector::PseudoFullPageMedia:
            return element.document().isMediaDocument();
            break;
        case CSSSelector::PseudoDefault:
            return element.isDefaultButtonForForm();
        case CSSSelector::PseudoDisabled:
            if (element.isFormControlElement() || isHTMLOptionElement(element) || isHTMLOptGroupElement(element))
                return element.isDisabledFormControl();
            break;
        case CSSSelector::PseudoReadOnly:
            return element.matchesReadOnlyPseudoClass();
        case CSSSelector::PseudoReadWrite:
            return element.matchesReadWritePseudoClass();
        case CSSSelector::PseudoOptional:
            return element.isOptionalFormControl();
        case CSSSelector::PseudoRequired:
            return element.isRequiredFormControl();
        case CSSSelector::PseudoValid:
            element.document().setContainsValidityStyleRules();
            return element.willValidate() && element.isValidFormControlElement();
        case CSSSelector::PseudoInvalid:
            element.document().setContainsValidityStyleRules();
            return element.willValidate() && !element.isValidFormControlElement();
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
                } else if (isHTMLOptionElement(element) && toHTMLOptionElement(element).selected())
                    return true;
                break;
            }
        case CSSSelector::PseudoIndeterminate:
            return element.shouldAppearIndeterminate();
        case CSSSelector::PseudoRoot:
            if (element == element.document().documentElement())
                return true;
            break;
        case CSSSelector::PseudoLang:
            {
                AtomicString value;
                if (element.isVTTElement())
                    value = toVTTElement(element).language();
                else
                    value = element.computeInheritedLanguage();
                const AtomicString& argument = selector.argument();
                if (value.isEmpty() || !value.startsWith(argument, false))
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
            if (FullscreenElementStack* fullscreen = FullscreenElementStack::fromIfExists(element.document())) {
                if (!fullscreen->webkitIsFullScreen())
                    return false;
                return element == fullscreen->webkitCurrentFullScreenElement();
            }
            return false;
        case CSSSelector::PseudoFullScreenAncestor:
            return element.containsFullScreenElement();
        case CSSSelector::PseudoFullScreenDocument:
            // While a Document is in the fullscreen state, the 'full-screen-document' pseudoclass applies
            // to all elements of that Document.
            if (!FullscreenElementStack::isFullScreen(element.document()))
                return false;
            return true;
        case CSSSelector::PseudoInRange:
            element.document().setContainsValidityStyleRules();
            return element.isInRange();
        case CSSSelector::PseudoOutOfRange:
            element.document().setContainsValidityStyleRules();
            return element.isOutOfRange();
        case CSSSelector::PseudoFutureCue:
            return (element.isVTTElement() && !toVTTElement(element).isPastNode());
        case CSSSelector::PseudoPastCue:
            return (element.isVTTElement() && toVTTElement(element).isPastNode());

        case CSSSelector::PseudoScope:
            {
                if (m_mode == SharingRules)
                    return true;
                const Node* contextualReferenceNode = !context.scope ? element.document().documentElement() : context.scope;
                if (element == contextualReferenceNode)
                    return true;
                break;
            }

        case CSSSelector::PseudoUnresolved:
            if (element.isUnresolvedCustomElement())
                return true;
            break;

        case CSSSelector::PseudoHost:
        case CSSSelector::PseudoHostContext:
            {
                if (m_mode == SharingRules)
                    return true;
                // :host only matches a shadow host when :host is in a shadow tree of the shadow host.
                if (!context.scope)
                    return false;
                const ContainerNode* shadowHost = context.scope->shadowHost();
                if (!shadowHost || shadowHost != element)
                    return false;
                ASSERT(element.shadow());

                // For empty parameter case, i.e. just :host or :host().
                if (!selector.selectorList()) // Use *'s specificity. So just 0.
                    return true;

                SelectorCheckingContext subContext(context);
                subContext.isSubSelector = true;

                bool matched = false;
                unsigned maxSpecificity = 0;

                // If one of simple selectors matches an element, returns SelectorMatches. Just "OR".
                for (subContext.selector = selector.selectorList()->first(); subContext.selector; subContext.selector = CSSSelectorList::next(*subContext.selector)) {
                    subContext.behaviorAtBoundary = ScopeIsShadowHostInPseudoHostParameter;
                    subContext.scope = context.scope;
                    // Use NodeRenderingTraversal to traverse a composed ancestor list of a given element.
                    Element* nextElement = &element;
                    SelectorCheckingContext hostContext(subContext);
                    do {
                        MatchResult subResult;
                        hostContext.element = nextElement;
                        if (match(hostContext, siblingTraversalStrategy, &subResult) == SelectorMatches) {
                            matched = true;
                            // Consider div:host(div:host(div:host(div:host...))).
                            maxSpecificity = std::max(maxSpecificity, hostContext.selector->specificity() + subResult.specificity);
                            break;
                        }
                        hostContext.behaviorAtBoundary = DoesNotCrossBoundary;
                        hostContext.scope = 0;

                        if (selector.pseudoType() == CSSSelector::PseudoHost)
                            break;

                        hostContext.elementStyle = 0;
                        nextElement = NodeRenderingTraversal::parentElement(nextElement);
                    } while (nextElement);
                }
                if (matched) {
                    if (specificity)
                        *specificity = maxSpecificity;
                    return true;
                }
            }
            break;

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
        case CSSSelector::PseudoNotParsed:
        default:
            ASSERT_NOT_REACHED();
            break;
        }
        return false;
    } else if (selector.match() == CSSSelector::PseudoElement && selector.pseudoType() == CSSSelector::PseudoCue) {
        SelectorCheckingContext subContext(context);
        subContext.isSubSelector = true;
        subContext.behaviorAtBoundary = StaysWithinTreeScope;

        const CSSSelector* contextSelector = context.selector;
        ASSERT(contextSelector);
        for (subContext.selector = contextSelector->selectorList()->first(); subContext.selector; subContext.selector = CSSSelectorList::next(*subContext.selector)) {
            if (match(subContext, siblingTraversalStrategy) == SelectorMatches)
                return true;
        }
        return false;
    }
    // ### add the rest of the checks...
    return true;
}

bool SelectorChecker::checkScrollbarPseudoClass(const SelectorCheckingContext& context, Document* document, const CSSSelector& selector) const
{
    RenderScrollbar* scrollbar = context.scrollbar;
    ScrollbarPart part = context.scrollbarPart;

    // FIXME: This is a temporary hack for resizers and scrollbar corners. Eventually :window-inactive should become a real
    // pseudo class and just apply to everything.
    if (selector.pseudoType() == CSSSelector::PseudoWindowInactive)
        return !document->page()->focusController().isActive();

    if (!scrollbar)
        return false;

    ASSERT(selector.match() == CSSSelector::PseudoClass);
    switch (selector.pseudoType()) {
    case CSSSelector::PseudoEnabled:
        return scrollbar->enabled();
    case CSSSelector::PseudoDisabled:
        return !scrollbar->enabled();
    case CSSSelector::PseudoHover:
        {
            ScrollbarPart hoveredPart = scrollbar->hoveredPart();
            if (part == ScrollbarBGPart)
                return hoveredPart != NoPart;
            if (part == TrackBGPart)
                return hoveredPart == BackTrackPart || hoveredPart == ForwardTrackPart || hoveredPart == ThumbPart;
            return part == hoveredPart;
        }
    case CSSSelector::PseudoActive:
        {
            ScrollbarPart pressedPart = scrollbar->pressedPart();
            if (part == ScrollbarBGPart)
                return pressedPart != NoPart;
            if (part == TrackBGPart)
                return pressedPart == BackTrackPart || pressedPart == ForwardTrackPart || pressedPart == ThumbPart;
            return part == pressedPart;
        }
    case CSSSelector::PseudoHorizontal:
        return scrollbar->orientation() == HorizontalScrollbar;
    case CSSSelector::PseudoVertical:
        return scrollbar->orientation() == VerticalScrollbar;
    case CSSSelector::PseudoDecrement:
        return part == BackButtonStartPart || part == BackButtonEndPart || part == BackTrackPart;
    case CSSSelector::PseudoIncrement:
        return part == ForwardButtonStartPart || part == ForwardButtonEndPart || part == ForwardTrackPart;
    case CSSSelector::PseudoStart:
        return part == BackButtonStartPart || part == ForwardButtonStartPart || part == BackTrackPart;
    case CSSSelector::PseudoEnd:
        return part == BackButtonEndPart || part == ForwardButtonEndPart || part == ForwardTrackPart;
    case CSSSelector::PseudoDoubleButton:
        {
            ScrollbarButtonsPlacement buttonsPlacement = scrollbar->theme()->buttonsPlacement();
            if (part == BackButtonStartPart || part == ForwardButtonStartPart || part == BackTrackPart)
                return buttonsPlacement == ScrollbarButtonsDoubleStart || buttonsPlacement == ScrollbarButtonsDoubleBoth;
            if (part == BackButtonEndPart || part == ForwardButtonEndPart || part == ForwardTrackPart)
                return buttonsPlacement == ScrollbarButtonsDoubleEnd || buttonsPlacement == ScrollbarButtonsDoubleBoth;
            return false;
        }
    case CSSSelector::PseudoSingleButton:
        {
            ScrollbarButtonsPlacement buttonsPlacement = scrollbar->theme()->buttonsPlacement();
            if (part == BackButtonStartPart || part == ForwardButtonEndPart || part == BackTrackPart || part == ForwardTrackPart)
                return buttonsPlacement == ScrollbarButtonsSingle;
            return false;
        }
    case CSSSelector::PseudoNoButton:
        {
            ScrollbarButtonsPlacement buttonsPlacement = scrollbar->theme()->buttonsPlacement();
            if (part == BackTrackPart)
                return buttonsPlacement == ScrollbarButtonsNone || buttonsPlacement == ScrollbarButtonsDoubleEnd;
            if (part == ForwardTrackPart)
                return buttonsPlacement == ScrollbarButtonsNone || buttonsPlacement == ScrollbarButtonsDoubleStart;
            return false;
        }
    case CSSSelector::PseudoCornerPresent:
        return scrollbar->scrollableArea()->isScrollCornerVisible();
    default:
        return false;
    }
}

unsigned SelectorChecker::determineLinkMatchType(const CSSSelector& selector)
{
    unsigned linkMatchType = MatchAll;

    // Statically determine if this selector will match a link in visited, unvisited or any state, or never.
    // :visited never matches other elements than the innermost link element.
    for (const CSSSelector* current = &selector; current; current = current->tagHistory()) {
        switch (current->pseudoType()) {
        case CSSSelector::PseudoNot:
            {
                // :not(:visited) is equivalent to :link. Parser enforces that :not can't nest.
                ASSERT(current->selectorList());
                for (const CSSSelector* subSelector = current->selectorList()->first(); subSelector; subSelector = subSelector->tagHistory()) {
                    CSSSelector::PseudoType subType = subSelector->pseudoType();
                    if (subType == CSSSelector::PseudoVisited)
                        linkMatchType &= ~SelectorChecker::MatchVisited;
                    else if (subType == CSSSelector::PseudoLink)
                        linkMatchType &= ~SelectorChecker::MatchLink;
                }
            }
            break;
        case CSSSelector::PseudoLink:
            linkMatchType &= ~SelectorChecker::MatchVisited;
            break;
        case CSSSelector::PseudoVisited:
            linkMatchType &= ~SelectorChecker::MatchLink;
            break;
        default:
            // We don't support :link and :visited inside :-webkit-any.
            break;
        }
        CSSSelector::Relation relation = current->relation();
        if (relation == CSSSelector::SubSelector)
            continue;
        if (relation != CSSSelector::Descendant && relation != CSSSelector::Child)
            return linkMatchType;
        if (linkMatchType != MatchAll)
            return linkMatchType;
    }
    return linkMatchType;
}

bool SelectorChecker::isFrameFocused(const Element& element)
{
    return element.document().frame() && element.document().frame()->selection().isFocusedAndActive();
}

bool SelectorChecker::matchesFocusPseudoClass(const Element& element)
{
    if (InspectorInstrumentation::forcePseudoState(const_cast<Element*>(&element), CSSSelector::PseudoFocus))
        return true;
    return element.focused() && isFrameFocused(element);
}

template
SelectorChecker::Match SelectorChecker::match(const SelectorCheckingContext&, const DOMSiblingTraversalStrategy&, MatchResult*) const;

template
SelectorChecker::Match SelectorChecker::match(const SelectorCheckingContext&, const ShadowDOMSiblingTraversalStrategy&, MatchResult*) const;

}
