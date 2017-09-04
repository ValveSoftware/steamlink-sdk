// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEReactionsScope_h
#define CEReactionsScope_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/StdLibExtras.h"

namespace blink {

class CustomElementReaction;
class Element;

// https://html.spec.whatwg.org/multipage/scripting.html#cereactions
class CORE_EXPORT CEReactionsScope final {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(CEReactionsScope);

 public:
  static CEReactionsScope* current() { return s_topOfStack; }

  CEReactionsScope() : m_prev(s_topOfStack), m_workToDo(false) {
    s_topOfStack = this;
  }

  ~CEReactionsScope() {
    if (m_workToDo)
      invokeReactions();
    s_topOfStack = s_topOfStack->m_prev;
  }

  void enqueueToCurrentQueue(Element*, CustomElementReaction*);

 private:
  static CEReactionsScope* s_topOfStack;

  void invokeReactions();

  CEReactionsScope* m_prev;
  bool m_workToDo;
};

}  // namespace blink

#endif  // CEReactionsScope_h
