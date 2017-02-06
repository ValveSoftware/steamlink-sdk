// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/tracing/common/graphics_memory_dump_provider_android.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {

TEST(GraphicsMemoryDumpProviderTest, ParseResponse) {
  const char* kDumpBaseName = GraphicsMemoryDumpProvider::kDumpBaseName;

  base::trace_event::ProcessMemoryDump pmd(
      nullptr, {base::trace_event::MemoryDumpLevelOfDetail::DETAILED});
  auto instance = GraphicsMemoryDumpProvider::GetInstance();
  char buf[] = "graphics_total 12\ngraphics_pss 34\ngl_total 56\ngl_pss 78";
  instance->ParseResponseAndAddToDump(buf, strlen(buf), &pmd);

  // Check the "graphics" row.
  auto mad = pmd.GetAllocatorDump(kDumpBaseName + std::string("graphics"));
  ASSERT_TRUE(mad);
  std::string json;
  mad->attributes_for_testing()->AppendAsTraceFormat(&json);
  ASSERT_EQ(
      "{\"memtrack_pss\":{\"type\":\"scalar\",\"units\":\"bytes\",\"value\":"
      "\"22\"},"
      "\"memtrack_total\":{\"type\":\"scalar\",\"units\":\"bytes\",\"value\":"
      "\"c\"}}",
      json);

  // Check the "gl" row.
  mad = pmd.GetAllocatorDump(kDumpBaseName + std::string("gl"));
  ASSERT_TRUE(mad);
  json = "";
  mad->attributes_for_testing()->AppendAsTraceFormat(&json);
  ASSERT_EQ(
      "{\"memtrack_pss\":{\"type\":\"scalar\",\"units\":\"bytes\",\"value\":"
      "\"4e\"},"
      "\"memtrack_total\":{\"type\":\"scalar\",\"units\":\"bytes\",\"value\":"
      "\"38\"}}",
      json);

  // Test for truncated input.
  pmd.Clear();
  instance->ParseResponseAndAddToDump(buf, strlen(buf) - 14, &pmd);
  ASSERT_TRUE(pmd.GetAllocatorDump(kDumpBaseName + std::string("graphics")));
  ASSERT_FALSE(pmd.GetAllocatorDump(kDumpBaseName + std::string("gl")));
}

}  // namespace tracing
