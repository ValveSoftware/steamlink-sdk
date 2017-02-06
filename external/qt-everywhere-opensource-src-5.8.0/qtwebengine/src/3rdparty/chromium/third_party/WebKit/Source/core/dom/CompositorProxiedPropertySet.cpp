// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/CompositorProxiedPropertySet.h"

#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

std::unique_ptr<CompositorProxiedPropertySet> CompositorProxiedPropertySet::create()
{
    return wrapUnique(new CompositorProxiedPropertySet);
}

CompositorProxiedPropertySet::CompositorProxiedPropertySet()
{
    memset(m_counts, 0, sizeof(m_counts));
}

CompositorProxiedPropertySet::~CompositorProxiedPropertySet() {}

bool CompositorProxiedPropertySet::isEmpty() const
{
    return !proxiedProperties();
}

void CompositorProxiedPropertySet::increment(uint32_t mutableProperties)
{
    for (int i = 0; i < CompositorMutableProperty::kNumProperties; ++i) {
        if (mutableProperties & (1 << i))
            ++m_counts[i];
    }
}

void CompositorProxiedPropertySet::decrement(uint32_t mutableProperties)
{
    for (int i = 0; i < CompositorMutableProperty::kNumProperties; ++i) {
        if (mutableProperties & (1 << i)) {
            DCHECK(m_counts[i]);
            --m_counts[i];
        }
    }
}

uint32_t CompositorProxiedPropertySet::proxiedProperties() const
{
    uint32_t properties = CompositorMutableProperty::kNone;
    for (int i = 0; i < CompositorMutableProperty::kNumProperties; ++i) {
        if (m_counts[i])
            properties |= 1 << i;
    }
    return properties;
}

} // namespace blink
