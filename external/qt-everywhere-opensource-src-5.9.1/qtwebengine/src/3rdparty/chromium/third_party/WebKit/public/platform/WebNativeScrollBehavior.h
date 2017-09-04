// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebNativeScrollBehavior_h
#define WebNativeScrollBehavior_h

namespace blink {

enum class WebNativeScrollBehavior {
  DisableNativeScroll,
  PerformBeforeNativeScroll,
  PerformAfterNativeScroll,
};

}  // namespace blink

#endif  // WebNativeScrollBehavior_h
