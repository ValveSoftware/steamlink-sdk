// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorProxiedPropertySet_h
#define CompositorProxiedPropertySet_h

#include "platform/graphics/CompositorMutableProperties.h"
#include "wtf/Allocator.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"
#include <memory>

namespace blink {

// Keeps track of the number of proxies bound to each property.
class CompositorProxiedPropertySet final {
    WTF_MAKE_NONCOPYABLE(CompositorProxiedPropertySet);
    USING_FAST_MALLOC(CompositorProxiedPropertySet);
public:
    static std::unique_ptr<CompositorProxiedPropertySet> create();
    virtual ~CompositorProxiedPropertySet();

    bool isEmpty() const;
    void increment(uint32_t mutableProperties);
    void decrement(uint32_t mutableProperties);
    uint32_t proxiedProperties() const;

private:
    CompositorProxiedPropertySet();

    unsigned short m_counts[CompositorMutableProperty::kNumProperties];
};

} // namespace blink

#endif // CompositorProxiedPropertySet_h
