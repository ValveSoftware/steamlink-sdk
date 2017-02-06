// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/FontCacheMemoryDumpProvider.h"

#include "platform/fonts/FontCache.h"
#include "wtf/Assertions.h"

namespace blink {

FontCacheMemoryDumpProvider* FontCacheMemoryDumpProvider::instance()
{
    DEFINE_STATIC_LOCAL(FontCacheMemoryDumpProvider, instance, ());
    return &instance;
}

bool FontCacheMemoryDumpProvider::OnMemoryDump(const base::trace_event::MemoryDumpArgs&, base::trace_event::ProcessMemoryDump* memoryDump)
{
    ASSERT(isMainThread());
    FontCache::fontCache()->dumpFontPlatformDataCache(memoryDump);
    FontCache::fontCache()->dumpShapeResultCache(memoryDump);
    return true;
}

} // namespace blink
