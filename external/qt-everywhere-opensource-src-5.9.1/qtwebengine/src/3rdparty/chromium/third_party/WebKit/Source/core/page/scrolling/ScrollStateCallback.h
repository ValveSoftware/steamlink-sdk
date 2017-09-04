// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollStateCallback_h
#define ScrollStateCallback_h

#include "platform/heap/Handle.h"
#include "public/platform/WebNativeScrollBehavior.h"

namespace blink {

class ScrollState;

class ScrollStateCallback
    : public GarbageCollectedFinalized<ScrollStateCallback> {
 public:
  ScrollStateCallback()
      : m_nativeScrollBehavior(WebNativeScrollBehavior::DisableNativeScroll) {}

  virtual ~ScrollStateCallback() {}

  DEFINE_INLINE_VIRTUAL_TRACE() {}
  virtual void handleEvent(ScrollState*) = 0;

  void setNativeScrollBehavior(WebNativeScrollBehavior nativeScrollBehavior) {
    ASSERT(static_cast<int>(nativeScrollBehavior) < 3);
    m_nativeScrollBehavior = nativeScrollBehavior;
  }

  WebNativeScrollBehavior nativeScrollBehavior() {
    return m_nativeScrollBehavior;
  }

  static WebNativeScrollBehavior toNativeScrollBehavior(
      String nativeScrollBehavior);

 protected:
  WebNativeScrollBehavior m_nativeScrollBehavior;
};

}  // namespace blink

#endif  // ScrollStateCallback_h
