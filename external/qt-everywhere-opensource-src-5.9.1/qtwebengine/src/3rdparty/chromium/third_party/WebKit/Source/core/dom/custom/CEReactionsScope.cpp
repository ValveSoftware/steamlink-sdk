// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CEReactionsScope.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/custom/CustomElementReactionStack.h"

namespace blink {

CEReactionsScope* CEReactionsScope::s_topOfStack = nullptr;

void CEReactionsScope::enqueueToCurrentQueue(Element* element,
                                             CustomElementReaction* reaction) {
  if (!m_workToDo) {
    m_workToDo = true;
    CustomElementReactionStack::current().push();
  }
  CustomElementReactionStack::current().enqueueToCurrentQueue(element,
                                                              reaction);
}

void CEReactionsScope::invokeReactions() {
  CustomElementReactionStack::current().popInvokingReactions();
}

}  // namespace blink
