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

class CORE_EXPORT StyleRuleBase
    : public GarbageCollectedFinalized<StyleRuleBase> {
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
  DEFINE_INLINE_TRACE_AFTER_DISPATCH() {}
  void finalizeGarbageCollectedObject();

  // ~StyleRuleBase should be public, because non-public ~StyleRuleBase
  // causes C2248 error : 'blink::StyleRuleBase::~StyleRuleBase' : cannot
  // access protected member declared in class 'blink::StyleRuleBase' when
  // compiling 'source\wtf\refcounted.h' by using msvc.
  ~StyleRuleBase() {}

 protected:
  StyleRuleBase(RuleType type) : m_type(type) {}
  StyleRuleBase(const StyleRuleBase& rule) : m_type(rule.m_type) {}

 private:
  CSSRule* createCSSOMWrapper(CSSStyleSheet* parentSheet,
                              CSSRule* parentRule) const;

  unsigned m_type : 5;
};

class CORE_EXPORT StyleRule : public StyleRuleBase {
 public:
  // Adopts the selector list
  static StyleRule* create(CSSSelectorList selectorList,
                           StylePropertySet* properties) {
    return new StyleRule(std::move(selectorList), properties);
  }
  static StyleRule* createLazy(CSSSelectorList selectorList,
                               CSSLazyPropertyParser* lazyPropertyParser) {
    return new StyleRule(std::move(selectorList), lazyPropertyParser);
  }

  ~StyleRule();

  const CSSSelectorList& selectorList() const { return m_selectorList; }
  const StylePropertySet& properties() const;
  MutableStylePropertySet& mutableProperties();

  void wrapperAdoptSelectorList(CSSSelectorList selectors) {
    m_selectorList = std::move(selectors);
  }

  StyleRule* copy() const { return new StyleRule(*this); }

  static unsigned averageSizeInBytes();

  // Helper methods to avoid parsing lazy properties when not needed.
  bool propertiesHaveFailedOrCanceledSubresources() const;
  bool shouldConsiderForMatchingRules(bool includeEmptyRules) const;

  DECLARE_TRACE_AFTER_DISPATCH();

 private:
  friend class CSSLazyParsingTest;
  bool hasParsedProperties() const;

  StyleRule(CSSSelectorList, StylePropertySet*);
  StyleRule(CSSSelectorList, CSSLazyPropertyParser*);
  StyleRule(const StyleRule&);

  CSSSelectorList m_selectorList;
  mutable Member<StylePropertySet> m_properties;
  mutable Member<CSSLazyPropertyParser> m_lazyPropertyParser;

  // Whether or not we should consider this for matching rules. Usually we try
  // to avoid considering empty property sets, as an optimization. This is
  // not possible for lazy properties, which always need to be considered. The
  // lazy parser does its best to avoid lazy parsing for properties that look
  // empty due to lack of tokens.
  enum ConsiderForMatching {
    AlwaysConsider,
    ConsiderIfNonEmpty,
  };
  mutable ConsiderForMatching m_shouldConsiderForMatchingRules;
};

class StyleRuleFontFace : public StyleRuleBase {
 public:
  static StyleRuleFontFace* create(StylePropertySet* properties) {
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

  Member<StylePropertySet> m_properties;  // Cannot be null.
};

class StyleRulePage : public StyleRuleBase {
 public:
  // Adopts the selector list
  static StyleRulePage* create(CSSSelectorList selectorList,
                               StylePropertySet* properties) {
    return new StyleRulePage(std::move(selectorList), properties);
  }

  ~StyleRulePage();

  const CSSSelector* selector() const { return m_selectorList.first(); }
  const StylePropertySet& properties() const { return *m_properties; }
  MutableStylePropertySet& mutableProperties();

  void wrapperAdoptSelectorList(CSSSelectorList selectors) {
    m_selectorList = std::move(selectors);
  }

  StyleRulePage* copy() const { return new StyleRulePage(*this); }

  DECLARE_TRACE_AFTER_DISPATCH();

 private:
  StyleRulePage(CSSSelectorList, StylePropertySet*);
  StyleRulePage(const StyleRulePage&);

  Member<StylePropertySet> m_properties;  // Cannot be null.
  CSSSelectorList m_selectorList;
};

class CORE_EXPORT StyleRuleGroup : public StyleRuleBase {
 public:
  const HeapVector<Member<StyleRuleBase>>& childRules() const {
    return m_childRules;
  }

  void wrapperInsertRule(unsigned, StyleRuleBase*);
  void wrapperRemoveRule(unsigned);

  DECLARE_TRACE_AFTER_DISPATCH();

 protected:
  StyleRuleGroup(RuleType, HeapVector<Member<StyleRuleBase>>& adoptRule);
  StyleRuleGroup(const StyleRuleGroup&);

 private:
  HeapVector<Member<StyleRuleBase>> m_childRules;
};

class CORE_EXPORT StyleRuleCondition : public StyleRuleGroup {
 public:
  String conditionText() const { return m_conditionText; }

  DEFINE_INLINE_TRACE_AFTER_DISPATCH() {
    StyleRuleGroup::traceAfterDispatch(visitor);
  }

 protected:
  StyleRuleCondition(RuleType, HeapVector<Member<StyleRuleBase>>& adoptRule);
  StyleRuleCondition(RuleType,
                     const String& conditionText,
                     HeapVector<Member<StyleRuleBase>>& adoptRule);
  StyleRuleCondition(const StyleRuleCondition&);
  String m_conditionText;
};

class CORE_EXPORT StyleRuleMedia : public StyleRuleCondition {
 public:
  static StyleRuleMedia* create(MediaQuerySet* media,
                                HeapVector<Member<StyleRuleBase>>& adoptRules) {
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

class StyleRuleSupports : public StyleRuleCondition {
 public:
  static StyleRuleSupports* create(
      const String& conditionText,
      bool conditionIsSupported,
      HeapVector<Member<StyleRuleBase>>& adoptRules) {
    return new StyleRuleSupports(conditionText, conditionIsSupported,
                                 adoptRules);
  }

  bool conditionIsSupported() const { return m_conditionIsSupported; }
  StyleRuleSupports* copy() const { return new StyleRuleSupports(*this); }

  DEFINE_INLINE_TRACE_AFTER_DISPATCH() {
    StyleRuleCondition::traceAfterDispatch(visitor);
  }

 private:
  StyleRuleSupports(const String& conditionText,
                    bool conditionIsSupported,
                    HeapVector<Member<StyleRuleBase>>& adoptRules);
  StyleRuleSupports(const StyleRuleSupports&);

  String m_conditionText;
  bool m_conditionIsSupported;
};

class StyleRuleViewport : public StyleRuleBase {
 public:
  static StyleRuleViewport* create(StylePropertySet* properties) {
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

  Member<StylePropertySet> m_properties;  // Cannot be null
};

// This should only be used within the CSS Parser
class StyleRuleCharset : public StyleRuleBase {
 public:
  static StyleRuleCharset* create() { return new StyleRuleCharset(); }
  DEFINE_INLINE_TRACE_AFTER_DISPATCH() {
    StyleRuleBase::traceAfterDispatch(visitor);
  }

 private:
  StyleRuleCharset() : StyleRuleBase(Charset) {}
};

#define DEFINE_STYLE_RULE_TYPE_CASTS(Type)                \
  DEFINE_TYPE_CASTS(StyleRule##Type, StyleRuleBase, rule, \
                    rule->is##Type##Rule(), rule.is##Type##Rule())

DEFINE_TYPE_CASTS(StyleRule,
                  StyleRuleBase,
                  rule,
                  rule->isStyleRule(),
                  rule.isStyleRule());
DEFINE_STYLE_RULE_TYPE_CASTS(FontFace);
DEFINE_STYLE_RULE_TYPE_CASTS(Page);
DEFINE_STYLE_RULE_TYPE_CASTS(Media);
DEFINE_STYLE_RULE_TYPE_CASTS(Supports);
DEFINE_STYLE_RULE_TYPE_CASTS(Viewport);
DEFINE_STYLE_RULE_TYPE_CASTS(Charset);

}  // namespace blink

#endif  // StyleRule_h
