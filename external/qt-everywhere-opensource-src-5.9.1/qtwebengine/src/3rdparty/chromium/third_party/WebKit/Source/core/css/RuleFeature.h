/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
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

#ifndef RuleFeature_h
#define RuleFeature_h

#include "core/CoreExport.h"
#include "core/css/CSSSelector.h"
#include "core/css/invalidation/InvalidationSet.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/HashSet.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

struct InvalidationLists;
class QualifiedName;
class RuleData;
class StyleRule;

struct RuleFeature {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  RuleFeature(StyleRule*,
              unsigned selectorIndex,
              bool hasDocumentSecurityOrigin);

  DECLARE_TRACE();

  Member<StyleRule> rule;
  unsigned selectorIndex;
  bool hasDocumentSecurityOrigin;
};

}  // namespace blink

// Declare the VectorTraits specialization before RuleFeatureSet
// declares its vector members below.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::RuleFeature);

namespace blink {

class CORE_EXPORT RuleFeatureSet {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(RuleFeatureSet);

 public:
  RuleFeatureSet();
  ~RuleFeatureSet();

  void add(const RuleFeatureSet&);
  void clear();

  enum SelectorPreMatch { SelectorNeverMatches, SelectorMayMatch };

  SelectorPreMatch collectFeaturesFromRuleData(const RuleData&);

  bool usesSiblingRules() const { return !m_siblingRules.isEmpty(); }
  bool usesFirstLineRules() const { return m_metadata.usesFirstLineRules; }
  bool usesWindowInactiveSelector() const {
    return m_metadata.usesWindowInactiveSelector;
  }
  bool needsFullRecalcForRuleSetInvalidation() const {
    return m_metadata.needsFullRecalcForRuleSetInvalidation;
  }

  unsigned maxDirectAdjacentSelectors() const {
    return m_metadata.maxDirectAdjacentSelectors;
  }

  bool hasSelectorForAttribute(const AtomicString& attributeName) const {
    DCHECK(!attributeName.isEmpty());
    return m_attributeInvalidationSets.contains(attributeName);
  }

  bool hasSelectorForClass(const AtomicString& classValue) const {
    DCHECK(!classValue.isEmpty());
    return m_classInvalidationSets.contains(classValue);
  }

  bool hasSelectorForId(const AtomicString& idValue) const {
    return m_idInvalidationSets.contains(idValue);
  }

  const HeapVector<RuleFeature>& siblingRules() const { return m_siblingRules; }
  const HeapVector<RuleFeature>& uncommonAttributeRules() const {
    return m_uncommonAttributeRules;
  }

  // Collect descendant and sibling invalidation sets.
  void collectInvalidationSetsForClass(InvalidationLists&,
                                       Element&,
                                       const AtomicString& className) const;
  void collectInvalidationSetsForId(InvalidationLists&,
                                    Element&,
                                    const AtomicString& id) const;
  void collectInvalidationSetsForAttribute(
      InvalidationLists&,
      Element&,
      const QualifiedName& attributeName) const;
  void collectInvalidationSetsForPseudoClass(InvalidationLists&,
                                             Element&,
                                             CSSSelector::PseudoType) const;

  void collectSiblingInvalidationSetForClass(InvalidationLists&,
                                             Element&,
                                             const AtomicString& className,
                                             unsigned minDirectAdjacent) const;
  void collectSiblingInvalidationSetForId(InvalidationLists&,
                                          Element&,
                                          const AtomicString& id,
                                          unsigned minDirectAdjacent) const;
  void collectSiblingInvalidationSetForAttribute(
      InvalidationLists&,
      Element&,
      const QualifiedName& attributeName,
      unsigned minDirectAdjacent) const;
  void collectUniversalSiblingInvalidationSet(InvalidationLists&,
                                              unsigned minDirectAdjacent) const;
  void collectNthInvalidationSet(InvalidationLists&) const;

  bool hasIdsInSelectors() const { return m_idInvalidationSets.size() > 0; }

  DECLARE_TRACE();

  bool isAlive() const { return m_isAlive; }

 protected:
  InvalidationSet* invalidationSetForSimpleSelector(const CSSSelector&,
                                                    InvalidationType);

 private:
  // Each map entry is either a DescendantInvalidationSet or
  // SiblingInvalidationSet.
  // When both are needed, we store the SiblingInvalidationSet, and use it to
  // hold the DescendantInvalidationSet.
  using InvalidationSetMap = HashMap<AtomicString, RefPtr<InvalidationSet>>;
  using PseudoTypeInvalidationSetMap =
      HashMap<CSSSelector::PseudoType,
              RefPtr<InvalidationSet>,
              WTF::IntHash<unsigned>,
              WTF::UnsignedWithZeroKeyHashTraits<unsigned>>;

  struct FeatureMetadata {
    DISALLOW_NEW();
    void add(const FeatureMetadata& other);
    void clear();

    bool usesFirstLineRules = false;
    bool usesWindowInactiveSelector = false;
    bool foundSiblingSelector = false;
    bool foundInsertionPointCrossing = false;
    bool needsFullRecalcForRuleSetInvalidation = false;
    unsigned maxDirectAdjacentSelectors = 0;
  };

  SelectorPreMatch collectFeaturesFromSelector(const CSSSelector&,
                                               FeatureMetadata&);

  InvalidationSet& ensureClassInvalidationSet(const AtomicString& className,
                                              InvalidationType);
  InvalidationSet& ensureAttributeInvalidationSet(
      const AtomicString& attributeName,
      InvalidationType);
  InvalidationSet& ensureIdInvalidationSet(const AtomicString& id,
                                           InvalidationType);
  InvalidationSet& ensurePseudoInvalidationSet(CSSSelector::PseudoType,
                                               InvalidationType);
  SiblingInvalidationSet& ensureUniversalSiblingInvalidationSet();
  DescendantInvalidationSet& ensureNthInvalidationSet();

  void updateInvalidationSets(const RuleData&);
  void updateInvalidationSetsForContentAttribute(const RuleData&);

  struct InvalidationSetFeatures {
    DISALLOW_NEW();

    void add(const InvalidationSetFeatures& other);
    bool hasFeatures() const;
    bool hasTagIdClassOrAttribute() const;

    Vector<AtomicString> classes;
    Vector<AtomicString> attributes;
    Vector<AtomicString> ids;
    Vector<AtomicString> tagNames;
    unsigned maxDirectAdjacentSelectors = 0;
    bool customPseudoElement = false;
    bool hasBeforeOrAfter = false;
    bool treeBoundaryCrossing = false;
    bool insertionPointCrossing = false;
    bool forceSubtree = false;
    bool contentPseudoCrossing = false;
    bool invalidatesSlotted = false;
    bool hasNthPseudo = false;
    bool hasFeaturesForRuleSetInvalidation = false;
  };

  static void extractInvalidationSetFeature(const CSSSelector&,
                                            InvalidationSetFeatures&);

  enum PositionType { Subject, Ancestor };
  enum FeatureInvalidationType {
    NormalInvalidation,
    RequiresSubtreeInvalidation
  };

  void extractInvalidationSetFeaturesFromSimpleSelector(
      const CSSSelector&,
      InvalidationSetFeatures&);
  const CSSSelector* extractInvalidationSetFeaturesFromCompound(
      const CSSSelector&,
      InvalidationSetFeatures&,
      PositionType,
      CSSSelector::PseudoType = CSSSelector::PseudoUnknown);
  FeatureInvalidationType extractInvalidationSetFeaturesFromSelectorList(
      const CSSSelector&,
      InvalidationSetFeatures&,
      PositionType);
  void updateFeaturesFromCombinator(
      const CSSSelector&,
      const CSSSelector* lastCompoundSelectorInAdjacentChain,
      InvalidationSetFeatures& lastCompoundInAdjacentChainFeatures,
      InvalidationSetFeatures*& siblingFeatures,
      InvalidationSetFeatures& descendantFeatures);

  void addFeaturesToInvalidationSet(InvalidationSet&,
                                    const InvalidationSetFeatures&);
  void addFeaturesToInvalidationSets(
      const CSSSelector&,
      InvalidationSetFeatures* siblingFeatures,
      InvalidationSetFeatures& descendantFeatures);
  const CSSSelector* addFeaturesToInvalidationSetsForCompoundSelector(
      const CSSSelector&,
      InvalidationSetFeatures* siblingFeatures,
      InvalidationSetFeatures& descendantFeatures);
  void addFeaturesToInvalidationSetsForSimpleSelector(
      const CSSSelector&,
      InvalidationSetFeatures* siblingFeatures,
      InvalidationSetFeatures& descendantFeatures);
  void addFeaturesToInvalidationSetsForSelectorList(
      const CSSSelector&,
      InvalidationSetFeatures* siblingFeatures,
      InvalidationSetFeatures& descendantFeatures);
  void addFeaturesToUniversalSiblingInvalidationSet(
      const InvalidationSetFeatures& siblingFeatures,
      const InvalidationSetFeatures& descendantFeatures);

  void addClassToInvalidationSet(const AtomicString& className, Element&);

  FeatureMetadata m_metadata;
  InvalidationSetMap m_classInvalidationSets;
  InvalidationSetMap m_attributeInvalidationSets;
  InvalidationSetMap m_idInvalidationSets;
  PseudoTypeInvalidationSetMap m_pseudoInvalidationSets;
  RefPtr<SiblingInvalidationSet> m_universalSiblingInvalidationSet;
  RefPtr<DescendantInvalidationSet> m_nthInvalidationSet;
  HeapVector<RuleFeature> m_siblingRules;
  HeapVector<RuleFeature> m_uncommonAttributeRules;

  // If true, the RuleFeatureSet is alive and can be used.
  unsigned m_isAlive : 1;

  friend class RuleFeatureSetTest;
};

}  // namespace blink

#endif  // RuleFeature_h
