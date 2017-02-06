// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MemoryCacheDumpProvider_h
#define MemoryCacheDumpProvider_h

#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/process_memory_dump.h"
#include "platform/PlatformExport.h"
#include "platform/heap/Handle.h"
#include "platform/web_process_memory_dump.h"
#include "wtf/Allocator.h"

namespace blink {

class PLATFORM_EXPORT MemoryCacheDumpClient : public GarbageCollectedMixin {
public:
    virtual ~MemoryCacheDumpClient() { }
    virtual bool onMemoryDump(WebMemoryDumpLevelOfDetail, WebProcessMemoryDump*) = 0;

    DECLARE_VIRTUAL_TRACE();
};

// This class is wrapper around MemoryCache to take memory snapshots. It dumps
// the stats of cache only after the cache is created.
class PLATFORM_EXPORT MemoryCacheDumpProvider final : public base::trace_event::MemoryDumpProvider {
    USING_FAST_MALLOC(MemoryCacheDumpProvider);
public:
    // This class is singleton since there is a global MemoryCache object.
    static MemoryCacheDumpProvider* instance();
    ~MemoryCacheDumpProvider() override;

    // base::trace_event::MemoryDumpProvider implementation.
    bool OnMemoryDump(const base::trace_event::MemoryDumpArgs&, base::trace_event::ProcessMemoryDump*) override;

    void setMemoryCache(MemoryCacheDumpClient* client)
    {
        DCHECK(isMainThread());
        m_client = client;
    }

private:
    MemoryCacheDumpProvider();

    WeakPersistent<MemoryCacheDumpClient> m_client;

    WTF_MAKE_NONCOPYABLE(MemoryCacheDumpProvider);
};

} // namespace blink

#endif // MemoryCacheDumpProvider_h
