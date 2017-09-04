// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollCustomizationCallbacks_h
#define ScrollCustomizationCallbacks_h

#include "core/CoreExport.h"
#include "core/dom/Element.h"
#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"

namespace blink {

class ScrollStateCallback;

class CORE_EXPORT ScrollCustomizationCallbacks
    : public GarbageCollected<ScrollCustomizationCallbacks> {
  WTF_MAKE_NONCOPYABLE(ScrollCustomizationCallbacks);

 public:
  ScrollCustomizationCallbacks() {}
  void setDistributeScroll(Element*, ScrollStateCallback*);
  ScrollStateCallback* getDistributeScroll(Element*);
  void setApplyScroll(Element*, ScrollStateCallback*);
  void removeApplyScroll(Element*);
  ScrollStateCallback* getApplyScroll(Element*);

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_applyScrollCallbacks);
    visitor->trace(m_distributeScrollCallbacks);
  };

 private:
  using ScrollStateCallbackList =
      HeapHashMap<WeakMember<Element>, Member<ScrollStateCallback>>;
  ScrollStateCallbackList m_applyScrollCallbacks;
  ScrollStateCallbackList m_distributeScrollCallbacks;
};

}  // namespace blink

#endif  // ScrollCustomizationCallbacks_h
