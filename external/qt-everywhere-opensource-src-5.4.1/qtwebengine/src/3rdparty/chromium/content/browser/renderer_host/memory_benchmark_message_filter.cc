// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/memory_benchmark_message_filter.h"

#include "content/common/memory_benchmark_messages.h"

#if defined(USE_TCMALLOC) && (defined(OS_LINUX) || defined(OS_ANDROID))

#include "third_party/tcmalloc/chromium/src/gperftools/heap-profiler.h"

namespace content {

MemoryBenchmarkMessageFilter::MemoryBenchmarkMessageFilter()
    : BrowserMessageFilter(MemoryBenchmarkMsgStart) {
}

bool MemoryBenchmarkMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MemoryBenchmarkMessageFilter, message)
    IPC_MESSAGE_HANDLER(MemoryBenchmarkHostMsg_HeapProfilerDump,
                        OnHeapProfilerDump)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

MemoryBenchmarkMessageFilter::~MemoryBenchmarkMessageFilter() {
}

void MemoryBenchmarkMessageFilter::OnHeapProfilerDump(
    const std::string& reason) {
  ::HeapProfilerDump(reason.c_str());
}

}  // namespace content

#endif  // defined(USE_TCMALLOC) && (defined(OS_LINUX) || defined(OS_ANDROID))
