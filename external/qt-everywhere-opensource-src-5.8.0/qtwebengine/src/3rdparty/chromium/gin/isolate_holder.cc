// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/public/isolate_holder.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/sys_info.h"
#include "gin/debug_impl.h"
#include "gin/function_template.h"
#include "gin/per_isolate_data.h"
#include "gin/run_microtasks_observer.h"
#include "gin/v8_initializer.h"
#include "gin/v8_isolate_memory_dump_provider.h"

namespace gin {

namespace {
v8::ArrayBuffer::Allocator* g_array_buffer_allocator = nullptr;
}  // namespace

IsolateHolder::IsolateHolder() : IsolateHolder(AccessMode::kSingleThread) {
}

IsolateHolder::IsolateHolder(AccessMode access_mode)
    : access_mode_(access_mode) {
  v8::ArrayBuffer::Allocator* allocator = g_array_buffer_allocator;
  CHECK(allocator) << "You need to invoke gin::IsolateHolder::Initialize first";
  v8::Isolate::CreateParams params;
  params.entry_hook = DebugImpl::GetFunctionEntryHook();
  params.code_event_handler = DebugImpl::GetJitCodeEventHandler();
  params.constraints.ConfigureDefaults(base::SysInfo::AmountOfPhysicalMemory(),
                                       base::SysInfo::AmountOfVirtualMemory());
  params.array_buffer_allocator = allocator;
  isolate_ = v8::Isolate::New(params);
  isolate_data_.reset(new PerIsolateData(isolate_, allocator));
  isolate_memory_dump_provider_.reset(new V8IsolateMemoryDumpProvider(this));
#if defined(OS_WIN)
  {
    void* code_range;
    size_t size;
    isolate_->GetCodeRange(&code_range, &size);
    Debug::CodeRangeCreatedCallback callback =
        DebugImpl::GetCodeRangeCreatedCallback();
    if (code_range && size && callback)
      callback(code_range, size);
  }
#endif
}

IsolateHolder::~IsolateHolder() {
  if (task_observer_.get())
    base::MessageLoop::current()->RemoveTaskObserver(task_observer_.get());
#if defined(OS_WIN)
  {
    void* code_range;
    size_t size;
    isolate_->GetCodeRange(&code_range, &size);
    Debug::CodeRangeDeletedCallback callback =
        DebugImpl::GetCodeRangeDeletedCallback();
    if (code_range && callback)
      callback(code_range);
  }
#endif
  isolate_memory_dump_provider_.reset();
  isolate_data_.reset();
  isolate_->Dispose();
  isolate_ = NULL;
}

// static
void IsolateHolder::Initialize(ScriptMode mode,
                               V8ExtrasMode v8_extras_mode,
                               v8::ArrayBuffer::Allocator* allocator) {
  CHECK(allocator);
  V8Initializer::Initialize(mode, v8_extras_mode);
  g_array_buffer_allocator = allocator;
}

void IsolateHolder::AddRunMicrotasksObserver() {
  DCHECK(!task_observer_.get());
  task_observer_.reset(new RunMicrotasksObserver(isolate_));
  base::MessageLoop::current()->AddTaskObserver(task_observer_.get());
}

void IsolateHolder::RemoveRunMicrotasksObserver() {
  DCHECK(task_observer_.get());
  base::MessageLoop::current()->RemoveTaskObserver(task_observer_.get());
  task_observer_.reset();
}

void IsolateHolder::EnableIdleTasks(
    std::unique_ptr<V8IdleTaskRunner> idle_task_runner) {
  DCHECK(isolate_data_.get());
  isolate_data_->EnableIdleTasks(std::move(idle_task_runner));
}

}  // namespace gin
