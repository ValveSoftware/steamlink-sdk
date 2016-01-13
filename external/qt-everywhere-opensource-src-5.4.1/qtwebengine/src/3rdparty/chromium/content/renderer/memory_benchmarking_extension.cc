// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/memory_benchmarking_extension.h"

#include "content/common/memory_benchmark_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebKit.h"

#if defined(USE_TCMALLOC) && (defined(OS_LINUX) || defined(OS_ANDROID))
#include "third_party/tcmalloc/chromium/src/gperftools/heap-profiler.h"
#endif

namespace content {

gin::WrapperInfo MemoryBenchmarkingExtension::kWrapperInfo = {
    gin::kEmbedderNativeGin};

// static
void MemoryBenchmarkingExtension::Install(blink::WebFrame* frame) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Handle<v8::Context> context = frame->mainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);
  gin::Handle<MemoryBenchmarkingExtension> controller =
      gin::CreateHandle(isolate, new MemoryBenchmarkingExtension());
  if (controller.IsEmpty())
    return;

  v8::Handle<v8::Object> global = context->Global();
  v8::Handle<v8::Object> chrome =
      global->Get(gin::StringToV8(isolate, "chrome"))->ToObject();
  if (chrome.IsEmpty()) {
    chrome = v8::Object::New(isolate);
    global->Set(gin::StringToV8(isolate, "chrome"), chrome);
  }
  chrome->Set(gin::StringToV8(isolate, "memoryBenchmarking"),
              controller.ToV8());
}

MemoryBenchmarkingExtension::MemoryBenchmarkingExtension() {}

MemoryBenchmarkingExtension::~MemoryBenchmarkingExtension() {}

gin::ObjectTemplateBuilder
MemoryBenchmarkingExtension::GetObjectTemplateBuilder(v8::Isolate* isolate) {
  return gin::Wrappable<MemoryBenchmarkingExtension>::GetObjectTemplateBuilder(
             isolate)
      .SetMethod("isHeapProfilerRunning",
                 &MemoryBenchmarkingExtension::IsHeapProfilerRunning)
      .SetMethod("heapProfilerDump",
                 &MemoryBenchmarkingExtension::HeapProfilerDump);
}

bool MemoryBenchmarkingExtension::IsHeapProfilerRunning() {
#if defined(USE_TCMALLOC) && (defined(OS_LINUX) || defined(OS_ANDROID))
  return ::IsHeapProfilerRunning();
#else
  return false;
#endif
}

void MemoryBenchmarkingExtension::HeapProfilerDump(gin::Arguments* args) {
  std::string process_type;
  std::string reason("benchmarking_extension");

  if (args->PeekNext()->IsString()) {
    args->GetNext(&process_type);
    if (args->PeekNext()->IsString())
      args->GetNext(&reason);
  }

#if defined(USE_TCMALLOC) && (defined(OS_LINUX) || defined(OS_ANDROID))
  if (process_type == "browser") {
    content::RenderThreadImpl::current()->Send(
        new MemoryBenchmarkHostMsg_HeapProfilerDump(reason));
  } else {
    ::HeapProfilerDump(reason.c_str());
  }
#endif
}

}  // namespace content
