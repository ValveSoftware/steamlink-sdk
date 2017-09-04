// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ResizeObserverCallback_h
#define ResizeObserverCallback_h

#include "platform/heap/Handle.h"

namespace blink {

class ResizeObserver;
class ResizeObserverEntry;

class ResizeObserverCallback
    : public GarbageCollectedFinalized<ResizeObserverCallback> {
 public:
  virtual ~ResizeObserverCallback() {}
  virtual void handleEvent(
      const HeapVector<Member<ResizeObserverEntry>>& entries,
      ResizeObserver*) = 0;
  DEFINE_INLINE_VIRTUAL_TRACE() {}
};

}  // namespace blink

#endif  // ResizeObserverCallback_h
