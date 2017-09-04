// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyIDTemplates_h
#define CSSPropertyIDTemplates_h

#include "core/CSSPropertyNames.h"
#include "wtf/HashFunctions.h"
#include "wtf/HashTraits.h"

namespace WTF {
template <>
struct DefaultHash<blink::CSSPropertyID> {
  typedef IntHash<unsigned> Hash;
};
template <>
struct HashTraits<blink::CSSPropertyID>
    : GenericHashTraits<blink::CSSPropertyID> {
  static const bool emptyValueIsZero = true;
  static void constructDeletedValue(blink::CSSPropertyID& slot, bool) {
    slot =
        static_cast<blink::CSSPropertyID>(blink::lastUnresolvedCSSProperty + 1);
  }
  static bool isDeletedValue(blink::CSSPropertyID value) {
    return value == (blink::lastUnresolvedCSSProperty + 1);
  }
};
}  // namespace WTF

#endif  // CSSPropertyIDTemplates_h
