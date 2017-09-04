/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
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

bool supportsInvalidation(CSSSelector::MatchType match) {
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
      NOTREACHED();
      return false;
    default:
      // New match type added. Figure out if it needs a subtree invalidation or
      // not.
      NOTREACHED();
      return false;
  }
}

bool supportsInvalidation(CSSSelector::PseudoType type) {
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
      NOTREACHED();
      return false;
    default:
      // New pseudo type added. Figure out if it needs a subtree invalidation or
      // not.
      NOTREACHED();
      return false;
  }
}

bool supportsInvalidationWithSelectorList(CSSSelector::PseudoType pseudo) {
  return pseudo == CSSSelector::PseudoAny || pseudo == CSSSelector::PseudoCue ||
         pseudo == CSSSelector::PseudoHost ||
         pseudo == CSSSelector::PseudoHostContext ||
         pseudo == CSSSelector::PseudoNot ||
         pseudo == CSSSelector::PseudoSlotted;
}

bool requiresSubtreeInvalidation(const CSSSelector& selector) {
  if (selector.match() != CSSSelector::PseudoElement &&
      selector.match() != CSSSelector::PseudoClass) {
    DCHECK(supportsInvalidation(selector.match()));
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
      DCHECK(supportsInvalidation(selector.getPseudoType()));
      return false;
  }
}

InvalidationSet& storedInvalidationSet(RefPtr<InvalidationSet>& invalidationSet,
                                       InvalidationType type) {
  if (!invalidationSet) {
    if (type == InvalidateDescendants)
      invalidationSet = DescendantInvalidationSet::create();
    else
      invalidationSet = SiblingInvalidationSet::create(nullptr);
    return *invalidationSet;
  }
  if (invalidationSet->type() == type)
    return *invalidationSet;

  if (type == InvalidateDescendants)
    return toSiblingInvalidationSet(*invalidationSet).ensureDescendants();

  RefPtr<InvalidationSet> descendants = invalidationSet;
  invalidationSet = SiblingInvalidationSet::create(
      toDescendantInvalidationSet(descendants.get()));
  return *invalidationSet;
}

InvalidationSet& ensureInvalidationSet(
    HashMap<AtomicString, RefPtr<InvalidationSet>>& map,
    const AtomicString& key,
    InvalidationType type) {
  RefPtr<InvalidationSet>& invalidationSet =
      map.add(key, nullptr).storedValue->value;
  return storedInvalidationSet(invalidationSet, type);
}

InvalidationSet& ensureInvalidationSet(
    HashMap<CSSSelector::PseudoType,
            RefPtr<InvalidationSet>,
            WTF::IntHash<unsigned>,
            WTF::UnsignedWithZeroKeyHashTraits<unsigned>>& map,
    CSSSelector::PseudoType key,
    InvalidationType type) {
  RefPtr<InvalidationSet>& invalidationSet =
      map.add(key, nullptr).storedValue->value;
  return storedInvalidationSet(invalidationSet, type);
}

void extractInvalidationSets(InvalidationSet* invalidationSet,
                             DescendantInvalidationSet*& descendants,
                             SiblingInvalidationSet*& siblings) {
  RELEASE_ASSERT(invalidationSet->isAlive());
  if (invalidationSet->type() == InvalidateDescendants) {
    descendants = toDescendantInvalidationSet(invalidationSet);
    siblings = nullptr;
    return;
  }

  siblings = toSiblingInvalidationSet(invalidationSet);
  descendants = siblings->descendants();
}

}  // anonymous namespace

RuleFeature::RuleFeature(StyleRule* rule,
                         unsigned selectorIndex,
                         bool hasDocumentSecurityOrigin)
    : rule(rule),
      selectorIndex(selectorIndex),
      hasDocumentSecurityOrigin(hasDocumentSecurityOrigin) {}

DEFINE_TRACE(RuleFeature) {
  visitor->trace(rule);
}

RuleFeatureSet::RuleFeatureSet() : m_isAlive(true) {}

RuleFeatureSet::~RuleFeatureSet() {
  RELEASE_ASSERT(m_isAlive);

  m_metadata.clear();
  m_classInvalidationSets.clear();
  m_attributeInvalidationSets.clear();
  m_idInvalidationSets.clear();
  m_pseudoInvalidationSets.clear();
  m_universalSiblingInvalidationSet.clear();
  m_nthInvalidationSet.clear();

  m_isAlive = false;
}

ALWAYS_INLINE InvalidationSet& RuleFeatureSet::ensureClassInvalidationSet(
    const AtomicString& className,
    InvalidationType type) {
  RELEASE_ASSERT(!className.isEmpty());
  return ensureInvalidationSet(m_classInvalidationSets, className, type);
}

ALWAYS_INLINE InvalidationSet& RuleFeatureSet::ensureAttributeInvalidationSet(
    const AtomicString& attributeName,
    InvalidationType type) {
  RELEASE_ASSERT(!attributeName.isEmpty());
  return ensureInvalidationSet(m_attributeInvalidationSets, attributeName,
                               type);
}

ALWAYS_INLINE InvalidationSet& RuleFeatureSet::ensureIdInvalidationSet(
    const AtomicString& id,
    InvalidationType type) {
  RELEASE_ASSERT(!id.isEmpty());
  return ensureInvalidationSet(m_idInvalidationSets, id, type);
}

ALWAYS_INLINE InvalidationSet& RuleFeatureSet::ensurePseudoInvalidationSet(
    CSSSelector::PseudoType pseudoType,
    InvalidationType type) {
  RELEASE_ASSERT(pseudoType != CSSSelector::PseudoUnknown);
  return ensureInvalidationSet(m_pseudoInvalidationSets, pseudoType, type);
}

void RuleFeatureSet::updateFeaturesFromCombinator(
    const CSSSelector& lastInCompound,
    const CSSSelector* lastCompoundInAdjacentChain,
    InvalidationSetFeatures& lastCompoundInAdjacentChainFeatures,
    InvalidationSetFeatures*& siblingFeatures,
    InvalidationSetFeatures& descendantFeatures) {
  if (lastInCompound.isAdjacentSelector()) {
    if (!siblingFeatures) {
      siblingFeatures = &lastCompoundInAdjacentChainFeatures;
      if (lastCompoundInAdjacentChain) {
        extractInvalidationSetFeaturesFromCompound(
            *lastCompoundInAdjacentChain, lastCompoundInAdjacentChainFeatures,
            Ancestor);
        if (!lastCompoundInAdjacentChainFeatures.hasFeatures())
          lastCompoundInAdjacentChainFeatures.forceSubtree = true;
      }
    }
    if (siblingFeatures->maxDirectAdjacentSelectors == UINT_MAX)
      return;
    if (lastInCompound.relation() == CSSSelector::DirectAdjacent)
      ++siblingFeatures->maxDirectAdjacentSelectors;
    else
      siblingFeatures->maxDirectAdjacentSelectors = UINT_MAX;
    return;
  }

  if (siblingFeatures &&
      lastCompoundInAdjacentChainFeatures.maxDirectAdjacentSelectors)
    lastCompoundInAdjacentChainFeatures = InvalidationSetFeatures();

  siblingFeatures = nullptr;

  if (lastInCompound.isShadowSelector())
    descendantFeatures.treeBoundaryCrossing = true;
  if (lastInCompound.relation() == CSSSelector::ShadowSlot ||
      lastInCompound.relationIsAffectedByPseudoContent())
    descendantFeatures.insertionPointCrossing = true;
  if (lastInCompound.relationIsAffectedByPseudoContent())
    descendantFeatures.contentPseudoCrossing = true;
}

void RuleFeatureSet::extractInvalidationSetFeaturesFromSimpleSelector(
    const CSSSelector& selector,
    InvalidationSetFeatures& features) {
  if (selector.match() == CSSSelector::Tag &&
      selector.tagQName().localName() != starAtom) {
    features.tagNames.append(selector.tagQName().localName());
    return;
  }
  if (selector.match() == CSSSelector::Id) {
    features.ids.append(selector.value());
    return;
  }
  if (selector.match() == CSSSelector::Class) {
    features.classes.append(selector.value());
    return;
  }
  if (selector.isAttributeSelector()) {
    features.attributes.append(selector.attribute().localName());
    return;
  }
  switch (selector.getPseudoType()) {
    case CSSSelector::PseudoWebKitCustomElement:
    case CSSSelector::PseudoBlinkInternalElement:
      features.customPseudoElement = true;
      return;
    case CSSSelector::PseudoBefore:
    case CSSSelector::PseudoAfter:
      features.hasBeforeOrAfter = true;
      return;
    case CSSSelector::PseudoSlotted:
      features.invalidatesSlotted = true;
      return;
    default:
      return;
  }
}

InvalidationSet* RuleFeatureSet::invalidationSetForSimpleSelector(
    const CSSSelector& selector,
    InvalidationType type) {
  if (selector.match() == CSSSelector::Class)
    return &ensureClassInvalidationSet(selector.value(), type);
  if (selector.isAttributeSelector())
    return &ensureAttributeInvalidationSet(selector.attribute().localName(),
                                           type);
  if (selector.match() == CSSSelector::Id)
    return &ensureIdInvalidationSet(selector.value(), type);
  if (selector.match() == CSSSelector::PseudoClass) {
    switch (selector.getPseudoType()) {
      case CSSSelector::PseudoEmpty:
      case CSSSelector::PseudoFirstChild:
      case CSSSelector::PseudoLastChild:
      case CSSSelector::PseudoOnlyChild:
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
      case CSSSelector::PseudoFirstOfType:
      case CSSSelector::PseudoLastOfType:
      case CSSSelector::PseudoOnlyOfType:
      case CSSSelector::PseudoNthChild:
      case CSSSelector::PseudoNthOfType:
      case CSSSelector::PseudoNthLastChild:
      case CSSSelector::PseudoNthLastOfType:
        return &ensureNthInvalidationSet();
      default:
        break;
    }
  }
  return nullptr;
}

void RuleFeatureSet::updateInvalidationSets(const RuleData& ruleData) {
  // Given a rule, update the descendant invalidation sets for the features
  // found in its selector. The first step is to extract the features from the
  // rightmost compound selector (extractInvalidationSetFeaturesFromCompound).
  // Secondly, add those features to the invalidation sets for the features
  // found in the other compound selectors (addFeaturesToInvalidationSets). If
  // we find a feature in the right-most compound selector that requires a
  // subtree recalc, nextCompound will be the rightmost compound and we will
  // addFeaturesToInvalidationSets for that one as well.

  InvalidationSetFeatures features;
  InvalidationSetFeatures* siblingFeatures = nullptr;

  const CSSSelector* lastInCompound =
      extractInvalidationSetFeaturesFromCompound(ruleData.selector(), features,
                                                 Subject);

  if (features.forceSubtree)
    features.hasFeaturesForRuleSetInvalidation = false;
  else if (!features.hasFeatures())
    features.forceSubtree = true;
  if (features.hasNthPseudo)
    addFeaturesToInvalidationSet(ensureNthInvalidationSet(), features);
  if (features.hasBeforeOrAfter)
    updateInvalidationSetsForContentAttribute(ruleData);

  const CSSSelector* nextCompound =
      lastInCompound ? lastInCompound->tagHistory() : &ruleData.selector();
  if (!nextCompound) {
    if (!features.hasFeaturesForRuleSetInvalidation)
      m_metadata.needsFullRecalcForRuleSetInvalidation = true;
    return;
  }
  if (lastInCompound)
    updateFeaturesFromCombinator(*lastInCompound, nullptr, features,
                                 siblingFeatures, features);

  addFeaturesToInvalidationSets(*nextCompound, siblingFeatures, features);

  if (!features.hasFeaturesForRuleSetInvalidation)
    m_metadata.needsFullRecalcForRuleSetInvalidation = true;
}

void RuleFeatureSet::updateInvalidationSetsForContentAttribute(
    const RuleData& ruleData) {
  // If any ::before and ::after rules specify 'content: attr(...)', we
  // need to create invalidation sets for those attributes to have content
  // changes applied through style recalc.

  const StylePropertySet& propertySet = ruleData.rule()->properties();

  int propertyIndex = propertySet.findPropertyIndex(CSSPropertyContent);

  if (propertyIndex == -1)
    return;

  StylePropertySet::PropertyReference contentProperty =
      propertySet.propertyAt(propertyIndex);
  const CSSValue& contentValue = contentProperty.value();

  if (!contentValue.isValueList())
    return;

  for (auto& item : toCSSValueList(contentValue)) {
    if (!item->isFunctionValue())
      continue;
    const CSSFunctionValue* functionValue = toCSSFunctionValue(item.get());
    if (functionValue->functionType() != CSSValueAttr)
      continue;
    ensureAttributeInvalidationSet(
        AtomicString(toCSSCustomIdentValue(functionValue->item(0)).value()),
        InvalidateDescendants)
        .setInvalidatesSelf();
  }
}

RuleFeatureSet::FeatureInvalidationType
RuleFeatureSet::extractInvalidationSetFeaturesFromSelectorList(
    const CSSSelector& simpleSelector,
    InvalidationSetFeatures& features,
    PositionType position) {
  const CSSSelectorList* selectorList = simpleSelector.selectorList();
  if (!selectorList)
    return NormalInvalidation;

  DCHECK(supportsInvalidationWithSelectorList(simpleSelector.getPseudoType()));

  const CSSSelector* subSelector = selectorList->first();

  bool allSubSelectorsHaveFeatures = true;
  InvalidationSetFeatures anyFeatures;

  for (; subSelector; subSelector = CSSSelectorList::next(*subSelector)) {
    InvalidationSetFeatures compoundFeatures;
    if (!extractInvalidationSetFeaturesFromCompound(
            *subSelector, compoundFeatures, position,
            simpleSelector.getPseudoType())) {
      // A null selector return means the sub-selector contained a
      // selector which requiresSubtreeInvalidation().
      DCHECK(compoundFeatures.forceSubtree);
      features.forceSubtree = true;
      return RequiresSubtreeInvalidation;
    }
    if (compoundFeatures.hasNthPseudo)
      features.hasNthPseudo = true;
    if (!allSubSelectorsHaveFeatures)
      continue;
    if (compoundFeatures.hasFeatures())
      anyFeatures.add(compoundFeatures);
    else
      allSubSelectorsHaveFeatures = false;
  }
  // Don't add any features if one of the sub-selectors of does not contain
  // any invalidation set features. E.g. :-webkit-any(*, span).
  if (allSubSelectorsHaveFeatures)
    features.add(anyFeatures);
  return NormalInvalidation;
}

const CSSSelector* RuleFeatureSet::extractInvalidationSetFeaturesFromCompound(
    const CSSSelector& compound,
    InvalidationSetFeatures& features,
    PositionType position,
    CSSSelector::PseudoType pseudo) {
  // Extract invalidation set features and return a pointer to the the last
  // simple selector of the compound, or nullptr if one of the selectors
  // requiresSubtreeInvalidation().

  const CSSSelector* simpleSelector = &compound;
  for (;; simpleSelector = simpleSelector->tagHistory()) {
    // Fall back to use subtree invalidations, even for features in the
    // rightmost compound selector. Returning nullptr here will make
    // addFeaturesToInvalidationSets start marking invalidation sets for
    // subtree recalc for features in the rightmost compound selector.
    if (requiresSubtreeInvalidation(*simpleSelector)) {
      features.forceSubtree = true;
      return nullptr;
    }

    // When inside a :not(), we should not use the found features for
    // invalidation because we should invalidate elements _without_ that
    // feature. On the other hand, we should still have invalidation sets
    // for the features since we are able to detect when they change.
    // That is, ".a" should not have ".b" in its invalidation set for
    // ".a :not(.b)", but there should be an invalidation set for ".a" in
    // ":not(.a) .b".
    if (pseudo != CSSSelector::PseudoNot)
      extractInvalidationSetFeaturesFromSimpleSelector(*simpleSelector,
                                                       features);

    // Initialize the entry in the invalidation set map for self-
    // invalidation, if supported.
    if (InvalidationSet* invalidationSet = invalidationSetForSimpleSelector(
            *simpleSelector, InvalidateDescendants)) {
      if (invalidationSet == m_nthInvalidationSet)
        features.hasNthPseudo = true;
      else if (position == Subject)
        invalidationSet->setInvalidatesSelf();
    }

    if (extractInvalidationSetFeaturesFromSelectorList(*simpleSelector,
                                                       features, position) ==
        RequiresSubtreeInvalidation) {
      DCHECK(features.forceSubtree);
      return nullptr;
    }

    if (!simpleSelector->tagHistory() ||
        simpleSelector->relation() != CSSSelector::SubSelector) {
      features.hasFeaturesForRuleSetInvalidation =
          features.hasTagIdClassOrAttribute();
      return simpleSelector;
    }
  }
}

// Add features extracted from the rightmost compound selector to descendant
// invalidation sets for features found in other compound selectors.
//
// We use descendant invalidation for descendants, sibling invalidation for
// siblings and their subtrees.
//
// As we encounter a descendant type of combinator, the features only need to be
// checked against descendants in the same subtree only. features.adjacent is
// set to false, and we start adding features to the descendant invalidation
// set.

void RuleFeatureSet::addFeaturesToInvalidationSet(
    InvalidationSet& invalidationSet,
    const InvalidationSetFeatures& features) {
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

  for (const auto& id : features.ids)
    invalidationSet.addId(id);
  for (const auto& tagName : features.tagNames)
    invalidationSet.addTagName(tagName);
  for (const auto& className : features.classes)
    invalidationSet.addClass(className);
  for (const auto& attribute : features.attributes)
    invalidationSet.addAttribute(attribute);
  if (features.customPseudoElement)
    invalidationSet.setCustomPseudoInvalid();
}

void RuleFeatureSet::addFeaturesToInvalidationSetsForSelectorList(
    const CSSSelector& simpleSelector,
    InvalidationSetFeatures* siblingFeatures,
    InvalidationSetFeatures& descendantFeatures) {
  if (!simpleSelector.selectorList())
    return;

  DCHECK(supportsInvalidationWithSelectorList(simpleSelector.getPseudoType()));

  bool hadFeaturesForRuleSetInvalidation =
      descendantFeatures.hasFeaturesForRuleSetInvalidation;
  bool selectorListContainsUniversal =
      simpleSelector.getPseudoType() == CSSSelector::PseudoNot ||
      simpleSelector.getPseudoType() == CSSSelector::PseudoHostContext;

  for (const CSSSelector* subSelector = simpleSelector.selectorList()->first();
       subSelector; subSelector = CSSSelectorList::next(*subSelector)) {
    descendantFeatures.hasFeaturesForRuleSetInvalidation = false;

    addFeaturesToInvalidationSetsForCompoundSelector(
        *subSelector, siblingFeatures, descendantFeatures);

    if (!descendantFeatures.hasFeaturesForRuleSetInvalidation)
      selectorListContainsUniversal = true;
  }

  descendantFeatures.hasFeaturesForRuleSetInvalidation =
      hadFeaturesForRuleSetInvalidation || !selectorListContainsUniversal;
}

void RuleFeatureSet::addFeaturesToInvalidationSetsForSimpleSelector(
    const CSSSelector& simpleSelector,
    InvalidationSetFeatures* siblingFeatures,
    InvalidationSetFeatures& descendantFeatures) {
  if (InvalidationSet* invalidationSet = invalidationSetForSimpleSelector(
          simpleSelector,
          siblingFeatures ? InvalidateSiblings : InvalidateDescendants)) {
    if (!siblingFeatures || invalidationSet == m_nthInvalidationSet) {
      addFeaturesToInvalidationSet(*invalidationSet, descendantFeatures);
      return;
    }

    SiblingInvalidationSet* siblingInvalidationSet =
        toSiblingInvalidationSet(invalidationSet);
    siblingInvalidationSet->updateMaxDirectAdjacentSelectors(
        siblingFeatures->maxDirectAdjacentSelectors);
    addFeaturesToInvalidationSet(*invalidationSet, *siblingFeatures);
    if (siblingFeatures == &descendantFeatures)
      siblingInvalidationSet->setInvalidatesSelf();
    else
      addFeaturesToInvalidationSet(
          siblingInvalidationSet->ensureSiblingDescendants(),
          descendantFeatures);
    return;
  }

  if (simpleSelector.isHostPseudoClass())
    descendantFeatures.treeBoundaryCrossing = true;
  if (simpleSelector.isInsertionPointCrossing())
    descendantFeatures.insertionPointCrossing = true;

  addFeaturesToInvalidationSetsForSelectorList(simpleSelector, siblingFeatures,
                                               descendantFeatures);
}

const CSSSelector*
RuleFeatureSet::addFeaturesToInvalidationSetsForCompoundSelector(
    const CSSSelector& compound,
    InvalidationSetFeatures* siblingFeatures,
    InvalidationSetFeatures& descendantFeatures) {
  bool compoundHasIdClassOrAttribute = false;
  const CSSSelector* simpleSelector = &compound;
  for (; simpleSelector; simpleSelector = simpleSelector->tagHistory()) {
    addFeaturesToInvalidationSetsForSimpleSelector(
        *simpleSelector, siblingFeatures, descendantFeatures);
    if (simpleSelector->isIdClassOrAttributeSelector())
      compoundHasIdClassOrAttribute = true;
    if (simpleSelector->relation() != CSSSelector::SubSelector)
      break;
    if (!simpleSelector->tagHistory())
      break;
  }

  if (compoundHasIdClassOrAttribute)
    descendantFeatures.hasFeaturesForRuleSetInvalidation = true;
  else if (siblingFeatures)
    addFeaturesToUniversalSiblingInvalidationSet(*siblingFeatures,
                                                 descendantFeatures);

  return simpleSelector;
}

void RuleFeatureSet::addFeaturesToInvalidationSets(
    const CSSSelector& selector,
    InvalidationSetFeatures* siblingFeatures,
    InvalidationSetFeatures& descendantFeatures) {
  // selector is the selector immediately to the left of the rightmost
  // combinator. descendantFeatures has the features of the rightmost compound
  // selector.

  InvalidationSetFeatures lastCompoundInSiblingChainFeatures;
  const CSSSelector* compound = &selector;
  while (compound) {
    const CSSSelector* lastInCompound =
        addFeaturesToInvalidationSetsForCompoundSelector(
            *compound, siblingFeatures, descendantFeatures);
    DCHECK(lastInCompound);
    updateFeaturesFromCombinator(*lastInCompound, compound,
                                 lastCompoundInSiblingChainFeatures,
                                 siblingFeatures, descendantFeatures);
    compound = lastInCompound->tagHistory();
  }
}

RuleFeatureSet::SelectorPreMatch RuleFeatureSet::collectFeaturesFromRuleData(
    const RuleData& ruleData) {
  RELEASE_ASSERT(m_isAlive);
  FeatureMetadata metadata;
  if (collectFeaturesFromSelector(ruleData.selector(), metadata) ==
      SelectorNeverMatches)
    return SelectorNeverMatches;

  m_metadata.add(metadata);

  if (metadata.foundSiblingSelector) {
    m_siblingRules.append(RuleFeature(ruleData.rule(), ruleData.selectorIndex(),
                                      ruleData.hasDocumentSecurityOrigin()));
  }
  if (ruleData.containsUncommonAttributeSelector()) {
    m_uncommonAttributeRules.append(
        RuleFeature(ruleData.rule(), ruleData.selectorIndex(),
                    ruleData.hasDocumentSecurityOrigin()));
  }

  updateInvalidationSets(ruleData);
  return SelectorMayMatch;
}

RuleFeatureSet::SelectorPreMatch RuleFeatureSet::collectFeaturesFromSelector(
    const CSSSelector& selector,
    RuleFeatureSet::FeatureMetadata& metadata) {
  unsigned maxDirectAdjacentSelectors = 0;
  CSSSelector::RelationType relation = CSSSelector::Descendant;
  bool foundHostPseudo = false;

  for (const CSSSelector* current = &selector; current;
       current = current->tagHistory()) {
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
        if (!current->isLastInTagHistory() &&
            current->tagHistory()->match() != CSSSelector::PseudoElement &&
            !current->tagHistory()->isHostPseudoClass()) {
          return SelectorNeverMatches;
        }
        foundHostPseudo = true;
      // fall through.
      default:
        if (const CSSSelectorList* selectorList = current->selectorList()) {
          for (const CSSSelector* subSelector = selectorList->first();
               subSelector; subSelector = CSSSelectorList::next(*subSelector))
            collectFeaturesFromSelector(*subSelector, metadata);
        }
        break;
    }

    if (current->relationIsAffectedByPseudoContent() ||
        current->getPseudoType() == CSSSelector::PseudoSlotted)
      metadata.foundInsertionPointCrossing = true;

    relation = current->relation();

    if (foundHostPseudo && relation != CSSSelector::SubSelector)
      return SelectorNeverMatches;

    if (relation == CSSSelector::DirectAdjacent) {
      maxDirectAdjacentSelectors++;
    } else if (maxDirectAdjacentSelectors &&
               ((relation != CSSSelector::SubSelector) ||
                current->isLastInTagHistory())) {
      if (maxDirectAdjacentSelectors > metadata.maxDirectAdjacentSelectors)
        metadata.maxDirectAdjacentSelectors = maxDirectAdjacentSelectors;
      maxDirectAdjacentSelectors = 0;
    }

    if (!metadata.foundInsertionPointCrossing && current->isAdjacentSelector())
      metadata.foundSiblingSelector = true;
  }

  DCHECK(!maxDirectAdjacentSelectors);
  return SelectorMayMatch;
}

void RuleFeatureSet::FeatureMetadata::add(const FeatureMetadata& other) {
  usesFirstLineRules |= other.usesFirstLineRules;
  usesWindowInactiveSelector |= other.usesWindowInactiveSelector;
  maxDirectAdjacentSelectors =
      std::max(maxDirectAdjacentSelectors, other.maxDirectAdjacentSelectors);
}

void RuleFeatureSet::FeatureMetadata::clear() {
  usesFirstLineRules = false;
  usesWindowInactiveSelector = false;
  foundSiblingSelector = false;
  foundInsertionPointCrossing = false;
  needsFullRecalcForRuleSetInvalidation = false;
  maxDirectAdjacentSelectors = 0;
}

void RuleFeatureSet::add(const RuleFeatureSet& other) {
  RELEASE_ASSERT(m_isAlive);
  RELEASE_ASSERT(other.m_isAlive);
  RELEASE_ASSERT(&other != this);
  for (const auto& entry : other.m_classInvalidationSets)
    ensureInvalidationSet(m_classInvalidationSets, entry.key,
                          entry.value->type())
        .combine(*entry.value);
  for (const auto& entry : other.m_attributeInvalidationSets)
    ensureInvalidationSet(m_attributeInvalidationSets, entry.key,
                          entry.value->type())
        .combine(*entry.value);
  for (const auto& entry : other.m_idInvalidationSets)
    ensureInvalidationSet(m_idInvalidationSets, entry.key, entry.value->type())
        .combine(*entry.value);
  for (const auto& entry : other.m_pseudoInvalidationSets)
    ensureInvalidationSet(m_pseudoInvalidationSets,
                          static_cast<CSSSelector::PseudoType>(entry.key),
                          entry.value->type())
        .combine(*entry.value);
  if (other.m_universalSiblingInvalidationSet)
    ensureUniversalSiblingInvalidationSet().combine(
        *other.m_universalSiblingInvalidationSet);
  if (other.m_nthInvalidationSet)
    ensureNthInvalidationSet().combine(*other.m_nthInvalidationSet);

  m_metadata.add(other.m_metadata);

  m_siblingRules.appendVector(other.m_siblingRules);
  m_uncommonAttributeRules.appendVector(other.m_uncommonAttributeRules);
}

void RuleFeatureSet::clear() {
  RELEASE_ASSERT(m_isAlive);
  m_siblingRules.clear();
  m_uncommonAttributeRules.clear();
  m_metadata.clear();
  m_classInvalidationSets.clear();
  m_attributeInvalidationSets.clear();
  m_idInvalidationSets.clear();
  m_pseudoInvalidationSets.clear();
  m_universalSiblingInvalidationSet.clear();
  m_nthInvalidationSet.clear();
}

void RuleFeatureSet::collectInvalidationSetsForClass(
    InvalidationLists& invalidationLists,
    Element& element,
    const AtomicString& className) const {
  InvalidationSetMap::const_iterator it =
      m_classInvalidationSets.find(className);
  if (it == m_classInvalidationSets.end())
    return;

  DescendantInvalidationSet* descendants;
  SiblingInvalidationSet* siblings;
  extractInvalidationSets(it->value.get(), descendants, siblings);

  if (descendants) {
    TRACE_SCHEDULE_STYLE_INVALIDATION(element, *descendants, classChange,
                                      className);
    invalidationLists.descendants.append(descendants);
  }

  if (siblings) {
    TRACE_SCHEDULE_STYLE_INVALIDATION(element, *siblings, classChange,
                                      className);
    invalidationLists.siblings.append(siblings);
  }
}

void RuleFeatureSet::collectSiblingInvalidationSetForClass(
    InvalidationLists& invalidationLists,
    Element& element,
    const AtomicString& className,
    unsigned minDirectAdjacent) const {
  InvalidationSetMap::const_iterator it =
      m_classInvalidationSets.find(className);
  if (it == m_classInvalidationSets.end())
    return;

  InvalidationSet* invalidationSet = it->value.get();
  if (invalidationSet->type() == InvalidateDescendants)
    return;

  SiblingInvalidationSet* siblingSet =
      toSiblingInvalidationSet(invalidationSet);
  if (siblingSet->maxDirectAdjacentSelectors() < minDirectAdjacent)
    return;

  TRACE_SCHEDULE_STYLE_INVALIDATION(element, *siblingSet, classChange,
                                    className);
  invalidationLists.siblings.append(siblingSet);
}

void RuleFeatureSet::collectInvalidationSetsForId(
    InvalidationLists& invalidationLists,
    Element& element,
    const AtomicString& id) const {
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

void RuleFeatureSet::collectSiblingInvalidationSetForId(
    InvalidationLists& invalidationLists,
    Element& element,
    const AtomicString& id,
    unsigned minDirectAdjacent) const {
  InvalidationSetMap::const_iterator it = m_idInvalidationSets.find(id);
  if (it == m_idInvalidationSets.end())
    return;

  InvalidationSet* invalidationSet = it->value.get();
  if (invalidationSet->type() == InvalidateDescendants)
    return;

  SiblingInvalidationSet* siblingSet =
      toSiblingInvalidationSet(invalidationSet);
  if (siblingSet->maxDirectAdjacentSelectors() < minDirectAdjacent)
    return;

  TRACE_SCHEDULE_STYLE_INVALIDATION(element, *siblingSet, idChange, id);
  invalidationLists.siblings.append(siblingSet);
}

void RuleFeatureSet::collectInvalidationSetsForAttribute(
    InvalidationLists& invalidationLists,
    Element& element,
    const QualifiedName& attributeName) const {
  InvalidationSetMap::const_iterator it =
      m_attributeInvalidationSets.find(attributeName.localName());
  if (it == m_attributeInvalidationSets.end())
    return;

  DescendantInvalidationSet* descendants;
  SiblingInvalidationSet* siblings;
  extractInvalidationSets(it->value.get(), descendants, siblings);

  if (descendants) {
    TRACE_SCHEDULE_STYLE_INVALIDATION(element, *descendants, attributeChange,
                                      attributeName);
    invalidationLists.descendants.append(descendants);
  }

  if (siblings) {
    TRACE_SCHEDULE_STYLE_INVALIDATION(element, *siblings, attributeChange,
                                      attributeName);
    invalidationLists.siblings.append(siblings);
  }
}

void RuleFeatureSet::collectSiblingInvalidationSetForAttribute(
    InvalidationLists& invalidationLists,
    Element& element,
    const QualifiedName& attributeName,
    unsigned minDirectAdjacent) const {
  InvalidationSetMap::const_iterator it =
      m_attributeInvalidationSets.find(attributeName.localName());
  if (it == m_attributeInvalidationSets.end())
    return;

  InvalidationSet* invalidationSet = it->value.get();
  if (invalidationSet->type() == InvalidateDescendants)
    return;

  SiblingInvalidationSet* siblingSet =
      toSiblingInvalidationSet(invalidationSet);
  if (siblingSet->maxDirectAdjacentSelectors() < minDirectAdjacent)
    return;

  TRACE_SCHEDULE_STYLE_INVALIDATION(element, *siblingSet, attributeChange,
                                    attributeName);
  invalidationLists.siblings.append(siblingSet);
}

void RuleFeatureSet::collectInvalidationSetsForPseudoClass(
    InvalidationLists& invalidationLists,
    Element& element,
    CSSSelector::PseudoType pseudo) const {
  PseudoTypeInvalidationSetMap::const_iterator it =
      m_pseudoInvalidationSets.find(pseudo);
  if (it == m_pseudoInvalidationSets.end())
    return;

  DescendantInvalidationSet* descendants;
  SiblingInvalidationSet* siblings;
  extractInvalidationSets(it->value.get(), descendants, siblings);

  if (descendants) {
    TRACE_SCHEDULE_STYLE_INVALIDATION(element, *descendants, pseudoChange,
                                      pseudo);
    invalidationLists.descendants.append(descendants);
  }

  if (siblings) {
    TRACE_SCHEDULE_STYLE_INVALIDATION(element, *siblings, pseudoChange, pseudo);
    invalidationLists.siblings.append(siblings);
  }
}

void RuleFeatureSet::collectUniversalSiblingInvalidationSet(
    InvalidationLists& invalidationLists,
    unsigned minDirectAdjacent) const {
  if (m_universalSiblingInvalidationSet &&
      m_universalSiblingInvalidationSet->maxDirectAdjacentSelectors() >=
          minDirectAdjacent)
    invalidationLists.siblings.append(m_universalSiblingInvalidationSet);
}

SiblingInvalidationSet&
RuleFeatureSet::ensureUniversalSiblingInvalidationSet() {
  if (!m_universalSiblingInvalidationSet)
    m_universalSiblingInvalidationSet = SiblingInvalidationSet::create(nullptr);
  return *m_universalSiblingInvalidationSet;
}

void RuleFeatureSet::collectNthInvalidationSet(
    InvalidationLists& invalidationLists) const {
  if (m_nthInvalidationSet)
    invalidationLists.descendants.append(m_nthInvalidationSet);
}

DescendantInvalidationSet& RuleFeatureSet::ensureNthInvalidationSet() {
  if (!m_nthInvalidationSet)
    m_nthInvalidationSet = DescendantInvalidationSet::create();
  return *m_nthInvalidationSet;
}

void RuleFeatureSet::addFeaturesToUniversalSiblingInvalidationSet(
    const InvalidationSetFeatures& siblingFeatures,
    const InvalidationSetFeatures& descendantFeatures) {
  SiblingInvalidationSet& universalSet =
      ensureUniversalSiblingInvalidationSet();
  addFeaturesToInvalidationSet(universalSet, siblingFeatures);
  universalSet.updateMaxDirectAdjacentSelectors(
      siblingFeatures.maxDirectAdjacentSelectors);

  if (&siblingFeatures == &descendantFeatures)
    universalSet.setInvalidatesSelf();
  else
    addFeaturesToInvalidationSet(universalSet.ensureSiblingDescendants(),
                                 descendantFeatures);
}

DEFINE_TRACE(RuleFeatureSet) {
  visitor->trace(m_siblingRules);
  visitor->trace(m_uncommonAttributeRules);
}

void RuleFeatureSet::InvalidationSetFeatures::add(
    const InvalidationSetFeatures& other) {
  classes.appendVector(other.classes);
  attributes.appendVector(other.attributes);
  ids.appendVector(other.ids);
  tagNames.appendVector(other.tagNames);
  maxDirectAdjacentSelectors =
      std::max(maxDirectAdjacentSelectors, other.maxDirectAdjacentSelectors);
  customPseudoElement |= other.customPseudoElement;
  hasBeforeOrAfter |= other.hasBeforeOrAfter;
  treeBoundaryCrossing |= other.treeBoundaryCrossing;
  insertionPointCrossing |= other.insertionPointCrossing;
  forceSubtree |= other.forceSubtree;
  contentPseudoCrossing |= other.contentPseudoCrossing;
  invalidatesSlotted |= other.invalidatesSlotted;
  hasNthPseudo |= other.hasNthPseudo;
}

bool RuleFeatureSet::InvalidationSetFeatures::hasFeatures() const {
  return !classes.isEmpty() || !attributes.isEmpty() || !ids.isEmpty() ||
         !tagNames.isEmpty() || customPseudoElement;
}

bool RuleFeatureSet::InvalidationSetFeatures::hasTagIdClassOrAttribute() const {
  return !classes.isEmpty() || !attributes.isEmpty() || !ids.isEmpty() ||
         !tagNames.isEmpty();
}

}  // namespace blink
