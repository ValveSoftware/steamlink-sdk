// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSConditionRule_h
#define CSSConditionRule_h

#include "core/css/CSSGroupingRule.h"

namespace blink {

class CSSConditionRule : public CSSGroupingRule {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~CSSConditionRule() override;

  virtual String conditionText() const;

 protected:
  CSSConditionRule(StyleRuleCondition* conditionRule, CSSStyleSheet* parent);
};

}  // namespace blink

#endif  // CSSConditionRule_h
