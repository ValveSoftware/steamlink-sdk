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

#ifndef RuleSet_h
#define RuleSet_h

#include "core/CoreExport.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/RuleFeature.h"
#include "core/css/StyleRule.h"
#include "core/css/resolver/MediaQueryResult.h"
#include "platform/heap/HeapLinkedStack.h"
#include "platform/heap/HeapTerminatedArray.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/LinkedStack.h"
#include "wtf/TerminatedArray.h"

namespace blink {

enum AddRuleFlags {
    RuleHasNoSpecialState         = 0,
    RuleHasDocumentSecurityOrigin = 1,
};

enum PropertyWhitelistType {
    PropertyWhitelistNone,
    PropertyWhitelistCue,
    PropertyWhitelistFirstLetter,
};

class CSSSelector;
class MediaQueryEvaluator;
class StyleSheetContents;

class MinimalRuleData {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    MinimalRuleData(StyleRule* rule, unsigned selectorIndex, AddRuleFlags flags)
    : m_rule(rule)
    , m_selectorIndex(selectorIndex)
    , m_flags(flags)
    {
    }

    DECLARE_TRACE();

    Member<StyleRule> m_rule;
    unsigned m_selectorIndex;
    AddRuleFlags m_flags;
};

} // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::MinimalRuleData);

namespace blink {

class CORE_EXPORT RuleData {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    RuleData(StyleRule*, unsigned selectorIndex, unsigned position, AddRuleFlags);

    unsigned position() const { return m_position; }
    StyleRule* rule() const { return m_rule; }
    const CSSSelector& selector() const { return m_rule->selectorList().selectorAt(m_selectorIndex); }
    unsigned selectorIndex() const { return m_selectorIndex; }

    bool isLastInArray() const { return m_isLastInArray; }
    void setLastInArray(bool flag) { m_isLastInArray = flag; }

    bool containsUncommonAttributeSelector() const { return m_containsUncommonAttributeSelector; }
    unsigned specificity() const { return m_specificity; }
    unsigned linkMatchType() const { return m_linkMatchType; }
    bool hasDocumentSecurityOrigin() const { return m_hasDocumentSecurityOrigin; }
    PropertyWhitelistType propertyWhitelist(bool isMatchingUARules = false) const { return isMatchingUARules ? PropertyWhitelistNone : static_cast<PropertyWhitelistType>(m_propertyWhitelist); }
    // Try to balance between memory usage (there can be lots of RuleData objects) and good filtering performance.
    static const unsigned maximumIdentifierCount = 4;
    const unsigned* descendantSelectorIdentifierHashes() const { return m_descendantSelectorIdentifierHashes; }

    DECLARE_TRACE();

private:
    Member<StyleRule> m_rule;
    unsigned m_selectorIndex : 13;
    unsigned m_isLastInArray : 1; // We store an array of RuleData objects in a primitive array.
    // This number was picked fairly arbitrarily. We can probably lower it if we need to.
    // Some simple testing showed <100,000 RuleData's on large sites.
    unsigned m_position : 18;
    unsigned m_specificity : 24;
    unsigned m_containsUncommonAttributeSelector : 1;
    unsigned m_linkMatchType : 2; //  CSSSelector::LinkMatchMask
    unsigned m_hasDocumentSecurityOrigin : 1;
    unsigned m_propertyWhitelist : 2;
    // Use plain array instead of a Vector to minimize memory overhead.
    unsigned m_descendantSelectorIdentifierHashes[maximumIdentifierCount];
};

} // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::RuleData);

namespace blink {

struct SameSizeAsRuleData {
    DISALLOW_NEW();
    Member<void*> a;
    unsigned b;
    unsigned c;
    unsigned d[4];
};

static_assert(sizeof(RuleData) == sizeof(SameSizeAsRuleData), "RuleData should stay small");

class CORE_EXPORT RuleSet : public GarbageCollectedFinalized<RuleSet> {
    WTF_MAKE_NONCOPYABLE(RuleSet);
public:
    static RuleSet* create() { return new RuleSet; }

    void addRulesFromSheet(StyleSheetContents*, const MediaQueryEvaluator&, AddRuleFlags = RuleHasNoSpecialState);
    void addStyleRule(StyleRule*, AddRuleFlags);
    void addRule(StyleRule*, unsigned selectorIndex, AddRuleFlags);

    const RuleFeatureSet& features() const { return m_features; }

    const HeapTerminatedArray<RuleData>* idRules(const AtomicString& key) const { ASSERT(!m_pendingRules); return m_idRules.get(key); }
    const HeapTerminatedArray<RuleData>* classRules(const AtomicString& key) const { ASSERT(!m_pendingRules); return m_classRules.get(key); }
    const HeapTerminatedArray<RuleData>* tagRules(const AtomicString& key) const { ASSERT(!m_pendingRules); return m_tagRules.get(key); }
    const HeapTerminatedArray<RuleData>* shadowPseudoElementRules(const AtomicString& key) const { ASSERT(!m_pendingRules); return m_shadowPseudoElementRules.get(key); }
    const HeapVector<RuleData>* linkPseudoClassRules() const { ASSERT(!m_pendingRules); return &m_linkPseudoClassRules; }
    const HeapVector<RuleData>* cuePseudoRules() const { ASSERT(!m_pendingRules); return &m_cuePseudoRules; }
    const HeapVector<RuleData>* focusPseudoClassRules() const { ASSERT(!m_pendingRules); return &m_focusPseudoClassRules; }
    const HeapVector<RuleData>* universalRules() const { ASSERT(!m_pendingRules); return &m_universalRules; }
    const HeapVector<RuleData>* shadowHostRules() const { ASSERT(!m_pendingRules); return &m_shadowHostRules; }
    const HeapVector<Member<StyleRulePage>>& pageRules() const { ASSERT(!m_pendingRules); return m_pageRules; }
    const HeapVector<Member<StyleRuleViewport>>& viewportRules() const { ASSERT(!m_pendingRules); return m_viewportRules; }
    const HeapVector<Member<StyleRuleFontFace>>& fontFaceRules() const { return m_fontFaceRules; }
    const HeapVector<Member<StyleRuleKeyframes>>& keyframesRules() const { return m_keyframesRules; }
    const HeapVector<MinimalRuleData>& deepCombinatorOrShadowPseudoRules() const { return m_deepCombinatorOrShadowPseudoRules; }
    const HeapVector<MinimalRuleData>& contentPseudoElementRules() const { return m_contentPseudoElementRules; }
    const HeapVector<MinimalRuleData>& slottedPseudoElementRules() const { return m_slottedPseudoElementRules; }
    const MediaQueryResultList& viewportDependentMediaQueryResults() const { return m_viewportDependentMediaQueryResults; }
    const MediaQueryResultList& deviceDependentMediaQueryResults() const { return m_deviceDependentMediaQueryResults; }

    unsigned ruleCount() const { return m_ruleCount; }

    void compactRulesIfNeeded()
    {
        if (!m_pendingRules)
            return;
        compactRules();
    }

#ifndef NDEBUG
    void show() const;
#endif

    DECLARE_TRACE();

private:
    using PendingRuleMap = HeapHashMap<AtomicString, Member<HeapLinkedStack<RuleData>>>;
    using CompactRuleMap = HeapHashMap<AtomicString, Member<HeapTerminatedArray<RuleData>>>;

    RuleSet()
        : m_ruleCount(0)
    {
    }

    void addToRuleSet(const AtomicString& key, PendingRuleMap&, const RuleData&);
    void addPageRule(StyleRulePage*);
    void addViewportRule(StyleRuleViewport*);
    void addFontFaceRule(StyleRuleFontFace*);
    void addKeyframesRule(StyleRuleKeyframes*);

    void addChildRules(const HeapVector<Member<StyleRuleBase>>&, const MediaQueryEvaluator& medium, AddRuleFlags);
    bool findBestRuleSetAndAdd(const CSSSelector&, RuleData&);

    void compactRules();
    static void compactPendingRules(PendingRuleMap&, CompactRuleMap&);

    class PendingRuleMaps : public GarbageCollected<PendingRuleMaps> {
    public:
        static PendingRuleMaps* create() { return new PendingRuleMaps; }

        PendingRuleMap idRules;
        PendingRuleMap classRules;
        PendingRuleMap tagRules;
        PendingRuleMap shadowPseudoElementRules;

        DECLARE_TRACE();

    private:
        PendingRuleMaps() { }
    };

    PendingRuleMaps* ensurePendingRules()
    {
        if (!m_pendingRules)
            m_pendingRules = PendingRuleMaps::create();
        return m_pendingRules.get();
    }

    CompactRuleMap m_idRules;
    CompactRuleMap m_classRules;
    CompactRuleMap m_tagRules;
    CompactRuleMap m_shadowPseudoElementRules;
    HeapVector<RuleData> m_linkPseudoClassRules;
    HeapVector<RuleData> m_cuePseudoRules;
    HeapVector<RuleData> m_focusPseudoClassRules;
    HeapVector<RuleData> m_universalRules;
    HeapVector<RuleData> m_shadowHostRules;
    RuleFeatureSet m_features;
    HeapVector<Member<StyleRulePage>> m_pageRules;
    HeapVector<Member<StyleRuleViewport>> m_viewportRules;
    HeapVector<Member<StyleRuleFontFace>> m_fontFaceRules;
    HeapVector<Member<StyleRuleKeyframes>> m_keyframesRules;
    HeapVector<MinimalRuleData> m_deepCombinatorOrShadowPseudoRules;
    HeapVector<MinimalRuleData> m_contentPseudoElementRules;
    HeapVector<MinimalRuleData> m_slottedPseudoElementRules;

    MediaQueryResultList m_viewportDependentMediaQueryResults;
    MediaQueryResultList m_deviceDependentMediaQueryResults;

    unsigned m_ruleCount;
    Member<PendingRuleMaps> m_pendingRules;

#ifndef NDEBUG
    HeapVector<RuleData> m_allRules;
#endif
};

} // namespace blink

#endif // RuleSet_h
