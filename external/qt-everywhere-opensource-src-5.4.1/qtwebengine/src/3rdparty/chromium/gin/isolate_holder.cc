// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/public/isolate_holder.h"

#include <stdlib.h>
#include <string.h>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/sys_info.h"
#include "gin/array_buffer.h"
#include "gin/function_template.h"
#include "gin/per_isolate_data.h"
#include "gin/public/v8_platform.h"

namespace gin {

namespace {

bool GenerateEntropy(unsigned char* buffer, size_t amount) {
  base::RandBytes(buffer, amount);
  return true;
}

void EnsureV8Initialized(gin::IsolateHolder::ScriptMode mode,
                         bool gin_managed) {
  static bool v8_is_initialized = false;
  static bool v8_is_gin_managed = false;
  if (v8_is_initialized) {
    CHECK_EQ(v8_is_gin_managed, gin_managed);
    return;
  }
  v8_is_initialized = true;
  v8_is_gin_managed = gin_managed;
  if (!gin_managed)
    return;

  v8::V8::InitializePlatform(V8Platform::Get());
  v8::V8::SetArrayBufferAllocator(ArrayBufferAllocator::SharedInstance());
  if (mode == gin::IsolateHolder::kStrictMode) {
    static const char v8_flags[] = "--use_strict";
    v8::V8::SetFlagsFromString(v8_flags, sizeof(v8_flags) - 1);
  }
  v8::V8::SetEntropySource(&GenerateEntropy);
  v8::V8::Initialize();
}

}  // namespace

IsolateHolder::IsolateHolder(ScriptMode mode)
  : isolate_owner_(true) {
  EnsureV8Initialized(mode, true);
  isolate_ = v8::Isolate::New();
  v8::ResourceConstraints constraints;
  constraints.ConfigureDefaults(base::SysInfo::AmountOfPhysicalMemory(),
                                base::SysInfo::AmountOfVirtualMemory(),
                                base::SysInfo::NumberOfProcessors());
  v8::SetResourceConstraints(isolate_, &constraints);
  Init(ArrayBufferAllocator::SharedInstance());
}

IsolateHolder::IsolateHolder(v8::Isolate* isolate,
                             v8::ArrayBuffer::Allocator* allocator)
    : isolate_owner_(false), isolate_(isolate) {
  EnsureV8Initialized(kNonStrictMode, false);
  Init(allocator);
}

IsolateHolder::~IsolateHolder() {
  isolate_data_.reset();
  if (isolate_owner_)
    isolate_->Dispose();
}

void IsolateHolder::Init(v8::ArrayBuffer::Allocator* allocator) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  isolate_data_.reset(new PerIsolateData(isolate_, allocator));
}

}  // namespace gin
