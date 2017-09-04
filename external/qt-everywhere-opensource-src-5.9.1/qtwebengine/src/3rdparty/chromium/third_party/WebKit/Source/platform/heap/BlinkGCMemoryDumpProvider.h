// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlinkGCMemoryDumpProvider_h
#define BlinkGCMemoryDumpProvider_h

#include "base/trace_event/memory_dump_provider.h"
#include "platform/PlatformExport.h"
#include "platform/heap/BlinkGC.h"
#include "wtf/Allocator.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/text/WTFString.h"

namespace base {
namespace trace_event {

class AllocationRegister;
class MemoryAllocatorDump;

}  // namespace trace_event
}  // namespace base

namespace blink {
class WebMemoryAllocatorDump;

class PLATFORM_EXPORT BlinkGCMemoryDumpProvider final
    : public base::trace_event::MemoryDumpProvider {
  USING_FAST_MALLOC(BlinkGCMemoryDumpProvider);

 public:
  static BlinkGCMemoryDumpProvider* instance();
  ~BlinkGCMemoryDumpProvider() override;

  // MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs&,
                    base::trace_event::ProcessMemoryDump*) override;
  void OnHeapProfilingEnabled(bool) override;

  // The returned WebMemoryAllocatorDump is owned by
  // BlinkGCMemoryDumpProvider, and should not be retained (just used to
  // dump in the current call stack).
  base::trace_event::MemoryAllocatorDump* createMemoryAllocatorDumpForCurrentGC(
      const String& absoluteName);

  // This must be called before taking a new process-wide GC snapshot, to
  // clear the previous dumps.
  void clearProcessDumpForCurrentGC();

  base::trace_event::ProcessMemoryDump* currentProcessMemoryDump() {
    return m_currentProcessMemoryDump.get();
  }

  void insert(Address, size_t, const char*);
  void remove(Address);

 private:
  BlinkGCMemoryDumpProvider();

  Mutex m_allocationRegisterMutex;
  std::unique_ptr<base::trace_event::AllocationRegister> m_allocationRegister;
  std::unique_ptr<base::trace_event::ProcessMemoryDump>
      m_currentProcessMemoryDump;
  bool m_isHeapProfilingEnabled;
};

}  // namespace blink

#endif
