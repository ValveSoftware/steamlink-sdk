// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementConnectedCallbackReaction.h"

#include "core/dom/custom/CustomElementDefinition.h"

namespace blink {

CustomElementConnectedCallbackReaction::CustomElementConnectedCallbackReaction(
    CustomElementDefinition* definition)
    : CustomElementReaction(definition)
{
    DCHECK(definition->hasConnectedCallback());
}

void CustomElementConnectedCallbackReaction::invoke(Element* element)
{
    m_definition->runConnectedCallback(element);
}

} // namespace blink
