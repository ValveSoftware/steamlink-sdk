/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
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

#include "core/css/StyleRule.h"

#include "core/css/CSSFontFaceRule.h"
#include "core/css/CSSImportRule.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/css/CSSMediaRule.h"
#include "core/css/CSSNamespaceRule.h"
#include "core/css/CSSPageRule.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSSupportsRule.h"
#include "core/css/CSSViewportRule.h"
#include "core/css/StyleRuleImport.h"
#include "core/css/StyleRuleKeyframe.h"
#include "core/css/StyleRuleNamespace.h"

namespace blink {

struct SameSizeAsStyleRuleBase
    : public GarbageCollectedFinalized<SameSizeAsStyleRuleBase> {
  unsigned bitfields;
};

static_assert(sizeof(StyleRuleBase) <= sizeof(SameSizeAsStyleRuleBase),
              "StyleRuleBase should stay small");

CSSRule* StyleRuleBase::createCSSOMWrapper(CSSStyleSheet* parentSheet) const {
  return createCSSOMWrapper(parentSheet, 0);
}

CSSRule* StyleRuleBase::createCSSOMWrapper(CSSRule* parentRule) const {
  return createCSSOMWrapper(0, parentRule);
}

DEFINE_TRACE(StyleRuleBase) {
  switch (type()) {
    case Charset:
      toStyleRuleCharset(this)->traceAfterDispatch(visitor);
      return;
    case Style:
      toStyleRule(this)->traceAfterDispatch(visitor);
      return;
    case Page:
      toStyleRulePage(this)->traceAfterDispatch(visitor);
      return;
    case FontFace:
      toStyleRuleFontFace(this)->traceAfterDispatch(visitor);
      return;
    case Media:
      toStyleRuleMedia(this)->traceAfterDispatch(visitor);
      return;
    case Supports:
      toStyleRuleSupports(this)->traceAfterDispatch(visitor);
      return;
    case Import:
      toStyleRuleImport(this)->traceAfterDispatch(visitor);
      return;
    case Keyframes:
      toStyleRuleKeyframes(this)->traceAfterDispatch(visitor);
      return;
    case Keyframe:
      toStyleRuleKeyframe(this)->traceAfterDispatch(visitor);
      return;
    case Namespace:
      toStyleRuleNamespace(this)->traceAfterDispatch(visitor);
      return;
    case Viewport:
      toStyleRuleViewport(this)->traceAfterDispatch(visitor);
      return;
  }
  ASSERT_NOT_REACHED();
}

void StyleRuleBase::finalizeGarbageCollectedObject() {
  switch (type()) {
    case Charset:
      toStyleRuleCharset(this)->~StyleRuleCharset();
      return;
    case Style:
      toStyleRule(this)->~StyleRule();
      return;
    case Page:
      toStyleRulePage(this)->~StyleRulePage();
      return;
    case FontFace:
      toStyleRuleFontFace(this)->~StyleRuleFontFace();
      return;
    case Media:
      toStyleRuleMedia(this)->~StyleRuleMedia();
      return;
    case Supports:
      toStyleRuleSupports(this)->~StyleRuleSupports();
      return;
    case Import:
      toStyleRuleImport(this)->~StyleRuleImport();
      return;
    case Keyframes:
      toStyleRuleKeyframes(this)->~StyleRuleKeyframes();
      return;
    case Keyframe:
      toStyleRuleKeyframe(this)->~StyleRuleKeyframe();
      return;
    case Namespace:
      toStyleRuleNamespace(this)->~StyleRuleNamespace();
      return;
    case Viewport:
      toStyleRuleViewport(this)->~StyleRuleViewport();
      return;
  }
  ASSERT_NOT_REACHED();
}

StyleRuleBase* StyleRuleBase::copy() const {
  switch (type()) {
    case Style:
      return toStyleRule(this)->copy();
    case Page:
      return toStyleRulePage(this)->copy();
    case FontFace:
      return toStyleRuleFontFace(this)->copy();
    case Media:
      return toStyleRuleMedia(this)->copy();
    case Supports:
      return toStyleRuleSupports(this)->copy();
    case Import:
      // FIXME: Copy import rules.
      ASSERT_NOT_REACHED();
      return nullptr;
    case Keyframes:
      return toStyleRuleKeyframes(this)->copy();
    case Viewport:
      return toStyleRuleViewport(this)->copy();
    case Namespace:
      return toStyleRuleNamespace(this)->copy();
    case Charset:
    case Keyframe:
      ASSERT_NOT_REACHED();
      return nullptr;
  }
  ASSERT_NOT_REACHED();
  return nullptr;
}

CSSRule* StyleRuleBase::createCSSOMWrapper(CSSStyleSheet* parentSheet,
                                           CSSRule* parentRule) const {
  CSSRule* rule = nullptr;
  StyleRuleBase* self = const_cast<StyleRuleBase*>(this);
  switch (type()) {
    case Style:
      rule = CSSStyleRule::create(toStyleRule(self), parentSheet);
      break;
    case Page:
      rule = CSSPageRule::create(toStyleRulePage(self), parentSheet);
      break;
    case FontFace:
      rule = CSSFontFaceRule::create(toStyleRuleFontFace(self), parentSheet);
      break;
    case Media:
      rule = CSSMediaRule::create(toStyleRuleMedia(self), parentSheet);
      break;
    case Supports:
      rule = CSSSupportsRule::create(toStyleRuleSupports(self), parentSheet);
      break;
    case Import:
      rule = CSSImportRule::create(toStyleRuleImport(self), parentSheet);
      break;
    case Keyframes:
      rule = CSSKeyframesRule::create(toStyleRuleKeyframes(self), parentSheet);
      break;
    case Namespace:
      rule = CSSNamespaceRule::create(toStyleRuleNamespace(self), parentSheet);
      break;
    case Viewport:
      rule = CSSViewportRule::create(toStyleRuleViewport(self), parentSheet);
      break;
    case Keyframe:
    case Charset:
      ASSERT_NOT_REACHED();
      return nullptr;
  }
  if (parentRule)
    rule->setParentRule(parentRule);
  return rule;
}

unsigned StyleRule::averageSizeInBytes() {
  return sizeof(StyleRule) + sizeof(CSSSelector) +
         StylePropertySet::averageSizeInBytes();
}

StyleRule::StyleRule(CSSSelectorList selectorList, StylePropertySet* properties)
    : StyleRuleBase(Style),
      m_selectorList(std::move(selectorList)),
      m_properties(properties),
      m_shouldConsiderForMatchingRules(ConsiderIfNonEmpty) {}

StyleRule::StyleRule(CSSSelectorList selectorList,
                     CSSLazyPropertyParser* lazyPropertyParser)
    : StyleRuleBase(Style),
      m_selectorList(std::move(selectorList)),
      m_lazyPropertyParser(lazyPropertyParser),
      m_shouldConsiderForMatchingRules(AlwaysConsider) {}

const StylePropertySet& StyleRule::properties() const {
  if (!m_properties) {
    m_properties = m_lazyPropertyParser->parseProperties();
    m_lazyPropertyParser.clear();
  }
  return *m_properties;
}

StyleRule::StyleRule(const StyleRule& o)
    : StyleRuleBase(o),
      m_selectorList(o.m_selectorList.copy()),
      m_properties(o.properties().mutableCopy()),
      m_shouldConsiderForMatchingRules(ConsiderIfNonEmpty) {}

StyleRule::~StyleRule() {}

MutableStylePropertySet& StyleRule::mutableProperties() {
  // Ensure m_properties is initialized.
  if (!properties().isMutable())
    m_properties = m_properties->mutableCopy();
  return *toMutableStylePropertySet(m_properties.get());
}

bool StyleRule::propertiesHaveFailedOrCanceledSubresources() const {
  return m_properties && m_properties->hasFailedOrCanceledSubresources();
}

bool StyleRule::shouldConsiderForMatchingRules(bool includeEmptyRules) const {
  DCHECK(m_shouldConsiderForMatchingRules == AlwaysConsider || m_properties);
  return includeEmptyRules ||
         m_shouldConsiderForMatchingRules == AlwaysConsider ||
         !m_properties->isEmpty();
}

bool StyleRule::hasParsedProperties() const {
  // StyleRule should only have one of {m_lazyPropertyParser, m_properties} set.
  DCHECK(m_lazyPropertyParser || m_properties);
  DCHECK(!m_lazyPropertyParser || !m_properties);
  return !m_lazyPropertyParser;
}

DEFINE_TRACE_AFTER_DISPATCH(StyleRule) {
  visitor->trace(m_properties);
  visitor->trace(m_lazyPropertyParser);
  StyleRuleBase::traceAfterDispatch(visitor);
}

StyleRulePage::StyleRulePage(CSSSelectorList selectorList,
                             StylePropertySet* properties)
    : StyleRuleBase(Page),
      m_properties(properties),
      m_selectorList(std::move(selectorList)) {}

StyleRulePage::StyleRulePage(const StyleRulePage& pageRule)
    : StyleRuleBase(pageRule),
      m_properties(pageRule.m_properties->mutableCopy()),
      m_selectorList(pageRule.m_selectorList.copy()) {}

StyleRulePage::~StyleRulePage() {}

MutableStylePropertySet& StyleRulePage::mutableProperties() {
  if (!m_properties->isMutable())
    m_properties = m_properties->mutableCopy();
  return *toMutableStylePropertySet(m_properties.get());
}

DEFINE_TRACE_AFTER_DISPATCH(StyleRulePage) {
  visitor->trace(m_properties);
  StyleRuleBase::traceAfterDispatch(visitor);
}

StyleRuleFontFace::StyleRuleFontFace(StylePropertySet* properties)
    : StyleRuleBase(FontFace), m_properties(properties) {}

StyleRuleFontFace::StyleRuleFontFace(const StyleRuleFontFace& fontFaceRule)
    : StyleRuleBase(fontFaceRule),
      m_properties(fontFaceRule.m_properties->mutableCopy()) {}

StyleRuleFontFace::~StyleRuleFontFace() {}

MutableStylePropertySet& StyleRuleFontFace::mutableProperties() {
  if (!m_properties->isMutable())
    m_properties = m_properties->mutableCopy();
  return *toMutableStylePropertySet(m_properties);
}

DEFINE_TRACE_AFTER_DISPATCH(StyleRuleFontFace) {
  visitor->trace(m_properties);
  StyleRuleBase::traceAfterDispatch(visitor);
}

StyleRuleGroup::StyleRuleGroup(RuleType type,
                               HeapVector<Member<StyleRuleBase>>& adoptRule)
    : StyleRuleBase(type) {
  m_childRules.swap(adoptRule);
}

StyleRuleGroup::StyleRuleGroup(const StyleRuleGroup& groupRule)
    : StyleRuleBase(groupRule), m_childRules(groupRule.m_childRules.size()) {
  for (unsigned i = 0; i < m_childRules.size(); ++i)
    m_childRules[i] = groupRule.m_childRules[i]->copy();
}

void StyleRuleGroup::wrapperInsertRule(unsigned index, StyleRuleBase* rule) {
  m_childRules.insert(index, rule);
}

void StyleRuleGroup::wrapperRemoveRule(unsigned index) {
  m_childRules.remove(index);
}

DEFINE_TRACE_AFTER_DISPATCH(StyleRuleGroup) {
  visitor->trace(m_childRules);
  StyleRuleBase::traceAfterDispatch(visitor);
}

StyleRuleCondition::StyleRuleCondition(
    RuleType type,
    HeapVector<Member<StyleRuleBase>>& adoptRules)
    : StyleRuleGroup(type, adoptRules) {}

StyleRuleCondition::StyleRuleCondition(
    RuleType type,
    const String& conditionText,
    HeapVector<Member<StyleRuleBase>>& adoptRules)
    : StyleRuleGroup(type, adoptRules), m_conditionText(conditionText) {}

StyleRuleCondition::StyleRuleCondition(const StyleRuleCondition& conditionRule)
    : StyleRuleGroup(conditionRule),
      m_conditionText(conditionRule.m_conditionText) {}

StyleRuleMedia::StyleRuleMedia(MediaQuerySet* media,
                               HeapVector<Member<StyleRuleBase>>& adoptRules)
    : StyleRuleCondition(Media, adoptRules), m_mediaQueries(media) {}

StyleRuleMedia::StyleRuleMedia(const StyleRuleMedia& mediaRule)
    : StyleRuleCondition(mediaRule) {
  if (mediaRule.m_mediaQueries)
    m_mediaQueries = mediaRule.m_mediaQueries->copy();
}

DEFINE_TRACE_AFTER_DISPATCH(StyleRuleMedia) {
  visitor->trace(m_mediaQueries);
  StyleRuleCondition::traceAfterDispatch(visitor);
}

StyleRuleSupports::StyleRuleSupports(
    const String& conditionText,
    bool conditionIsSupported,
    HeapVector<Member<StyleRuleBase>>& adoptRules)
    : StyleRuleCondition(Supports, conditionText, adoptRules),
      m_conditionIsSupported(conditionIsSupported) {}

StyleRuleSupports::StyleRuleSupports(const StyleRuleSupports& supportsRule)
    : StyleRuleCondition(supportsRule),
      m_conditionIsSupported(supportsRule.m_conditionIsSupported) {}

StyleRuleViewport::StyleRuleViewport(StylePropertySet* properties)
    : StyleRuleBase(Viewport), m_properties(properties) {}

StyleRuleViewport::StyleRuleViewport(const StyleRuleViewport& viewportRule)
    : StyleRuleBase(viewportRule),
      m_properties(viewportRule.m_properties->mutableCopy()) {}

StyleRuleViewport::~StyleRuleViewport() {}

MutableStylePropertySet& StyleRuleViewport::mutableProperties() {
  if (!m_properties->isMutable())
    m_properties = m_properties->mutableCopy();
  return *toMutableStylePropertySet(m_properties);
}

DEFINE_TRACE_AFTER_DISPATCH(StyleRuleViewport) {
  visitor->trace(m_properties);
  StyleRuleBase::traceAfterDispatch(visitor);
}

}  // namespace blink
