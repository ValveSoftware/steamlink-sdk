// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef KeyframeStyleRuleCSSStyleDeclaration_h
#define KeyframeStyleRuleCSSStyleDeclaration_h

#include "core/css/PropertySetCSSStyleDeclaration.h"

namespace blink {

class CSSKeyframeRule;

class KeyframeStyleRuleCSSStyleDeclaration final
    : public StyleRuleCSSStyleDeclaration {
 public:
  static KeyframeStyleRuleCSSStyleDeclaration* create(
      MutableStylePropertySet& propertySet,
      CSSKeyframeRule* parentRule) {
    return new KeyframeStyleRuleCSSStyleDeclaration(propertySet, parentRule);
  }

 private:
  KeyframeStyleRuleCSSStyleDeclaration(MutableStylePropertySet&,
                                       CSSKeyframeRule*);

  void didMutate(MutationType) override;
  bool isKeyframeStyle() const final { return true; }
};

}  // namespace blink

#endif
