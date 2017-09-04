// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/ScrollStateCallback.h"

#include "wtf/text/WTFString.h"

namespace blink {

WebNativeScrollBehavior ScrollStateCallback::toNativeScrollBehavior(
    String nativeScrollBehavior) {
  static const char disable[] = "disable-native-scroll";
  static const char before[] = "perform-before-native-scroll";
  static const char after[] = "perform-after-native-scroll";

  if (nativeScrollBehavior == disable)
    return WebNativeScrollBehavior::DisableNativeScroll;
  if (nativeScrollBehavior == before)
    return WebNativeScrollBehavior::PerformBeforeNativeScroll;
  if (nativeScrollBehavior == after)
    return WebNativeScrollBehavior::PerformAfterNativeScroll;

  ASSERT_NOT_REACHED();
  return WebNativeScrollBehavior::DisableNativeScroll;
}

}  // namespace blink
