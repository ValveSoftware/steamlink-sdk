// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CompositorMutableStateProvider.h"

#include "cc/layers/layer_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "platform/graphics/CompositorElementId.h"
#include "platform/graphics/CompositorMutableProperties.h"
#include "platform/graphics/CompositorMutableState.h"
#include "platform/graphics/CompositorMutation.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

CompositorMutableStateProvider::CompositorMutableStateProvider(cc::LayerTreeImpl* treeImpl, CompositorMutations* mutations)
    : m_tree(treeImpl)
    , m_mutations(mutations)
{
}

CompositorMutableStateProvider::~CompositorMutableStateProvider() {}

std::unique_ptr<CompositorMutableState>
CompositorMutableStateProvider::getMutableStateFor(uint64_t elementId)
{
    cc::LayerImpl* mainLayer = m_tree->LayerByElementId(createCompositorElementId(elementId, CompositorSubElementId::Primary));
    cc::LayerImpl* scrollLayer = m_tree->LayerByElementId(createCompositorElementId(elementId, CompositorSubElementId::Scroll));

    if (!mainLayer && !scrollLayer)
        return nullptr;

    // Ensure that we have an entry in the map for |elementId| but do as few
    // allocations and queries as possible. This will update the map only if we
    // have not added a value for |elementId|.
    auto result = m_mutations->map.add(elementId, nullptr);

    // Only if this is a new entry do we want to allocate a new mutation.
    if (result.isNewEntry)
        result.storedValue->value = wrapUnique(new CompositorMutation);

    return wrapUnique(new CompositorMutableState(result.storedValue->value.get(), mainLayer, scrollLayer));
}

} // namespace blink
