// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementAdoptedCallbackReaction.h"

#include "core/dom/Document.h"
#include "core/dom/custom/CustomElementDefinition.h"

namespace blink {

CustomElementAdoptedCallbackReaction::CustomElementAdoptedCallbackReaction(
    CustomElementDefinition* definition,
    Document* oldOwner,
    Document* newOwner)
    : CustomElementReaction(definition),
      m_oldOwner(oldOwner),
      m_newOwner(newOwner) {
  DCHECK(definition->hasAdoptedCallback());
}

DEFINE_TRACE(CustomElementAdoptedCallbackReaction) {
  CustomElementReaction::trace(visitor);
  visitor->trace(m_oldOwner);
  visitor->trace(m_newOwner);
}

void CustomElementAdoptedCallbackReaction::invoke(Element* element) {
  m_definition->runAdoptedCallback(element, m_oldOwner.get(), m_newOwner.get());
}

}  // namespace blink
