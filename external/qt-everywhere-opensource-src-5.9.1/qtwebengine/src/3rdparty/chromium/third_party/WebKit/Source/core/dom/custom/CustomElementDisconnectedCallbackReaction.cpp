// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementDisconnectedCallbackReaction.h"

#include "core/dom/custom/CustomElementDefinition.h"

namespace blink {

CustomElementDisconnectedCallbackReaction::
    CustomElementDisconnectedCallbackReaction(
        CustomElementDefinition* definition)
    : CustomElementReaction(definition) {
  DCHECK(definition->hasDisconnectedCallback());
}

void CustomElementDisconnectedCallbackReaction::invoke(Element* element) {
  m_definition->runDisconnectedCallback(element);
}

}  // namespace blink
