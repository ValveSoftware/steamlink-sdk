// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSGlobalRuleSet.h"

#include "core/css/CSSDefaultStyleSheets.h"
#include "core/css/RuleSet.h"
#include "core/dom/CSSSelectorWatch.h"
#include "core/dom/Document.h"
#include "core/dom/StyleEngine.h"

namespace blink {

void CSSGlobalRuleSet::initWatchedSelectorsRuleSet(Document& document) {
  markDirty();
  m_watchedSelectorsRuleSet = nullptr;
  CSSSelectorWatch* watch = CSSSelectorWatch::fromIfExists(document);
  if (!watch)
    return;
  const HeapVector<Member<StyleRule>>& watchedSelectors =
      watch->watchedCallbackSelectors();
  if (!watchedSelectors.size())
    return;
  m_watchedSelectorsRuleSet = RuleSet::create();
  for (unsigned i = 0; i < watchedSelectors.size(); ++i) {
    m_watchedSelectorsRuleSet->addStyleRule(watchedSelectors[i],
                                            RuleHasNoSpecialState);
  }
}

static RuleSet* makeRuleSet(const HeapVector<RuleFeature>& rules) {
  size_t size = rules.size();
  if (!size)
    return nullptr;
  RuleSet* ruleSet = RuleSet::create();
  for (size_t i = 0; i < size; ++i) {
    ruleSet->addRule(rules[i].rule, rules[i].selectorIndex,
                     rules[i].hasDocumentSecurityOrigin
                         ? RuleHasDocumentSecurityOrigin
                         : RuleHasNoSpecialState);
  }
  return ruleSet;
}

void CSSGlobalRuleSet::update(Document& document) {
  if (!m_isDirty)
    return;

  m_isDirty = false;
  m_features.clear();
  m_hasFullscreenUAStyle = false;

  CSSDefaultStyleSheets& defaultStyleSheets = CSSDefaultStyleSheets::instance();
  if (defaultStyleSheets.defaultStyle()) {
    m_features.add(defaultStyleSheets.defaultStyle()->features());
    m_hasFullscreenUAStyle = defaultStyleSheets.fullscreenStyleSheet();
  }

  if (document.isViewSource())
    m_features.add(defaultStyleSheets.defaultViewSourceStyle()->features());

  if (m_watchedSelectorsRuleSet)
    m_features.add(m_watchedSelectorsRuleSet->features());

  document.styleEngine().collectScopedStyleFeaturesTo(m_features);

  m_siblingRuleSet = makeRuleSet(m_features.siblingRules());
  m_uncommonAttributeRuleSet = makeRuleSet(m_features.uncommonAttributeRules());
}

DEFINE_TRACE(CSSGlobalRuleSet) {
  visitor->trace(m_features);
  visitor->trace(m_siblingRuleSet);
  visitor->trace(m_uncommonAttributeRuleSet);
  visitor->trace(m_watchedSelectorsRuleSet);
}

}  // namespace blink
