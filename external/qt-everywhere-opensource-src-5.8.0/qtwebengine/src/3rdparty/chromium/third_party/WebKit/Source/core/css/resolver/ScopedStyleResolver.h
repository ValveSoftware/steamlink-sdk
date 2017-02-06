/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScopedStyleResolver_h
#define ScopedStyleResolver_h

#include "core/css/ElementRuleCollector.h"
#include "core/css/RuleSet.h"
#include "core/dom/TreeScope.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"

namespace blink {

class PageRuleCollector;
class StyleSheetContents;
class ViewportStyleResolver;

// This class selects a ComputedStyle for a given element based on a collection of stylesheets.
class ScopedStyleResolver final : public GarbageCollected<ScopedStyleResolver> {
    WTF_MAKE_NONCOPYABLE(ScopedStyleResolver);
public:
    static ScopedStyleResolver* create(TreeScope& scope)
    {
        return new ScopedStyleResolver(scope);
    }

    const TreeScope& treeScope() const { return *m_scope; }
    ScopedStyleResolver* parent() const;

    StyleRuleKeyframes* keyframeStylesForAnimation(const StringImpl* animationName);

    void appendCSSStyleSheet(CSSStyleSheet&, const MediaQueryEvaluator&);
    void collectMatchingAuthorRules(ElementRuleCollector&, CascadeOrder = ignoreCascadeOrder);
    void collectMatchingShadowHostRules(ElementRuleCollector&, CascadeOrder = ignoreCascadeOrder);
    void collectMatchingTreeBoundaryCrossingRules(ElementRuleCollector&, CascadeOrder = ignoreCascadeOrder);
    void matchPageRules(PageRuleCollector&);
    void collectFeaturesTo(RuleFeatureSet&, HeapHashSet<Member<const StyleSheetContents>>& visitedSharedStyleSheetContents) const;
    void resetAuthorStyle();
    void collectViewportRulesTo(ViewportStyleResolver*) const;
    bool hasDeepOrShadowSelector() const { return m_hasDeepOrShadowSelector; }

    DECLARE_TRACE();

private:
    explicit ScopedStyleResolver(TreeScope& scope)
        : m_scope(scope)
    {
    }

    void addTreeBoundaryCrossingRules(const RuleSet&, CSSStyleSheet*, unsigned sheetIndex);
    void addKeyframeRules(const RuleSet&);
    void addFontFaceRules(const RuleSet&);
    void addKeyframeStyle(StyleRuleKeyframes*);

    Member<TreeScope> m_scope;

    HeapVector<Member<CSSStyleSheet>> m_authorStyleSheets;

    using KeyframesRuleMap = HeapHashMap<const StringImpl*, Member<StyleRuleKeyframes>>;
    KeyframesRuleMap m_keyframesRuleMap;

    class RuleSubSet final : public GarbageCollected<RuleSubSet> {
    public:
        static RuleSubSet* create(CSSStyleSheet* sheet, unsigned index, RuleSet* rules)
        {
            return new RuleSubSet(sheet, index, rules);
        }

        Member<CSSStyleSheet> m_parentStyleSheet;
        unsigned m_parentIndex;
        Member<RuleSet> m_ruleSet;

        DECLARE_TRACE();

    private:
        RuleSubSet(CSSStyleSheet* sheet, unsigned index, RuleSet* rules)
            : m_parentStyleSheet(sheet)
            , m_parentIndex(index)
            , m_ruleSet(rules)
        {
        }
    };
    using CSSStyleSheetRuleSubSet = HeapVector<Member<RuleSubSet>>;

    Member<CSSStyleSheetRuleSubSet> m_treeBoundaryCrossingRuleSet;
    bool m_hasDeepOrShadowSelector = false;
};

} // namespace blink

#endif // ScopedStyleResolver_h
