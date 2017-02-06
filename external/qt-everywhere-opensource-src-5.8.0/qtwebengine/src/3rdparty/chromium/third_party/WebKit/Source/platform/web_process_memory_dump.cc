// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/web_process_memory_dump.h"

#include "base/memory/discardable_memory.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/heap_profiler_heap_dump_writer.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event_argument.h"
#include "base/trace_event/trace_event_memory_overhead.h"
#include "platform/web_memory_allocator_dump.h"
#include "skia/ext/skia_trace_memory_dump_impl.h"
#include "wtf/text/StringUTF8Adaptor.h"

#include <stddef.h>

namespace blink {

WebProcessMemoryDump::WebProcessMemoryDump()
    : owned_process_memory_dump_(
          new base::trace_event::ProcessMemoryDump(nullptr, { base::trace_event::MemoryDumpLevelOfDetail::DETAILED }))
    , process_memory_dump_(owned_process_memory_dump_.get())
    , level_of_detail_(base::trace_event::MemoryDumpLevelOfDetail::DETAILED) {
}

WebProcessMemoryDump::WebProcessMemoryDump(
    base::trace_event::MemoryDumpLevelOfDetail level_of_detail,
    base::trace_event::ProcessMemoryDump* process_memory_dump)
    : process_memory_dump_(process_memory_dump),
      level_of_detail_(level_of_detail) {}

WebProcessMemoryDump::~WebProcessMemoryDump() {
}

blink::WebMemoryAllocatorDump*
WebProcessMemoryDump::createMemoryAllocatorDump(
    const String& absolute_name) {
  StringUTF8Adaptor adapter(absolute_name);
  std::string name(adapter.data(), adapter.length());
  // Get a MemoryAllocatorDump from the base/ object.
  base::trace_event::MemoryAllocatorDump* memory_allocator_dump =
      process_memory_dump_->CreateAllocatorDump(name);

  return createWebMemoryAllocatorDump(memory_allocator_dump);
}

blink::WebMemoryAllocatorDump*
WebProcessMemoryDump::createMemoryAllocatorDump(
    const String& absolute_name,
    blink::WebMemoryAllocatorDumpGuid guid) {
  StringUTF8Adaptor adapter(absolute_name);
  std::string name(adapter.data(), adapter.length());
  // Get a MemoryAllocatorDump from the base/ object with given guid.
  base::trace_event::MemoryAllocatorDump* memory_allocator_dump =
      process_memory_dump_->CreateAllocatorDump(
          name,
          base::trace_event::MemoryAllocatorDumpGuid(guid));
  return createWebMemoryAllocatorDump(memory_allocator_dump);
}

blink::WebMemoryAllocatorDump*
WebProcessMemoryDump::createWebMemoryAllocatorDump(
    base::trace_event::MemoryAllocatorDump* memory_allocator_dump) {
  if (!memory_allocator_dump)
    return nullptr;

  // Wrap it and return to blink.
  WebMemoryAllocatorDump* web_memory_allocator_dump =
      new WebMemoryAllocatorDump(memory_allocator_dump);

  // memory_allocator_dumps_ will take ownership of
  // |web_memory_allocator_dump|.
  memory_allocator_dumps_.set(
      memory_allocator_dump, wrapUnique(web_memory_allocator_dump));
  return web_memory_allocator_dump;
}

blink::WebMemoryAllocatorDump* WebProcessMemoryDump::getMemoryAllocatorDump(
    const String& absolute_name) const {
  StringUTF8Adaptor adapter(absolute_name);
  std::string name(adapter.data(), adapter.length());
  // Retrieve the base MemoryAllocatorDump object and then reverse lookup
  // its wrapper.
  base::trace_event::MemoryAllocatorDump* memory_allocator_dump =
      process_memory_dump_->GetAllocatorDump(name);
  if (!memory_allocator_dump)
    return nullptr;

  // The only case of (memory_allocator_dump && !web_memory_allocator_dump)
  // is something from blink trying to get a MAD that was created from chromium,
  // which is an odd use case.
  blink::WebMemoryAllocatorDump* web_memory_allocator_dump =
      memory_allocator_dumps_.get(memory_allocator_dump);
  DCHECK(web_memory_allocator_dump);
  return web_memory_allocator_dump;
}

void WebProcessMemoryDump::clear() {
  // Clear all the WebMemoryAllocatorDump wrappers.
  memory_allocator_dumps_.clear();

  // Clear the actual MemoryAllocatorDump objects from the underlying PMD.
  process_memory_dump_->Clear();
}

void WebProcessMemoryDump::takeAllDumpsFrom(
    blink::WebProcessMemoryDump* other) {
  // WebProcessMemoryDump is a container of WebMemoryAllocatorDump(s) which
  // in turn are wrappers of base::trace_event::MemoryAllocatorDump(s).
  // In order to expose the move and ownership transfer semantics of the
  // underlying ProcessMemoryDump, we need to:

  // 1) Move and transfer the ownership of the wrapped
  // base::trace_event::MemoryAllocatorDump(s) instances.
  process_memory_dump_->TakeAllDumpsFrom(other->process_memory_dump_);

  // 2) Move and transfer the ownership of the WebMemoryAllocatorDump wrappers.
  const size_t expected_final_size = memory_allocator_dumps_.size() +
                                     other->memory_allocator_dumps_.size();
  while (!other->memory_allocator_dumps_.isEmpty()) {
    auto first_entry = other->memory_allocator_dumps_.begin();
    base::trace_event::MemoryAllocatorDump* memory_allocator_dump =
        first_entry->key;
    memory_allocator_dumps_.set(memory_allocator_dump,
        other->memory_allocator_dumps_.take(memory_allocator_dump));
  }
  DCHECK_EQ(expected_final_size, memory_allocator_dumps_.size());
  DCHECK(other->memory_allocator_dumps_.isEmpty());
}

void WebProcessMemoryDump::addOwnershipEdge(
    blink::WebMemoryAllocatorDumpGuid source,
    blink::WebMemoryAllocatorDumpGuid target,
    int importance) {
  process_memory_dump_->AddOwnershipEdge(
      base::trace_event::MemoryAllocatorDumpGuid(source),
      base::trace_event::MemoryAllocatorDumpGuid(target), importance);
}

void WebProcessMemoryDump::addOwnershipEdge(
    blink::WebMemoryAllocatorDumpGuid source,
    blink::WebMemoryAllocatorDumpGuid target) {
  process_memory_dump_->AddOwnershipEdge(
      base::trace_event::MemoryAllocatorDumpGuid(source),
      base::trace_event::MemoryAllocatorDumpGuid(target));
}

void WebProcessMemoryDump::addSuballocation(
    blink::WebMemoryAllocatorDumpGuid source,
    const String& target_node_name) {
  StringUTF8Adaptor adapter(target_node_name);
  std::string target_node(adapter.data(), adapter.length());
  process_memory_dump_->AddSuballocation(
      base::trace_event::MemoryAllocatorDumpGuid(source),
      target_node);
}

SkTraceMemoryDump* WebProcessMemoryDump::createDumpAdapterForSkia(
    const String& dump_name_prefix) {
  StringUTF8Adaptor adapter(dump_name_prefix);
  std::string prefix(adapter.data(), adapter.length());
  sk_trace_dump_list_.push_back(base::WrapUnique(
      new skia::SkiaTraceMemoryDumpImpl(
          prefix, level_of_detail_, process_memory_dump_)));
  return sk_trace_dump_list_.back().get();
}

blink::WebMemoryAllocatorDump*
WebProcessMemoryDump::createDiscardableMemoryAllocatorDump(
    const std::string& name,
    base::DiscardableMemory* discardable) {
  base::trace_event::MemoryAllocatorDump* dump =
      discardable->CreateMemoryAllocatorDump(name.c_str(),
                                             process_memory_dump_);
  return createWebMemoryAllocatorDump(dump);
}

void WebProcessMemoryDump::dumpHeapUsage(
    const base::hash_map<base::trace_event::AllocationContext,
        base::trace_event::AllocationMetrics>& metrics_by_context,
    base::trace_event::TraceEventMemoryOverhead& overhead,
    const char* allocator_name) {
  process_memory_dump_->DumpHeapUsage(metrics_by_context, overhead, allocator_name);
}

}  // namespace content
