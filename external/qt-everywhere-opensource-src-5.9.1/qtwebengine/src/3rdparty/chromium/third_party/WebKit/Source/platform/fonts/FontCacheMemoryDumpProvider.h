// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FontCacheMemoryDumpProvider_h
#define FontCacheMemoryDumpProvider_h

#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/process_memory_dump.h"
#include "platform/PlatformExport.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"

namespace blink {

class PLATFORM_EXPORT FontCacheMemoryDumpProvider final
    : public base::trace_event::MemoryDumpProvider {
  USING_FAST_MALLOC(FontCacheMemoryDumpProvider);

 public:
  static FontCacheMemoryDumpProvider* instance();
  ~FontCacheMemoryDumpProvider() override {}

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs&,
                    base::trace_event::ProcessMemoryDump*) override;

 private:
  FontCacheMemoryDumpProvider() {}

  WTF_MAKE_NONCOPYABLE(FontCacheMemoryDumpProvider);
};

}  // namespace blink

#endif  // FontCacheMemoryDumpProvider_h
