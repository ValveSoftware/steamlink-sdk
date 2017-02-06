// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/web_process_memory_dump.h"

#include "base/memory/discardable_memory.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event_argument.h"
#include "base/values.h"
#include "platform/web_memory_allocator_dump.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

// Tests that the Chromium<>Blink plumbing that exposes the MemoryInfra classes
// behaves correctly, performs the right transfers of memory ownerships and
// doesn't leak objects.
TEST(WebProcessMemoryDumpTest, IntegrationTest) {
  std::unique_ptr<base::trace_event::TracedValue> traced_value(
          new base::trace_event::TracedValue());

  std::unique_ptr<WebProcessMemoryDump> wpmd1(new WebProcessMemoryDump());
  auto wmad1 = wpmd1->createMemoryAllocatorDump("1/1");
  auto wmad2 = wpmd1->createMemoryAllocatorDump("1/2");
  ASSERT_EQ(wmad1, wpmd1->getMemoryAllocatorDump("1/1"));
  ASSERT_EQ(wmad2, wpmd1->getMemoryAllocatorDump("1/2"));

  std::unique_ptr<WebProcessMemoryDump> wpmd2(new WebProcessMemoryDump());
  wpmd2->createMemoryAllocatorDump("2/1");
  wpmd2->createMemoryAllocatorDump("2/2");

  wpmd1->takeAllDumpsFrom(wpmd2.get());

  // Make sure that wpmd2 still owns its own PMD, even if empty.
  ASSERT_NE(static_cast<base::trace_event::ProcessMemoryDump*>(nullptr),
            wpmd2->process_memory_dump_);
  ASSERT_EQ(wpmd2->owned_process_memory_dump_.get(),
            wpmd2->process_memory_dump());
  ASSERT_TRUE(wpmd2->process_memory_dump()->allocator_dumps().empty());

  // Make sure that wpmd2 is still usable after it has been emptied.
  auto wmad = wpmd2->createMemoryAllocatorDump("2/new");
  wmad->addScalar("attr_name", "bytes", 42);
  wmad->addScalarF("attr_name_2", "rate", 42.0f);
  ASSERT_EQ(1u, wpmd2->process_memory_dump()->allocator_dumps().size());
  auto mad = wpmd2->process_memory_dump()->GetAllocatorDump("2/new");
  ASSERT_NE(static_cast<base::trace_event::MemoryAllocatorDump*>(nullptr), mad);
  ASSERT_EQ(wmad, wpmd2->getMemoryAllocatorDump("2/new"));

  // Check that the attributes are propagated correctly.
  auto raw_attrs = mad->attributes_for_testing()->ToBaseValue();
  base::DictionaryValue* attrs = nullptr;
  ASSERT_TRUE(raw_attrs->GetAsDictionary(&attrs));
  base::DictionaryValue* attr = nullptr;
  ASSERT_TRUE(attrs->GetDictionary("attr_name", &attr));
  std::string attr_value;
  ASSERT_TRUE(attr->GetString("type", &attr_value));
  ASSERT_EQ(base::trace_event::MemoryAllocatorDump::kTypeScalar, attr_value);
  ASSERT_TRUE(attr->GetString("units", &attr_value));
  ASSERT_EQ("bytes", attr_value);

  ASSERT_TRUE(attrs->GetDictionary("attr_name_2", &attr));
  ASSERT_TRUE(attr->GetString("type", &attr_value));
  ASSERT_EQ(base::trace_event::MemoryAllocatorDump::kTypeScalar, attr_value);
  ASSERT_TRUE(attr->GetString("units", &attr_value));
  ASSERT_EQ("rate", attr_value);
  ASSERT_TRUE(attr->HasKey("value"));

  // Check that AsValueInto() doesn't cause a crash.
  wpmd2->process_memory_dump()->AsValueInto(traced_value.get());

  // Free the |wpmd2| to check that the memory ownership of the two MAD(s)
  // has been transferred to |wpmd1|.
  wpmd2.reset();

  // Now check that |wpmd1| has been effectively merged.
  ASSERT_EQ(4u, wpmd1->process_memory_dump()->allocator_dumps().size());
  ASSERT_EQ(1u, wpmd1->process_memory_dump()->allocator_dumps().count("1/1"));
  ASSERT_EQ(1u, wpmd1->process_memory_dump()->allocator_dumps().count("1/2"));
  ASSERT_EQ(1u, wpmd1->process_memory_dump()->allocator_dumps().count("2/1"));
  ASSERT_EQ(1u, wpmd1->process_memory_dump()->allocator_dumps().count("1/2"));

  // Check that also the WMAD wrappers got merged.
  blink::WebMemoryAllocatorDump* null_wmad = nullptr;
  ASSERT_NE(null_wmad, wpmd1->getMemoryAllocatorDump("1/1"));
  ASSERT_NE(null_wmad, wpmd1->getMemoryAllocatorDump("1/2"));
  ASSERT_NE(null_wmad, wpmd1->getMemoryAllocatorDump("2/1"));
  ASSERT_NE(null_wmad, wpmd1->getMemoryAllocatorDump("2/2"));

  // Check that AsValueInto() doesn't cause a crash.
  traced_value.reset(new base::trace_event::TracedValue);
  wpmd1->process_memory_dump()->AsValueInto(traced_value.get());

  // Check that clear() actually works.
  wpmd1->clear();
  ASSERT_TRUE(wpmd1->process_memory_dump()->allocator_dumps().empty());
  ASSERT_EQ(nullptr, wpmd1->process_memory_dump()->GetAllocatorDump("1/1"));
  ASSERT_EQ(nullptr, wpmd1->process_memory_dump()->GetAllocatorDump("2/1"));

  // Check that AsValueInto() doesn't cause a crash.
  traced_value.reset(new base::trace_event::TracedValue);
  wpmd1->process_memory_dump()->AsValueInto(traced_value.get());

  // Check if a WebMemoryAllocatorDump created with guid, has correct guid.
  blink::WebMemoryAllocatorDumpGuid guid =
      base::trace_event::MemoryAllocatorDumpGuid("id_1").ToUint64();
  auto wmad3 = wpmd1->createMemoryAllocatorDump("1/3", guid);
  ASSERT_EQ(wmad3->guid(), guid);
  ASSERT_EQ(wmad3, wpmd1->getMemoryAllocatorDump("1/3"));

  // Check that AddOwnershipEdge is propagated correctly.
  auto wmad4 = wpmd1->createMemoryAllocatorDump("1/4");
  wpmd1->addOwnershipEdge(wmad4->guid(), guid);
  auto allocator_dumps_edges =
      wpmd1->process_memory_dump()->allocator_dumps_edges();
  ASSERT_EQ(1u, allocator_dumps_edges.size());
  ASSERT_EQ(wmad4->guid(), allocator_dumps_edges[0].source.ToUint64());
  ASSERT_EQ(guid, allocator_dumps_edges[0].target.ToUint64());

  // Check that createDumpAdapterForSkia() works.
  auto skia_trace_memory_dump = wpmd1->createDumpAdapterForSkia("1/skia");
  ASSERT_TRUE(skia_trace_memory_dump);

  // Check that createDiscardableMemoryAllocatorDump() works.
  base::TestDiscardableMemoryAllocator discardable_memory_allocator;
  auto discardable_memory =
      discardable_memory_allocator.AllocateLockedDiscardableMemory(1024);
  wpmd1->createDiscardableMemoryAllocatorDump("1/discardable",
                                              discardable_memory.get());
  discardable_memory->Unlock();

  wpmd1.reset();
}

}  // namespace blink
