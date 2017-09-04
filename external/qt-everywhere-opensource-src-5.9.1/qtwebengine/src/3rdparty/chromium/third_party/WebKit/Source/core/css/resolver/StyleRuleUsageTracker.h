// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleRuleUsageTracker_h
#define StyleRuleUsageTracker_h

#include "core/css/CSSStyleRule.h"

namespace blink {

class StyleRule;

class StyleRuleUsageTracker : public GarbageCollected<StyleRuleUsageTracker> {
 public:
  void track(StyleRule* rule) { m_ruleList.add(rule); }

  bool contains(StyleRule*) const;

  DECLARE_TRACE();

 private:
  HeapHashSet<Member<StyleRule>> m_ruleList;
};

}  // namespace blink

#endif  // StyleRuleUsageTracker_h
