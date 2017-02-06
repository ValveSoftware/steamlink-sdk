// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementUpgradeReaction.h"

#include "core/dom/custom/CustomElementDefinition.h"

namespace blink {

CustomElementUpgradeReaction::CustomElementUpgradeReaction(
    CustomElementDefinition* definition)
    : CustomElementReaction(definition)
{
}

void CustomElementUpgradeReaction::invoke(Element* element)
{
    // Don't call upgrade() if it's already upgraded. Multiple upgrade reactions
    // could be enqueued because the state changes in step 10 of upgrades.
    // https://html.spec.whatwg.org/multipage/scripting.html#upgrades
    if (element->getCustomElementState() == CustomElementState::Undefined)
        m_definition->upgrade(element);
}

} // namespace blink
