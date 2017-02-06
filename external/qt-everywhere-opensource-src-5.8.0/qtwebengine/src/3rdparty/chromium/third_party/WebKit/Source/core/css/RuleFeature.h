/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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
class SpaceSplitString;
class StyleRule;

struct RuleFeature {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    RuleFeature(StyleRule*, unsigned selectorIndex, bool hasDocumentSecurityOrigin);

    DECLARE_TRACE();

    Member<StyleRule> rule;
    unsigned selectorIndex;
    bool hasDocumentSecurityOrigin;
};

} // namespace blink

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

    bool usesSiblingRules() const { return !siblingRules.isEmpty(); }
    bool usesFirstLineRules() const { return m_metadata.usesFirstLineRules; }
    bool usesWindowInactiveSelector() const { return m_metadata.usesWindowInactiveSelector; }

    unsigned maxDirectAdjacentSelectors() const { return m_metadata.maxDirectAdjacentSelectors; }

    bool hasSelectorForAttribute(const AtomicString& attributeName) const
    {
        ASSERT(!attributeName.isEmpty());
        return m_attributeInvalidationSets.contains(attributeName);
    }

    bool hasSelectorForClass(const AtomicString& classValue) const
    {
        ASSERT(!classValue.isEmpty());
        return m_classInvalidationSets.contains(classValue);
    }

    bool hasSelectorForId(const AtomicString& idValue) const { return m_idInvalidationSets.contains(idValue); }

    // Collect descendant and sibling invalidation sets.
    void collectInvalidationSetsForClass(InvalidationLists&, Element&, const AtomicString& className) const;
    void collectInvalidationSetsForId(InvalidationLists&, Element&, const AtomicString& id) const;
    void collectInvalidationSetsForAttribute(InvalidationLists&, Element&, const QualifiedName& attributeName) const;
    void collectInvalidationSetsForPseudoClass(InvalidationLists&, Element&, CSSSelector::PseudoType) const;

    void collectSiblingInvalidationSetForClass(InvalidationLists&, Element&, const AtomicString& className) const;
    void collectSiblingInvalidationSetForId(InvalidationLists&, Element&, const AtomicString& id) const;
    void collectSiblingInvalidationSetForAttribute(InvalidationLists&, Element&, const QualifiedName& attributeName) const;
    void collectUniversalSiblingInvalidationSet(InvalidationLists&) const;

    bool hasIdsInSelectors() const
    {
        return m_idInvalidationSets.size() > 0;
    }

    DECLARE_TRACE();

    HeapVector<RuleFeature> siblingRules;
    HeapVector<RuleFeature> uncommonAttributeRules;

protected:
    InvalidationSet* invalidationSetForSelector(const CSSSelector&, InvalidationType);

private:
    // Each map entry is either a DescendantInvalidationSet or SiblingInvalidationSet.
    // When both are needed, we store the SiblingInvalidationSet, and use it to hold the DescendantInvalidationSet.
    using InvalidationSetMap = HashMap<AtomicString, RefPtr<InvalidationSet>>;
    using PseudoTypeInvalidationSetMap = HashMap<CSSSelector::PseudoType, RefPtr<InvalidationSet>, WTF::IntHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>>;

    struct FeatureMetadata {
        DISALLOW_NEW();
        void add(const FeatureMetadata& other);
        void clear();

        bool usesFirstLineRules = false;
        bool usesWindowInactiveSelector = false;
        bool foundSiblingSelector = false;
        bool foundInsertionPointCrossing = false;
        unsigned maxDirectAdjacentSelectors = 0;
    };

    SelectorPreMatch collectFeaturesFromSelector(const CSSSelector&, FeatureMetadata&);

    InvalidationSet& ensureClassInvalidationSet(const AtomicString& className, InvalidationType);
    InvalidationSet& ensureAttributeInvalidationSet(const AtomicString& attributeName, InvalidationType);
    InvalidationSet& ensureIdInvalidationSet(const AtomicString& id, InvalidationType);
    InvalidationSet& ensurePseudoInvalidationSet(CSSSelector::PseudoType, InvalidationType);
    SiblingInvalidationSet& ensureUniversalSiblingInvalidationSet();

    void updateInvalidationSets(const RuleData&);
    void updateInvalidationSetsForContentAttribute(const RuleData&);

    struct InvalidationSetFeatures {
        DISALLOW_NEW();

        Vector<AtomicString> classes;
        Vector<AtomicString> attributes;
        AtomicString id;
        AtomicString tagName;
        unsigned maxDirectAdjacentSelectors = UINT_MAX;
        bool customPseudoElement = false;
        bool hasBeforeOrAfter = false;
        bool treeBoundaryCrossing = false;
        bool adjacent = false;
        bool insertionPointCrossing = false;
        bool forceSubtree = false;
        bool contentPseudoCrossing = false;
        bool invalidatesSlotted = false;
    };

    static bool extractInvalidationSetFeature(const CSSSelector&, InvalidationSetFeatures&);

    enum UseFeaturesType { UseFeatures, ForceSubtree };

    enum PositionType { Subject, Ancestor };
    std::pair<const CSSSelector*, UseFeaturesType> extractInvalidationSetFeatures(const CSSSelector&, InvalidationSetFeatures&, PositionType, CSSSelector::PseudoType = CSSSelector::PseudoUnknown);

    void addFeaturesToInvalidationSet(InvalidationSet&, const InvalidationSetFeatures&);
    void addFeaturesToInvalidationSets(const CSSSelector*, InvalidationSetFeatures* siblingFeatures, InvalidationSetFeatures& descendantFeatures);
    void addFeaturesToUniversalSiblingInvalidationSet(const InvalidationSetFeatures& siblingFeatures, const InvalidationSetFeatures& descendantFeatures);

    void addClassToInvalidationSet(const AtomicString& className, Element&);

    FeatureMetadata m_metadata;
    InvalidationSetMap m_classInvalidationSets;
    InvalidationSetMap m_attributeInvalidationSets;
    InvalidationSetMap m_idInvalidationSets;
    PseudoTypeInvalidationSetMap m_pseudoInvalidationSets;
    RefPtr<SiblingInvalidationSet> m_universalSiblingInvalidationSet;

    friend class RuleFeatureSetTest;
};

} // namespace blink

#endif // RuleFeature_h
