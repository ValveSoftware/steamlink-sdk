// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_binding_test.h"

#include "base/memory/ptr_util.h"
#include "gin/array_buffer.h"
#include "gin/public/context_holder.h"
#include "gin/public/isolate_holder.h"
#include "gin/v8_initializer.h"

namespace extensions {

APIBindingTest::APIBindingTest() {}
APIBindingTest::~APIBindingTest() {}

void APIBindingTest::SetUp() {
  // Much of this initialization is stolen from the somewhat-similar
  // gin::V8Test.
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  gin::V8Initializer::LoadV8Snapshot();
  gin::V8Initializer::LoadV8Natives();
#endif

  gin::IsolateHolder::Initialize(gin::IsolateHolder::kStrictMode,
                                 gin::IsolateHolder::kStableV8Extras,
                                 gin::ArrayBufferAllocator::SharedInstance());

  isolate_holder_ = base::MakeUnique<gin::IsolateHolder>();
  isolate()->Enter();

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = v8::Context::New(isolate());
  context->Enter();
  context_holder_ = base::MakeUnique<gin::ContextHolder>(isolate());
  context_holder_->SetContext(context);
}

void APIBindingTest::TearDown() {
  // Check for leaks - a weak handle to a context is invalidated on context
  // destruction, so resetting the context should reset the handle.
  v8::Global<v8::Context> weak_context;
  {
    v8::HandleScope handle_scope(isolate());
    v8::Local<v8::Context> context = ContextLocal();
    weak_context.Reset(isolate(), context);
    weak_context.SetWeak();
    context->Exit();
  }
  context_holder_.reset();

  // Garbage collect everything so that we find any issues where we might be
  // double-freeing.
  // '5' is a magic number stolen from Blink; arbitrarily large enough to
  // hopefully clean up all the various paths.
  for (int i = 0; i < 5; ++i) {
    isolate()->RequestGarbageCollectionForTesting(
        v8::Isolate::kFullGarbageCollection);
  }

  // The context should have been deleted.
  ASSERT_TRUE(weak_context.IsEmpty());

  isolate()->Exit();
  isolate_holder_.reset();
}

v8::Local<v8::Context> APIBindingTest::ContextLocal() {
  return context_holder_->context();
}

v8::Isolate* APIBindingTest::isolate() {
  return isolate_holder_->isolate();
}

}  // namespace extensions
