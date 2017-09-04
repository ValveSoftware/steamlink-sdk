// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/resolver/StyleRuleUsageTracker.h"

#include "core/css/StyleRule.h"

namespace blink {

bool StyleRuleUsageTracker::contains(StyleRule* rule) const {
  return m_ruleList.contains(rule);
}

DEFINE_TRACE(StyleRuleUsageTracker) {
  visitor->trace(m_ruleList);
}

}  // namespace blink
