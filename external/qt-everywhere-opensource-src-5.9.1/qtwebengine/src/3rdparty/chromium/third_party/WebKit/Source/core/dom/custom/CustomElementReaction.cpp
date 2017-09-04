// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementReaction.h"

#include "core/dom/custom/CustomElementDefinition.h"

namespace blink {

CustomElementReaction::CustomElementReaction(
    CustomElementDefinition* definition)
    : m_definition(definition) {}

DEFINE_TRACE(CustomElementReaction) {
  visitor->trace(m_definition);
}

}  // namespace blink
