// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementAttributeChangedCallbackReaction.h"

#include "core/dom/custom/CustomElementDefinition.h"

namespace blink {

CustomElementAttributeChangedCallbackReaction::CustomElementAttributeChangedCallbackReaction(
    CustomElementDefinition* definition,
    const QualifiedName& name,
    const AtomicString& oldValue, const AtomicString& newValue)
    : CustomElementReaction(definition)
    , m_name(name)
    , m_oldValue(oldValue)
    , m_newValue(newValue)
{
    DCHECK(definition->hasAttributeChangedCallback(name));
}

void CustomElementAttributeChangedCallbackReaction::invoke(Element* element)
{
    m_definition->runAttributeChangedCallback(element, m_name, m_oldValue, m_newValue);
}

} // namespace blink
