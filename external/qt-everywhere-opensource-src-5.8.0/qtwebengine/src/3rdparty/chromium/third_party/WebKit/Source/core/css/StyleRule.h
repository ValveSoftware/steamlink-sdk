/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2008, 2012, 2013 Apple Inc. All rights reserved.
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

#ifndef StyleRule_h
#define StyleRule_h

#include "core/CoreExport.h"
#include "core/css/CSSSelectorList.h"
#include "core/css/MediaList.h"
#include "core/css/StylePropertySet.h"
#include "platform/heap/Handle.h"
#include "wtf/RefPtr.h"

namespace blink {

class CSSRule;
class CSSStyleSheet;

class CORE_EXPORT StyleRuleBase : public GarbageCollectedFinalized<StyleRuleBase> {
public:
    enum RuleType {
        Charset,
        Style,
        Import,
        Media,
        FontFace,
        Page,
        Keyframes,
        Keyframe,
        Namespace,
        Supports,
        Viewport,
    };

    RuleType type() const { return static_cast<RuleType>(m_type); }

    bool isCharsetRule() const { return type() == Charset; }
    bool isFontFaceRule() const { return type() == FontFace; }
    bool isKeyframesRule() const { return type() == Keyframes; }
    bool isKeyframeRule() const { return type() == Keyframe; }
    bool isNamespaceRule() const { return type() == Namespace; }
    bool isMediaRule() const { return type() == Media; }
    bool isPageRule() const { return type() == Page; }
    bool isStyleRule() const { return type() == Style; }
    bool isSupportsRule() const { return type() == Supports; }
    bool isViewportRule() const { return type() == Viewport; }
    bool isImportRule() const { return type() == Import; }

    StyleRuleBase* copy() const;

    // FIXME: There shouldn't be any need for the null parent version.
    CSSRule* createCSSOMWrapper(CSSStyleSheet* parentSheet = 0) const;
    CSSRule* createCSSOMWrapper(CSSRule* parentRule) const;

    DECLARE_TRACE();
    DEFINE_INLINE_TRACE_AFTER_DISPATCH() { }
    void finalizeGarbageCollectedObject();

    // ~StyleRuleBase should be public, because non-public ~StyleRuleBase
    // causes C2248 error : 'blink::StyleRuleBase::~StyleRuleBase' : cannot
    // access protected member declared in class 'blink::StyleRuleBase' when
    // compiling 'source\wtf\refcounted.h' by using msvc.
    ~StyleRuleBase() { }

protected:
    StyleRuleBase(RuleType type) : m_type(type) { }
    StyleRuleBase(const StyleRuleBase& o) : m_type(o.m_type) { }

private:
    void destroy();

    CSSRule* createCSSOMWrapper(CSSStyleSheet* parentSheet, CSSRule* parentRule) const;

    unsigned m_type : 5;
};

class CORE_EXPORT StyleRule : public StyleRuleBase {
public:
    // Adopts the selector list
    static StyleRule* create(CSSSelectorList selectorList, StylePropertySet* properties)
    {
        return new StyleRule(std::move(selectorList), properties);
    }

    ~StyleRule();

    const CSSSelectorList& selectorList() const { return m_selectorList; }
    const StylePropertySet& properties() const { return *m_properties; }
    MutableStylePropertySet& mutableProperties();

    void wrapperAdoptSelectorList(CSSSelectorList selectors) { m_selectorList = std::move(selectors); }

    StyleRule* copy() const { return new StyleRule(*this); }

    static unsigned averageSizeInBytes();

    DECLARE_TRACE_AFTER_DISPATCH();

private:
    StyleRule(CSSSelectorList, StylePropertySet*);
    StyleRule(const StyleRule&);

    Member<StylePropertySet> m_properties; // Cannot be null.
    CSSSelectorList m_selectorList;
};

class StyleRuleFontFace : public StyleRuleBase {
public:
    static StyleRuleFontFace* create(StylePropertySet* properties)
    {
        return new StyleRuleFontFace(properties);
    }

    ~StyleRuleFontFace();

    const StylePropertySet& properties() const { return *m_properties; }
    MutableStylePropertySet& mutableProperties();

    StyleRuleFontFace* copy() const { return new StyleRuleFontFace(*this); }

    DECLARE_TRACE_AFTER_DISPATCH();

private:
    StyleRuleFontFace(StylePropertySet*);
    StyleRuleFontFace(const StyleRuleFontFace&);

    Member<StylePropertySet> m_properties; // Cannot be null.
};

class StyleRulePage : public StyleRuleBase {
public:
    // Adopts the selector list
    static StyleRulePage* create(CSSSelectorList selectorList, StylePropertySet* properties)
    {
        return new StyleRulePage(std::move(selectorList), properties);
    }

    ~StyleRulePage();

    const CSSSelector* selector() const { return m_selectorList.first(); }
    const StylePropertySet& properties() const { return *m_properties; }
    MutableStylePropertySet& mutableProperties();

    void wrapperAdoptSelectorList(CSSSelectorList selectors) { m_selectorList = std::move(selectors); }

    StyleRulePage* copy() const { return new StyleRulePage(*this); }

    DECLARE_TRACE_AFTER_DISPATCH();

private:
    StyleRulePage(CSSSelectorList, StylePropertySet*);
    StyleRulePage(const StyleRulePage&);

    Member<StylePropertySet> m_properties; // Cannot be null.
    CSSSelectorList m_selectorList;
};

class StyleRuleGroup : public StyleRuleBase {
public:
    const HeapVector<Member<StyleRuleBase>>& childRules() const { return m_childRules; }

    void wrapperInsertRule(unsigned, StyleRuleBase*);
    void wrapperRemoveRule(unsigned);

    DECLARE_TRACE_AFTER_DISPATCH();

protected:
    StyleRuleGroup(RuleType, HeapVector<Member<StyleRuleBase>>& adoptRule);
    StyleRuleGroup(const StyleRuleGroup&);

private:
    HeapVector<Member<StyleRuleBase>> m_childRules;
};

class StyleRuleMedia : public StyleRuleGroup {
public:
    static StyleRuleMedia* create(MediaQuerySet* media, HeapVector<Member<StyleRuleBase>>& adoptRules)
    {
        return new StyleRuleMedia(media, adoptRules);
    }

    MediaQuerySet* mediaQueries() const { return m_mediaQueries.get(); }

    StyleRuleMedia* copy() const { return new StyleRuleMedia(*this); }

    DECLARE_TRACE_AFTER_DISPATCH();

private:
    StyleRuleMedia(MediaQuerySet*, HeapVector<Member<StyleRuleBase>>& adoptRules);
    StyleRuleMedia(const StyleRuleMedia&);

    Member<MediaQuerySet> m_mediaQueries;
};

class StyleRuleSupports : public StyleRuleGroup {
public:
    static StyleRuleSupports* create(const String& conditionText, bool conditionIsSupported, HeapVector<Member<StyleRuleBase>>& adoptRules)
    {
        return new StyleRuleSupports(conditionText, conditionIsSupported, adoptRules);
    }

    String conditionText() const { return m_conditionText; }
    bool conditionIsSupported() const { return m_conditionIsSupported; }
    StyleRuleSupports* copy() const { return new StyleRuleSupports(*this); }

    DEFINE_INLINE_TRACE_AFTER_DISPATCH() { StyleRuleGroup::traceAfterDispatch(visitor); }

private:
    StyleRuleSupports(const String& conditionText, bool conditionIsSupported, HeapVector<Member<StyleRuleBase>>& adoptRules);
    StyleRuleSupports(const StyleRuleSupports&);

    String m_conditionText;
    bool m_conditionIsSupported;
};

class StyleRuleViewport : public StyleRuleBase {
public:
    static StyleRuleViewport* create(StylePropertySet* properties)
    {
        return new StyleRuleViewport(properties);
    }

    ~StyleRuleViewport();

    const StylePropertySet& properties() const { return *m_properties; }
    MutableStylePropertySet& mutableProperties();

    StyleRuleViewport* copy() const { return new StyleRuleViewport(*this); }

    DECLARE_TRACE_AFTER_DISPATCH();

private:
    StyleRuleViewport(StylePropertySet*);
    StyleRuleViewport(const StyleRuleViewport&);

    Member<StylePropertySet> m_properties; // Cannot be null
};

// This should only be used within the CSS Parser
class StyleRuleCharset : public StyleRuleBase {
public:
    static StyleRuleCharset* create() { return new StyleRuleCharset(); }
    DEFINE_INLINE_TRACE_AFTER_DISPATCH() { StyleRuleBase::traceAfterDispatch(visitor); }

private:
    StyleRuleCharset() : StyleRuleBase(Charset) { }
};


#define DEFINE_STYLE_RULE_TYPE_CASTS(Type) \
    DEFINE_TYPE_CASTS(StyleRule##Type, StyleRuleBase, rule, rule->is##Type##Rule(), rule.is##Type##Rule())

DEFINE_TYPE_CASTS(StyleRule, StyleRuleBase, rule, rule->isStyleRule(), rule.isStyleRule());
DEFINE_STYLE_RULE_TYPE_CASTS(FontFace);
DEFINE_STYLE_RULE_TYPE_CASTS(Page);
DEFINE_STYLE_RULE_TYPE_CASTS(Media);
DEFINE_STYLE_RULE_TYPE_CASTS(Supports);
DEFINE_STYLE_RULE_TYPE_CASTS(Viewport);
DEFINE_STYLE_RULE_TYPE_CASTS(Charset);

} // namespace blink

#endif // StyleRule_h
