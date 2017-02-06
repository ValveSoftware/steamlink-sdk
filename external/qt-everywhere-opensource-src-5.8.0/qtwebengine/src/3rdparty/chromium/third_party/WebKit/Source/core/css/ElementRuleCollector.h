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

#ifndef ElementRuleCollector_h
#define ElementRuleCollector_h

#include "core/css/PseudoStyleRequest.h"
#include "core/css/SelectorChecker.h"
#include "core/css/resolver/ElementResolveContext.h"
#include "core/css/resolver/MatchRequest.h"
#include "core/css/resolver/MatchResult.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace blink {

class CSSStyleSheet;
class CSSRuleList;
class RuleData;
class RuleSet;
class SelectorFilter;
class StaticCSSRuleList;

// TODO(kochi): CascadeOrder is used only for Shadow DOM V0 bug-compatible cascading order.
//              Once Shadow DOM V0 implementation is gone, remove this completely.
using CascadeOrder = unsigned;
const CascadeOrder ignoreCascadeOrder = 0;

class MatchedRule {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    MatchedRule(const RuleData* ruleData, unsigned specificity, CascadeOrder cascadeOrder, unsigned styleSheetIndex, const CSSStyleSheet* parentStyleSheet)
        : m_ruleData(ruleData)
        , m_specificity(specificity)
        , m_parentStyleSheet(parentStyleSheet)
    {
        ASSERT(m_ruleData);
        static const unsigned BitsForPositionInRuleData = 18;
        static const unsigned BitsForStyleSheetIndex = 32;
        m_position = ((uint64_t)cascadeOrder << (BitsForStyleSheetIndex + BitsForPositionInRuleData)) + ((uint64_t)styleSheetIndex << BitsForPositionInRuleData) + m_ruleData->position();
    }

    const RuleData* ruleData() const { return m_ruleData; }
    uint64_t position() const { return m_position; }
    unsigned specificity() const { return ruleData()->specificity() + m_specificity; }
    const CSSStyleSheet* parentStyleSheet() const { return m_parentStyleSheet; }
    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_parentStyleSheet);
    }

private:
    // TODO(Oilpan): RuleData is in the oilpan heap and this pointer
    // really should be traced. However, RuleData objects are
    // allocated inside larger TerminatedArray objects and we cannot
    // trace a raw rule data pointer at this point.
    const RuleData* m_ruleData;
    unsigned m_specificity;
    uint64_t m_position;
    Member<const CSSStyleSheet> m_parentStyleSheet;
};

} // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::MatchedRule);

namespace blink {

using StyleRuleList = HeapVector<Member<StyleRule>>;

// ElementRuleCollector is designed to be used as a stack object.
// Create one, ask what rules the ElementResolveContext matches
// and then let it go out of scope.
// FIXME: Currently it modifies the ComputedStyle but should not!
class ElementRuleCollector {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(ElementRuleCollector);
public:
    ElementRuleCollector(const ElementResolveContext&, const SelectorFilter&, ComputedStyle* = 0);
    ~ElementRuleCollector();

    void setMode(SelectorChecker::Mode mode) { m_mode = mode; }
    void setPseudoStyleRequest(const PseudoStyleRequest& request) { m_pseudoStyleRequest = request; }
    void setSameOriginOnly(bool f) { m_sameOriginOnly = f; }

    void setMatchingUARules(bool matchingUARules) { m_matchingUARules = matchingUARules; }
    bool hasAnyMatchingRules(RuleSet*);

    const MatchResult& matchedResult() const;
    StyleRuleList* matchedStyleRuleList();
    CSSRuleList* matchedCSSRuleList();

    void collectMatchingRules(const MatchRequest&, CascadeOrder = ignoreCascadeOrder, bool matchingTreeBoundaryRules = false);
    void collectMatchingShadowHostRules(const MatchRequest&, CascadeOrder = ignoreCascadeOrder);
    void sortAndTransferMatchedRules();
    void clearMatchedRules();
    void addElementStyleProperties(const StylePropertySet*, bool isCacheable = true);
    void finishAddingUARules() { m_result.finishAddingUARules(); }
    void finishAddingAuthorRulesForTreeScope() { m_result.finishAddingAuthorRulesForTreeScope(); }
    void setIncludeEmptyRules(bool include) { m_includeEmptyRules = include; }
    bool includeEmptyRules() const { return m_includeEmptyRules; }
    bool isCollectingForPseudoElement() const { return m_pseudoStyleRequest.pseudoId != PseudoIdNone; }

private:
    template<typename RuleDataListType>
    void collectMatchingRulesForList(const RuleDataListType*, CascadeOrder, const MatchRequest&);

    void didMatchRule(const RuleData&, const SelectorChecker::MatchResult&, CascadeOrder, const MatchRequest&);

    template<class CSSRuleCollection>
    CSSRule* findStyleRule(CSSRuleCollection*, StyleRule*);
    void appendCSSOMWrapperForRule(CSSStyleSheet*, StyleRule*);

    void sortMatchedRules();

    StaticCSSRuleList* ensureRuleList();
    StyleRuleList* ensureStyleRuleList();

private:
    const ElementResolveContext& m_context;
    const SelectorFilter& m_selectorFilter;
    RefPtr<ComputedStyle> m_style; // FIXME: This can be mutated during matching!

    PseudoStyleRequest m_pseudoStyleRequest;
    SelectorChecker::Mode m_mode;
    bool m_canUseFastReject;
    bool m_sameOriginOnly;
    bool m_matchingUARules;
    bool m_includeEmptyRules;

    HeapVector<MatchedRule, 32> m_matchedRules;

    // Output.
    Member<StaticCSSRuleList> m_cssRuleList;
    Member<StyleRuleList> m_styleRuleList;
    MatchResult m_result;
};

} // namespace blink

#endif // ElementRuleCollector_h
