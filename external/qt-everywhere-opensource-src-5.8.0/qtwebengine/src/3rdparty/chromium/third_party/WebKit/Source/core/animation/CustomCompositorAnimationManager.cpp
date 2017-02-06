// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CustomCompositorAnimationManager.h"

#include "core/dom/DOMNodeIds.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "platform/TraceEvent.h"
#include "platform/graphics/CompositorMutation.h"

namespace blink {

CustomCompositorAnimationManager::CustomCompositorAnimationManager()
{
}

CustomCompositorAnimationManager::~CustomCompositorAnimationManager()
{
}

void CustomCompositorAnimationManager::applyMutations(CompositorMutations* mutations)
{
    TRACE_EVENT0("compositor-worker", "CustomCompositorAnimationManager::applyMutations");
    for (const auto& entry : mutations->map) {
        int elementId = entry.key;
        const CompositorMutation& mutation = *entry.value;
        Node* node = DOMNodeIds::nodeForId(elementId);
        if (!node || !node->isElementNode())
            continue;
        toElement(node)->updateFromCompositorMutation(mutation);
    }
}

} // namespace blink
