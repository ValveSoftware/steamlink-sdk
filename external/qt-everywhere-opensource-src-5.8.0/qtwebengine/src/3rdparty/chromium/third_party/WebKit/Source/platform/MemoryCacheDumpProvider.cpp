// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/MemoryCacheDumpProvider.h"

namespace blink {

DEFINE_TRACE(MemoryCacheDumpClient)
{
}

MemoryCacheDumpProvider* MemoryCacheDumpProvider::instance()
{
    DEFINE_STATIC_LOCAL(MemoryCacheDumpProvider, instance, ());
    return &instance;
}

bool MemoryCacheDumpProvider::OnMemoryDump(const base::trace_event::MemoryDumpArgs& args, base::trace_event::ProcessMemoryDump* memoryDump)
{
    DCHECK(isMainThread());
    if (!m_client)
        return false;

    WebMemoryDumpLevelOfDetail level;
    switch (args.level_of_detail) {
    case base::trace_event::MemoryDumpLevelOfDetail::LIGHT:
        level = blink::WebMemoryDumpLevelOfDetail::Light;
        break;
    case base::trace_event::MemoryDumpLevelOfDetail::DETAILED:
        level = blink::WebMemoryDumpLevelOfDetail::Detailed;
        break;
    default:
        NOTREACHED();
        return false;
    }

    WebProcessMemoryDump dump(args.level_of_detail, memoryDump);
    return m_client->onMemoryDump(level, &dump);
}

MemoryCacheDumpProvider::MemoryCacheDumpProvider()
{
}

MemoryCacheDumpProvider::~MemoryCacheDumpProvider()
{
}

} // namespace blink
