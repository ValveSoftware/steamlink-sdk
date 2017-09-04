// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ShadowListPropertyFunctions_h
#define ShadowListPropertyFunctions_h

#include "core/CSSPropertyNames.h"
#include "core/style/ComputedStyle.h"

namespace blink {

class ShadowListPropertyFunctions {
 public:
  static const ShadowList* getInitialShadowList(CSSPropertyID) {
    return nullptr;
  }
  static const ShadowList* getShadowList(CSSPropertyID property,
                                         const ComputedStyle& style) {
    switch (property) {
      case CSSPropertyBoxShadow:
        return style.boxShadow();
      case CSSPropertyTextShadow:
        return style.textShadow();
      default:
        NOTREACHED();
        return nullptr;
    }
  }
  static void setShadowList(CSSPropertyID property,
                            ComputedStyle& style,
                            PassRefPtr<ShadowList> shadowList) {
    switch (property) {
      case CSSPropertyBoxShadow:
        style.setBoxShadow(std::move(shadowList));
        return;
      case CSSPropertyTextShadow:
        style.setTextShadow(std::move(shadowList));
        return;
      default:
        NOTREACHED();
    }
  }
};

}  // namespace blink

#endif  // ShadowListPropertyFunctions_h
