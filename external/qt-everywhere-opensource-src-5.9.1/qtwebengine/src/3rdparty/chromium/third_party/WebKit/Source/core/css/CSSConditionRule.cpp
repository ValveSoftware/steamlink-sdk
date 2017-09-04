// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSConditionRule.h"

#include "core/css/CSSStyleSheet.h"

namespace blink {

CSSConditionRule::CSSConditionRule(StyleRuleCondition* conditionRule,
                                   CSSStyleSheet* parent)
    : CSSGroupingRule(conditionRule, parent) {}

CSSConditionRule::~CSSConditionRule() {}

String CSSConditionRule::conditionText() const {
  return static_cast<StyleRuleCondition*>(m_groupRule.get())->conditionText();
}

}  // namespace blink
