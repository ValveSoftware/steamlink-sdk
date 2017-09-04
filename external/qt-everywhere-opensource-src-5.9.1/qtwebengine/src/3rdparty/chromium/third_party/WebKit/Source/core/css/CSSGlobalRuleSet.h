// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSGlobalRuleSet_h
#define CSSGlobalRuleSet_h

#include "core/css/RuleFeature.h"

namespace blink {

class Document;
class RuleSet;

// A per Document collection of CSS metadata used for style matching and
// invalidation. The data is aggregated from author rulesets from all TreeScopes
// in the whole Document as well as UA stylesheets and watched selectors which
// apply to elements in all TreeScopes.
//
// TODO(rune@opera.com): We would like to move as much of this data as possible
// to the ScopedStyleResolver as possible to avoid full reconstruction of these
// rulesets on shadow tree changes. See https://crbug.com/401359

class CSSGlobalRuleSet {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(CSSGlobalRuleSet);

 public:
  CSSGlobalRuleSet() {}

  void initWatchedSelectorsRuleSet(Document&);
  void markDirty() { m_isDirty = true; }
  bool isDirty() const { return m_isDirty; }
  void update(Document&);

  const RuleFeatureSet& ruleFeatureSet() const {
    RELEASE_ASSERT(m_features.isAlive());
    return m_features;
  }
  RuleSet* siblingRuleSet() const { return m_siblingRuleSet; }
  RuleSet* uncommonAttributeRuleSet() const {
    return m_uncommonAttributeRuleSet;
  }
  RuleSet* watchedSelectorsRuleSet() const { return m_watchedSelectorsRuleSet; }
  bool hasFullscreenUAStyle() const { return m_hasFullscreenUAStyle; }

  DECLARE_TRACE();

 private:
  // Constructed from rules in all TreeScopes including UA style and style
  // injected from extensions.
  RuleFeatureSet m_features;
  Member<RuleSet> m_siblingRuleSet;
  Member<RuleSet> m_uncommonAttributeRuleSet;

  // Rules injected from extensions.
  Member<RuleSet> m_watchedSelectorsRuleSet;

  bool m_hasFullscreenUAStyle = false;
  bool m_isDirty = true;
};

}  // namespace blink

#endif  // CSSGlobalRuleSet_h
