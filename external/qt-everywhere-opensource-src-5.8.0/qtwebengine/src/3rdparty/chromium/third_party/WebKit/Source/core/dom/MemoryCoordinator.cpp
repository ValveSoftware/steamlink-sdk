// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/MemoryCoordinator.h"

#include "core/fetch/MemoryCache.h"
#include "platform/TraceEvent.h"
#include "platform/fonts/FontCache.h"
#include "platform/graphics/ImageDecodingStore.h"
#include "platform/heap/Heap.h"
#include "wtf/allocator/Partitions.h"

namespace blink {

MemoryCoordinator& MemoryCoordinator::instance()
{
    DEFINE_STATIC_LOCAL(MemoryCoordinator, instance, ());
    return instance;
}

MemoryCoordinator::MemoryCoordinator()
{
}

MemoryCoordinator::~MemoryCoordinator()
{
}

void MemoryCoordinator::onMemoryPressure(WebMemoryPressureLevel level)
{
    TRACE_EVENT0("blink", "MemoryCoordinator::onMemoryPressure");
    if (level == WebMemoryPressureLevelCritical) {
        // Clear the image cache.
        ImageDecodingStore::instance().clear();
        FontCache::fontCache()->invalidate();
    }
    if (ProcessHeap::isLowEndDevice())
        memoryCache()->pruneAll();
    WTF::Partitions::decommitFreeableMemory();
}

} // namespace blink
