/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "core/css/RuleFeature.h"

#include "core/HTMLNames.h"
#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSSelector.h"
#include "core/css/CSSSelectorList.h"
#include "core/css/CSSValueList.h"
#include "core/css/RuleSet.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/invalidation/InvalidationSet.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "wtf/BitVector.h"

namespace blink {

namespace {

#if ENABLE(ASSERT)

bool supportsInvalidation(CSSSelector::MatchType match)
{
    switch (match) {
    case CSSSelector::Tag:
    case CSSSelector::Id:
    case CSSSelector::Class:
    case CSSSelector::AttributeExact:
    case CSSSelector::AttributeSet:
    case CSSSelector::AttributeHyphen:
    case CSSSelector::AttributeList:
    case CSSSelector::AttributeContain:
    case CSSSelector::AttributeBegin:
    case CSSSelector::AttributeEnd:
        return true;
    case CSSSelector::Unknown:
    case CSSSelector::PagePseudoClass:
        // These should not appear in StyleRule selectors.
        ASSERT_NOT_REACHED();
        return false;
    default:
        // New match type added. Figure out if it needs a subtree invalidation or not.
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool supportsInvalidation(CSSSelector::PseudoType type)
{
    switch (type) {
    case CSSSelector::PseudoEmpty:
    case CSSSelector::PseudoFirstChild:
    case CSSSelector::PseudoFirstOfType:
    case CSSSelector::PseudoLastChild:
    case CSSSelector::PseudoLastOfType:
    case CSSSelector::PseudoOnlyChild:
    case CSSSelector::PseudoOnlyOfType:
    case CSSSelector::PseudoNthChild:
    case CSSSelector::PseudoNthOfType:
    case CSSSelector::PseudoNthLastChild:
    case CSSSelector::PseudoNthLastOfType:
    case CSSSelector::PseudoLink:
    case CSSSelector::PseudoVisited:
    case CSSSelector::PseudoAny:
    case CSSSelector::PseudoAnyLink:
    case CSSSelector::PseudoAutofill:
    case CSSSelector::PseudoHover:
    case CSSSelector::PseudoDrag:
    case CSSSelector::PseudoFocus:
    case CSSSelector::PseudoActive:
    case CSSSelector::PseudoChecked:
    case CSSSelector::PseudoEnabled:
    case CSSSelector::PseudoFullPageMedia:
    case CSSSelector::PseudoDefault:
    case CSSSelector::PseudoDisabled:
    case CSSSelector::PseudoOptional:
    case CSSSelector::PseudoPlaceholderShown:
    case CSSSelector::PseudoRequired:
    case CSSSelector::PseudoReadOnly:
    case CSSSelector::PseudoReadWrite:
    case CSSSelector::PseudoValid:
    case CSSSelector::PseudoInvalid:
    case CSSSelector::PseudoIndeterminate:
    case CSSSelector::PseudoTarget:
    case CSSSelector::PseudoBefore:
    case CSSSelector::PseudoAfter:
    case CSSSelector::PseudoBackdrop:
    case CSSSelector::PseudoLang:
    case CSSSelector::PseudoNot:
    case CSSSelector::PseudoResizer:
    case CSSSelector::PseudoRoot:
    case CSSSelector::PseudoScope:
    case CSSSelector::PseudoScrollbar:
    case CSSSelector::PseudoScrollbarButton:
    case CSSSelector::PseudoScrollbarCorner:
    case CSSSelector::PseudoScrollbarThumb:
    case CSSSelector::PseudoScrollbarTrack:
    case CSSSelector::PseudoScrollbarTrackPiece:
    case CSSSelector::PseudoWindowInactive:
    case CSSSelector::PseudoSelection:
    case CSSSelector::PseudoCornerPresent:
    case CSSSelector::PseudoDecrement:
    case CSSSelector::PseudoIncrement:
    case CSSSelector::PseudoHorizontal:
    case CSSSelector::PseudoVertical:
    case CSSSelector::PseudoStart:
    case CSSSelector::PseudoEnd:
    case CSSSelector::PseudoDoubleButton:
    case CSSSelector::PseudoSingleButton:
    case CSSSelector::PseudoNoButton:
    case CSSSelector::PseudoFullScreen:
    case CSSSelector::PseudoFullScreenAncestor:
    case CSSSelector::PseudoInRange:
    case CSSSelector::PseudoOutOfRange:
    case CSSSelector::PseudoWebKitCustomElement:
    case CSSSelector::PseudoBlinkInternalElement:
    case CSSSelector::PseudoCue:
    case CSSSelector::PseudoFutureCue:
    case CSSSelector::PseudoPastCue:
    case CSSSelector::PseudoUnresolved:
    case CSSSelector::PseudoDefined:
    case CSSSelector::PseudoContent:
    case CSSSelector::PseudoHost:
    case CSSSelector::PseudoShadow:
    case CSSSelector::PseudoSpatialNavigationFocus:
    case CSSSelector::PseudoListBox:
    case CSSSelector::PseudoHostHasAppearance:
    case CSSSelector::PseudoSlotted:
        return true;
    case CSSSelector::PseudoUnknown:
    case CSSSelector::PseudoLeftPage:
    case CSSSelector::PseudoRightPage:
    case CSSSelector::PseudoFirstPage:
        // These should not appear in StyleRule selectors.
        ASSERT_NOT_REACHED();
        return false;
    default:
        // New pseudo type added. Figure out if it needs a subtree invalidation or not.
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool supportsInvalidationWithSelectorList(CSSSelector::PseudoType pseudo)
{
    return pseudo == CSSSelector::PseudoAny
        || pseudo == CSSSelector::PseudoCue
        || pseudo == CSSSelector::PseudoHost
        || pseudo == CSSSelector::PseudoHostContext
        || pseudo == CSSSelector::PseudoNot
        || pseudo == CSSSelector::PseudoSlotted;
}

#endif // ENABLE(ASSERT)

bool requiresSubtreeInvalidation(const CSSSelector& selector)
{
    if (selector.match() != CSSSelector::PseudoElement && selector.match() != CSSSelector::PseudoClass) {
        ASSERT(supportsInvalidation(selector.match()));
        return false;
    }

    switch (selector.getPseudoType()) {
    case CSSSelector::PseudoFirstLine:
    case CSSSelector::PseudoFirstLetter:
        // FIXME: Most pseudo classes/elements above can be supported and moved
        // to assertSupportedPseudo(). Move on a case-by-case basis. If they
        // require subtree invalidation, document why.
    case CSSSelector::PseudoHostContext:
        // :host-context matches a shadow host, yet the simple selectors inside
        // :host-context matches an ancestor of the shadow host.
        return true;
    default:
        ASSERT(supportsInvalidation(selector.getPseudoType()));
        return false;
    }
}

template<class Map>
InvalidationSet& ensureInvalidationSet(Map& map, const typename Map::KeyType& key, InvalidationType type)
{
    typename Map::AddResult addResult = map.add(key, nullptr);
    if (addResult.isNewEntry) {
        if (type == InvalidateDescendants)
            addResult.storedValue->value = DescendantInvalidationSet::create();
        else
            addResult.storedValue->value = SiblingInvalidationSet::create(nullptr);
        return *addResult.storedValue->value;
    }
    if (addResult.storedValue->value->type() == type)
        return *addResult.storedValue->value;

    if (type == InvalidateDescendants)
        return toSiblingInvalidationSet(addResult.storedValue->value.get())->ensureDescendants();

    addResult.storedValue->value = SiblingInvalidationSet::create(toDescendantInvalidationSet(addResult.storedValue->value.get()));
    return *addResult.storedValue->value;
}

void extractInvalidationSets(InvalidationSet* invalidationSet, DescendantInvalidationSet*& descendants, SiblingInvalidationSet*& siblings)
{
    if (invalidationSet->type() == InvalidateDescendants) {
        descendants = toDescendantInvalidationSet(invalidationSet);
        siblings = nullptr;
        return;
    }

    siblings = toSiblingInvalidationSet(invalidationSet);
    descendants = siblings->descendants();
}

} // anonymous namespace

RuleFeature::RuleFeature(StyleRule* rule, unsigned selectorIndex, bool hasDocumentSecurityOrigin)
    : rule(rule)
    , selectorIndex(selectorIndex)
    , hasDocumentSecurityOrigin(hasDocumentSecurityOrigin)
{
}

DEFINE_TRACE(RuleFeature)
{
    visitor->trace(rule);
}

RuleFeatureSet::RuleFeatureSet()
{
}

RuleFeatureSet::~RuleFeatureSet()
{
}

ALWAYS_INLINE InvalidationSet& RuleFeatureSet::ensureClassInvalidationSet(const AtomicString& className, InvalidationType type)
{
    return ensureInvalidationSet(m_classInvalidationSets, className, type);
}

ALWAYS_INLINE InvalidationSet& RuleFeatureSet::ensureAttributeInvalidationSet(const AtomicString& attributeName, InvalidationType type)
{
    return ensureInvalidationSet(m_attributeInvalidationSets, attributeName, type);
}

ALWAYS_INLINE InvalidationSet& RuleFeatureSet::ensureIdInvalidationSet(const AtomicString& id, InvalidationType type)
{
    return ensureInvalidationSet(m_idInvalidationSets, id, type);
}

ALWAYS_INLINE InvalidationSet& RuleFeatureSet::ensurePseudoInvalidationSet(CSSSelector::PseudoType pseudoType, InvalidationType type)
{
    return ensureInvalidationSet(m_pseudoInvalidationSets, pseudoType, type);
}

bool RuleFeatureSet::extractInvalidationSetFeature(const CSSSelector& selector, InvalidationSetFeatures& features)
{
    if (selector.match() == CSSSelector::Tag && selector.tagQName().localName() != starAtom) {
        features.tagName = selector.tagQName().localName();
        return true;
    }
    if (selector.match() == CSSSelector::Id) {
        features.id = selector.value();
        return true;
    }
    if (selector.match() == CSSSelector::Class) {
        features.classes.append(selector.value());
        return true;
    }
    if (selector.isAttributeSelector()) {
        features.attributes.append(selector.attribute().localName());
        return true;
    }
    if (selector.getPseudoType() == CSSSelector::PseudoWebKitCustomElement || selector.getPseudoType() == CSSSelector::PseudoBlinkInternalElement) {
        features.customPseudoElement = true;
        return true;
    }
    if (selector.getPseudoType() == CSSSelector::PseudoBefore || selector.getPseudoType() == CSSSelector::PseudoAfter)
        features.hasBeforeOrAfter = true;
    return false;
}

InvalidationSet* RuleFeatureSet::invalidationSetForSelector(const CSSSelector& selector, InvalidationType type)
{
    if (selector.match() == CSSSelector::Class)
        return &ensureClassInvalidationSet(selector.value(), type);
    if (selector.isAttributeSelector())
        return &ensureAttributeInvalidationSet(selector.attribute().localName(), type);
    if (selector.match() == CSSSelector::Id)
        return &ensureIdInvalidationSet(selector.value(), type);
    if (selector.match() == CSSSelector::PseudoClass) {
        switch (selector.getPseudoType()) {
        case CSSSelector::PseudoEmpty:
        case CSSSelector::PseudoLink:
        case CSSSelector::PseudoVisited:
        case CSSSelector::PseudoAnyLink:
        case CSSSelector::PseudoAutofill:
        case CSSSelector::PseudoHover:
        case CSSSelector::PseudoDrag:
        case CSSSelector::PseudoFocus:
        case CSSSelector::PseudoActive:
        case CSSSelector::PseudoChecked:
        case CSSSelector::PseudoEnabled:
        case CSSSelector::PseudoDefault:
        case CSSSelector::PseudoDisabled:
        case CSSSelector::PseudoOptional:
        case CSSSelector::PseudoPlaceholderShown:
        case CSSSelector::PseudoRequired:
        case CSSSelector::PseudoReadOnly:
        case CSSSelector::PseudoReadWrite:
        case CSSSelector::PseudoValid:
        case CSSSelector::PseudoInvalid:
        case CSSSelector::PseudoIndeterminate:
        case CSSSelector::PseudoTarget:
        case CSSSelector::PseudoLang:
        case CSSSelector::PseudoFullScreen:
        case CSSSelector::PseudoFullScreenAncestor:
        case CSSSelector::PseudoInRange:
        case CSSSelector::PseudoOutOfRange:
        case CSSSelector::PseudoUnresolved:
        case CSSSelector::PseudoDefined:
            return &ensurePseudoInvalidationSet(selector.getPseudoType(), type);
        default:
            break;
        }
    }
    return nullptr;
}

// Given a rule, update the descendant invalidation sets for the features found
// in its selector. The first step is to extract the features from the rightmost
// compound selector (extractInvalidationSetFeatures). Secondly, add those features
// to the invalidation sets for the features found in the other compound selectors
// (addFeaturesToInvalidationSets). If we find a feature in the right-most compound
// selector that requires a subtree recalc, we addFeaturesToInvalidationSets for the
// rightmost compound selector as well.

void RuleFeatureSet::updateInvalidationSets(const RuleData& ruleData)
{
    InvalidationSetFeatures features;
    auto result = extractInvalidationSetFeatures(ruleData.selector(), features, Subject);

    if (result.first) {
        features.forceSubtree = result.second == ForceSubtree;
        addFeaturesToInvalidationSets(result.first, features.adjacent ? &features : nullptr, features);
    }

    // If any ::before and ::after rules specify 'content: attr(...)', we
    // need to create invalidation sets for those attributes.
    if (features.hasBeforeOrAfter)
        updateInvalidationSetsForContentAttribute(ruleData);
}

void RuleFeatureSet::updateInvalidationSetsForContentAttribute(const RuleData& ruleData)
{
    const StylePropertySet& propertySet = ruleData.rule()->properties();

    int propertyIndex = propertySet.findPropertyIndex(CSSPropertyContent);

    if (propertyIndex == -1)
        return;

    StylePropertySet::PropertyReference contentProperty = propertySet.propertyAt(propertyIndex);
    CSSValue* contentValue = contentProperty.value();

    if (!contentValue->isValueList())
        return;

    for (auto& item : toCSSValueList(*contentValue)) {
        if (!item->isFunctionValue())
            continue;
        const CSSFunctionValue* functionValue = toCSSFunctionValue(item.get());
        if (functionValue->functionType() != CSSValueAttr)
            continue;
        ensureAttributeInvalidationSet(AtomicString(toCSSCustomIdentValue(functionValue->item(0)).value()), InvalidateDescendants).setInvalidatesSelf();
    }
}

std::pair<const CSSSelector*, RuleFeatureSet::UseFeaturesType>
RuleFeatureSet::extractInvalidationSetFeatures(const CSSSelector& selector, InvalidationSetFeatures& features, PositionType position, CSSSelector::PseudoType pseudo)
{
    bool foundFeatures = false;
    for (const CSSSelector* current = &selector; current; current = current->tagHistory()) {
        if (pseudo != CSSSelector::PseudoNot)
            foundFeatures |= extractInvalidationSetFeature(*current, features);
        // Initialize the entry in the invalidation set map, if supported.
        if (InvalidationSet* invalidationSet = invalidationSetForSelector(*current, InvalidateDescendants)) {
            if (position == Subject)
                invalidationSet->setInvalidatesSelf();
        } else {
            if (requiresSubtreeInvalidation(*current)) {
                // Fall back to use subtree invalidations, even for features in the
                // rightmost compound selector. Returning the start &selector here
                // will make addFeaturesToInvalidationSets start marking invalidation
                // sets for subtree recalc for features in the rightmost compound
                // selector.
                return std::make_pair(&selector, ForceSubtree);
            }
            if (const CSSSelectorList* selectorList = current->selectorList()) {
                if (current->getPseudoType() == CSSSelector::PseudoSlotted) {
                    ASSERT(position == Subject);
                    features.invalidatesSlotted = true;
                }
                ASSERT(supportsInvalidationWithSelectorList(current->getPseudoType()));
                const CSSSelector* subSelector = selectorList->first();
                bool allSubSelectorsHaveFeatures = !!subSelector;
                for (; subSelector; subSelector = CSSSelectorList::next(*subSelector)) {
                    auto result = extractInvalidationSetFeatures(*subSelector, features, position, current->getPseudoType());
                    if (result.first) {
                        // A non-null selector return means the sub-selector contained a
                        // selector which requiresSubtreeInvalidation(). Return the rightmost
                        // selector to mark for subtree recalcs like above.
                        return std::make_pair(&selector, ForceSubtree);
                    }
                    allSubSelectorsHaveFeatures &= result.second == UseFeatures;
                }
                foundFeatures |= allSubSelectorsHaveFeatures;
            }
        }

        if (current->relation() == CSSSelector::SubSelector)
            continue;

        features.treeBoundaryCrossing = current->isShadowSelector();
        if (current->relationIsAffectedByPseudoContent()) {
            features.contentPseudoCrossing = true;
            features.insertionPointCrossing = true;
        }
        features.adjacent = current->isAdjacentSelector();
        if (current->relation() == CSSSelector::DirectAdjacent)
            features.maxDirectAdjacentSelectors = 1;
        return std::make_pair(current->tagHistory(), foundFeatures ? UseFeatures : ForceSubtree);
    }
    return std::make_pair(nullptr,  foundFeatures ? UseFeatures : ForceSubtree);
}

// Add features extracted from the rightmost compound selector to descendant invalidation
// sets for features found in other compound selectors.
//
// We use descendant invalidation for descendants, sibling invalidation for siblings and their subtrees.
//
// As we encounter a descendant type of combinator, the features only need to be checked
// against descendants in the same subtree only. features.adjacent is set to false, and
// we start adding features to the descendant invalidation set.

void RuleFeatureSet::addFeaturesToInvalidationSet(InvalidationSet& invalidationSet, const InvalidationSetFeatures& features)
{
    if (features.treeBoundaryCrossing)
        invalidationSet.setTreeBoundaryCrossing();
    if (features.insertionPointCrossing)
        invalidationSet.setInsertionPointCrossing();
    if (features.invalidatesSlotted)
        invalidationSet.setInvalidatesSlotted();
    if (features.forceSubtree)
        invalidationSet.setWholeSubtreeInvalid();
    if (features.contentPseudoCrossing || features.forceSubtree)
        return;

    if (!features.id.isEmpty())
        invalidationSet.addId(features.id);
    if (!features.tagName.isEmpty())
        invalidationSet.addTagName(features.tagName);
    for (const auto& className : features.classes)
        invalidationSet.addClass(className);
    for (const auto& attribute : features.attributes)
        invalidationSet.addAttribute(attribute);
    if (features.customPseudoElement)
        invalidationSet.setCustomPseudoInvalid();
}

// selector is the selector immediately to the left of the rightmost combinator.
// siblingFeatures is null if selector is not immediately to the left of a sibling combinator.
// descendantFeatures has the features of the rightmost compound selector.
void RuleFeatureSet::addFeaturesToInvalidationSets(const CSSSelector* selector, InvalidationSetFeatures* siblingFeatures, InvalidationSetFeatures& descendantFeatures)
{
    const CSSSelector* lastCompoundSelectorInAdjacentChain = selector;

    // We set siblingFeatures to &localFeatures if we find a rightmost sibling combinator.
    InvalidationSetFeatures localFeatures;

    bool universalCompound = true;

    for (const CSSSelector* current = selector; current; current = current->tagHistory()) {
        InvalidationType type = siblingFeatures ? InvalidateSiblings : InvalidateDescendants;
        if (InvalidationSet* invalidationSet = invalidationSetForSelector(*current, type)) {
            if (current->match() != CSSSelector::PseudoClass)
                universalCompound = false;
            if (siblingFeatures) {
                SiblingInvalidationSet* siblingInvalidationSet = toSiblingInvalidationSet(invalidationSet);
                siblingInvalidationSet->updateMaxDirectAdjacentSelectors(siblingFeatures->maxDirectAdjacentSelectors);

                addFeaturesToInvalidationSet(*invalidationSet, *siblingFeatures);
                if (siblingFeatures == &descendantFeatures)
                    siblingInvalidationSet->setInvalidatesSelf();
                else
                    addFeaturesToInvalidationSet(siblingInvalidationSet->ensureSiblingDescendants(), descendantFeatures);
            } else {
                addFeaturesToInvalidationSet(*invalidationSet, descendantFeatures);
            }
        } else {
            if (current->isHostPseudoClass())
                descendantFeatures.treeBoundaryCrossing = true;
            if (current->isInsertionPointCrossing())
                descendantFeatures.insertionPointCrossing = true;
            if (const CSSSelectorList* selectorList = current->selectorList()) {
                ASSERT(supportsInvalidationWithSelectorList(current->getPseudoType()));
                for (const CSSSelector* subSelector = selectorList->first(); subSelector; subSelector = CSSSelectorList::next(*subSelector))
                    addFeaturesToInvalidationSets(subSelector, siblingFeatures, descendantFeatures);
            }
        }

        if (current->relation() == CSSSelector::SubSelector)
            continue;

        if (universalCompound && siblingFeatures)
            addFeaturesToUniversalSiblingInvalidationSet(*siblingFeatures, descendantFeatures);
        universalCompound = true;

        if (current->relationIsAffectedByPseudoContent() || current->relation() == CSSSelector::ShadowSlot) {
            descendantFeatures.insertionPointCrossing = true;
            descendantFeatures.contentPseudoCrossing = true;
        }
        if (current->isShadowSelector())
            descendantFeatures.treeBoundaryCrossing = true;
        if (!current->isAdjacentSelector()) {
            lastCompoundSelectorInAdjacentChain = current->tagHistory();
            siblingFeatures = nullptr;
            continue;
        }

        if (siblingFeatures) {
            if (siblingFeatures->maxDirectAdjacentSelectors == UINT_MAX)
                continue;

            if (current->relation() == CSSSelector::DirectAdjacent)
                siblingFeatures->maxDirectAdjacentSelectors++;
            else
                siblingFeatures->maxDirectAdjacentSelectors = UINT_MAX;
            continue;
        }

        localFeatures = InvalidationSetFeatures();
        auto result = extractInvalidationSetFeatures(*lastCompoundSelectorInAdjacentChain, localFeatures, Ancestor);
        ASSERT(result.first);
        localFeatures.forceSubtree = result.second == ForceSubtree;
        siblingFeatures = &localFeatures;
    }

    if (universalCompound && siblingFeatures)
        addFeaturesToUniversalSiblingInvalidationSet(*siblingFeatures, descendantFeatures);
}

RuleFeatureSet::SelectorPreMatch RuleFeatureSet::collectFeaturesFromRuleData(const RuleData& ruleData)
{
    FeatureMetadata metadata;
    if (collectFeaturesFromSelector(ruleData.selector(), metadata) == SelectorNeverMatches)
        return SelectorNeverMatches;

    m_metadata.add(metadata);

    if (metadata.foundSiblingSelector)
        siblingRules.append(RuleFeature(ruleData.rule(), ruleData.selectorIndex(), ruleData.hasDocumentSecurityOrigin()));
    if (ruleData.containsUncommonAttributeSelector())
        uncommonAttributeRules.append(RuleFeature(ruleData.rule(), ruleData.selectorIndex(), ruleData.hasDocumentSecurityOrigin()));

    updateInvalidationSets(ruleData);
    return SelectorMayMatch;
}

RuleFeatureSet::SelectorPreMatch RuleFeatureSet::collectFeaturesFromSelector(const CSSSelector& selector, RuleFeatureSet::FeatureMetadata& metadata)
{
    unsigned maxDirectAdjacentSelectors = 0;
    CSSSelector::RelationType relation = CSSSelector::Descendant;
    bool foundHostPseudo = false;

    for (const CSSSelector* current = &selector; current; current = current->tagHistory()) {
        switch (current->getPseudoType()) {
        case CSSSelector::PseudoFirstLine:
            metadata.usesFirstLineRules = true;
            break;
        case CSSSelector::PseudoWindowInactive:
            metadata.usesWindowInactiveSelector = true;
            break;
        case CSSSelector::PseudoEmpty:
        case CSSSelector::PseudoFirstChild:
        case CSSSelector::PseudoFirstOfType:
        case CSSSelector::PseudoLastChild:
        case CSSSelector::PseudoLastOfType:
        case CSSSelector::PseudoOnlyChild:
        case CSSSelector::PseudoOnlyOfType:
        case CSSSelector::PseudoNthChild:
        case CSSSelector::PseudoNthOfType:
        case CSSSelector::PseudoNthLastChild:
        case CSSSelector::PseudoNthLastOfType:
            if (!metadata.foundInsertionPointCrossing)
                metadata.foundSiblingSelector = true;
            break;
        case CSSSelector::PseudoHost:
        case CSSSelector::PseudoHostContext:
            if (!foundHostPseudo && relation == CSSSelector::SubSelector)
                return SelectorNeverMatches;
            if (!current->isLastInTagHistory()
                && current->tagHistory()->match() != CSSSelector::PseudoElement
                && !current->tagHistory()->isHostPseudoClass()) {
                return SelectorNeverMatches;
            }
            foundHostPseudo = true;
            // fall through.
        default:
            if (const CSSSelectorList* selectorList = current->selectorList()) {
                for (const CSSSelector* subSelector = selectorList->first(); subSelector; subSelector = CSSSelectorList::next(*subSelector))
                    collectFeaturesFromSelector(*subSelector, metadata);
            }
            break;
        }

        if (current->relationIsAffectedByPseudoContent() || current->getPseudoType() == CSSSelector::PseudoSlotted)
            metadata.foundInsertionPointCrossing = true;

        relation = current->relation();

        if (foundHostPseudo && relation != CSSSelector::SubSelector)
            return SelectorNeverMatches;

        if (relation == CSSSelector::DirectAdjacent) {
            maxDirectAdjacentSelectors++;
        } else if (maxDirectAdjacentSelectors
            && ((relation != CSSSelector::SubSelector) || current->isLastInTagHistory())) {
            if (maxDirectAdjacentSelectors > metadata.maxDirectAdjacentSelectors)
                metadata.maxDirectAdjacentSelectors = maxDirectAdjacentSelectors;
            maxDirectAdjacentSelectors = 0;
        }

        if (!metadata.foundInsertionPointCrossing && current->isAdjacentSelector())
            metadata.foundSiblingSelector = true;
    }

    ASSERT(!maxDirectAdjacentSelectors);
    return SelectorMayMatch;
}

void RuleFeatureSet::FeatureMetadata::add(const FeatureMetadata& other)
{
    usesFirstLineRules = usesFirstLineRules || other.usesFirstLineRules;
    usesWindowInactiveSelector = usesWindowInactiveSelector || other.usesWindowInactiveSelector;
    maxDirectAdjacentSelectors = std::max(maxDirectAdjacentSelectors, other.maxDirectAdjacentSelectors);
}

void RuleFeatureSet::FeatureMetadata::clear()
{
    usesFirstLineRules = false;
    usesWindowInactiveSelector = false;
    foundSiblingSelector = false;
    foundInsertionPointCrossing = false;
    maxDirectAdjacentSelectors = 0;
}

void RuleFeatureSet::add(const RuleFeatureSet& other)
{
    for (const auto& entry : other.m_classInvalidationSets)
        ensureInvalidationSet(m_classInvalidationSets, entry.key, entry.value->type()).combine(*entry.value);
    for (const auto& entry : other.m_attributeInvalidationSets)
        ensureInvalidationSet(m_attributeInvalidationSets, entry.key, entry.value->type()).combine(*entry.value);
    for (const auto& entry : other.m_idInvalidationSets)
        ensureInvalidationSet(m_idInvalidationSets, entry.key, entry.value->type()).combine(*entry.value);
    for (const auto& entry : other.m_pseudoInvalidationSets)
        ensureInvalidationSet(m_pseudoInvalidationSets, static_cast<CSSSelector::PseudoType>(entry.key), entry.value->type()).combine(*entry.value);
    if (other.m_universalSiblingInvalidationSet)
        ensureUniversalSiblingInvalidationSet().combine(*other.m_universalSiblingInvalidationSet);

    m_metadata.add(other.m_metadata);

    siblingRules.appendVector(other.siblingRules);
    uncommonAttributeRules.appendVector(other.uncommonAttributeRules);
}

void RuleFeatureSet::clear()
{
    siblingRules.clear();
    uncommonAttributeRules.clear();
    m_metadata.clear();
    m_classInvalidationSets.clear();
    m_attributeInvalidationSets.clear();
    m_idInvalidationSets.clear();
    m_pseudoInvalidationSets.clear();
}

void RuleFeatureSet::collectInvalidationSetsForClass(InvalidationLists& invalidationLists, Element& element, const AtomicString& className) const
{
    InvalidationSetMap::const_iterator it = m_classInvalidationSets.find(className);
    if (it == m_classInvalidationSets.end())
        return;

    DescendantInvalidationSet* descendants;
    SiblingInvalidationSet* siblings;
    extractInvalidationSets(it->value.get(), descendants, siblings);

    if (descendants) {
        TRACE_SCHEDULE_STYLE_INVALIDATION(element, *descendants, classChange, className);
        invalidationLists.descendants.append(descendants);
    }

    if (siblings) {
        TRACE_SCHEDULE_STYLE_INVALIDATION(element, *siblings, classChange, className);
        invalidationLists.siblings.append(siblings);
    }
}

void RuleFeatureSet::collectSiblingInvalidationSetForClass(InvalidationLists& invalidationLists, Element& element, const AtomicString& className) const
{
    InvalidationSetMap::const_iterator it = m_classInvalidationSets.find(className);
    if (it == m_classInvalidationSets.end())
        return;

    InvalidationSet* invalidationSet = it->value.get();
    if (invalidationSet->type() == InvalidateDescendants)
        return;

    SiblingInvalidationSet* siblingSet = toSiblingInvalidationSet(invalidationSet);
    TRACE_SCHEDULE_STYLE_INVALIDATION(element, *siblingSet, classChange, className);
    invalidationLists.siblings.append(siblingSet);
}

void RuleFeatureSet::collectInvalidationSetsForId(InvalidationLists& invalidationLists, Element& element, const AtomicString& id) const
{
    InvalidationSetMap::const_iterator it = m_idInvalidationSets.find(id);
    if (it == m_idInvalidationSets.end())
        return;

    DescendantInvalidationSet* descendants;
    SiblingInvalidationSet* siblings;
    extractInvalidationSets(it->value.get(), descendants, siblings);

    if (descendants) {
        TRACE_SCHEDULE_STYLE_INVALIDATION(element, *descendants, idChange, id);
        invalidationLists.descendants.append(descendants);
    }

    if (siblings) {
        TRACE_SCHEDULE_STYLE_INVALIDATION(element, *siblings, idChange, id);
        invalidationLists.siblings.append(siblings);
    }
}

void RuleFeatureSet::collectSiblingInvalidationSetForId(InvalidationLists& invalidationLists, Element& element, const AtomicString& id) const
{
    InvalidationSetMap::const_iterator it = m_idInvalidationSets.find(id);
    if (it == m_idInvalidationSets.end())
        return;

    InvalidationSet* invalidationSet = it->value.get();
    if (invalidationSet->type() == InvalidateDescendants)
        return;

    SiblingInvalidationSet* siblingSet = toSiblingInvalidationSet(invalidationSet);
    TRACE_SCHEDULE_STYLE_INVALIDATION(element, *siblingSet, idChange, id);
    invalidationLists.siblings.append(siblingSet);
}

void RuleFeatureSet::collectInvalidationSetsForAttribute(InvalidationLists& invalidationLists, Element& element, const QualifiedName& attributeName) const
{
    InvalidationSetMap::const_iterator it = m_attributeInvalidationSets.find(attributeName.localName());
    if (it == m_attributeInvalidationSets.end())
        return;

    DescendantInvalidationSet* descendants;
    SiblingInvalidationSet* siblings;
    extractInvalidationSets(it->value.get(), descendants, siblings);

    if (descendants) {
        TRACE_SCHEDULE_STYLE_INVALIDATION(element, *descendants, attributeChange, attributeName);
        invalidationLists.descendants.append(descendants);
    }

    if (siblings) {
        TRACE_SCHEDULE_STYLE_INVALIDATION(element, *siblings, attributeChange, attributeName);
        invalidationLists.siblings.append(siblings);
    }
}

void RuleFeatureSet::collectSiblingInvalidationSetForAttribute(InvalidationLists& invalidationLists, Element& element, const QualifiedName& attributeName) const
{
    InvalidationSetMap::const_iterator it = m_attributeInvalidationSets.find(attributeName.localName());
    if (it == m_attributeInvalidationSets.end())
        return;

    InvalidationSet* invalidationSet = it->value.get();
    if (invalidationSet->type() == InvalidateDescendants)
        return;

    SiblingInvalidationSet* siblingSet = toSiblingInvalidationSet(invalidationSet);
    TRACE_SCHEDULE_STYLE_INVALIDATION(element, *siblingSet, attributeChange, attributeName);
    invalidationLists.siblings.append(siblingSet);
}

void RuleFeatureSet::collectInvalidationSetsForPseudoClass(InvalidationLists& invalidationLists, Element& element, CSSSelector::PseudoType pseudo) const
{
    PseudoTypeInvalidationSetMap::const_iterator it = m_pseudoInvalidationSets.find(pseudo);
    if (it == m_pseudoInvalidationSets.end())
        return;

    DescendantInvalidationSet* descendants;
    SiblingInvalidationSet* siblings;
    extractInvalidationSets(it->value.get(), descendants, siblings);

    if (descendants) {
        TRACE_SCHEDULE_STYLE_INVALIDATION(element, *descendants, pseudoChange, pseudo);
        invalidationLists.descendants.append(descendants);
    }

    if (siblings) {
        TRACE_SCHEDULE_STYLE_INVALIDATION(element, *siblings, pseudoChange, pseudo);
        invalidationLists.siblings.append(siblings);
    }
}

void RuleFeatureSet::collectUniversalSiblingInvalidationSet(InvalidationLists& invalidationLists) const
{
    if (m_universalSiblingInvalidationSet)
        invalidationLists.siblings.append(m_universalSiblingInvalidationSet);
}

SiblingInvalidationSet& RuleFeatureSet::ensureUniversalSiblingInvalidationSet()
{
    if (!m_universalSiblingInvalidationSet)
        m_universalSiblingInvalidationSet = SiblingInvalidationSet::create(nullptr);
    return *m_universalSiblingInvalidationSet;
}

void RuleFeatureSet::addFeaturesToUniversalSiblingInvalidationSet(const InvalidationSetFeatures& siblingFeatures, const InvalidationSetFeatures& descendantFeatures)
{
    SiblingInvalidationSet& universalSet = ensureUniversalSiblingInvalidationSet();
    addFeaturesToInvalidationSet(universalSet, siblingFeatures);
    universalSet.updateMaxDirectAdjacentSelectors(siblingFeatures.maxDirectAdjacentSelectors);

    if (&siblingFeatures == &descendantFeatures)
        universalSet.setInvalidatesSelf();
    else
        addFeaturesToInvalidationSet(universalSet.ensureSiblingDescendants(), descendantFeatures);
}

DEFINE_TRACE(RuleFeatureSet)
{
    visitor->trace(siblingRules);
    visitor->trace(uncommonAttributeRules);
}

} // namespace blink
