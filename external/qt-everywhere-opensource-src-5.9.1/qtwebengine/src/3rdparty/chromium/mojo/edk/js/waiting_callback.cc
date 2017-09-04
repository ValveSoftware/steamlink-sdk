// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/js/waiting_callback.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "gin/per_context_data.h"

namespace mojo {
namespace edk {
namespace js {

namespace {

v8::Handle<v8::Private> GetHiddenPropertyName(v8::Isolate* isolate) {
  return v8::Private::ForApi(
      isolate, gin::StringToV8(isolate, "::mojo::js::WaitingCallback"));
}

}  // namespace

gin::WrapperInfo WaitingCallback::kWrapperInfo = { gin::kEmbedderNativeGin };

// static
gin::Handle<WaitingCallback> WaitingCallback::Create(
    v8::Isolate* isolate,
    v8::Handle<v8::Function> callback,
    gin::Handle<HandleWrapper> handle_wrapper,
    MojoHandleSignals signals,
    bool one_shot) {
  gin::Handle<WaitingCallback> waiting_callback = gin::CreateHandle(
      isolate, new WaitingCallback(isolate, callback, one_shot));
  MojoResult result = waiting_callback->watcher_.Start(
      handle_wrapper->get(), signals,
      base::Bind(&WaitingCallback::OnHandleReady,
                 base::Unretained(waiting_callback.get())));

  // The signals may already be unsatisfiable.
  if (result == MOJO_RESULT_FAILED_PRECONDITION)
    waiting_callback->OnHandleReady(MOJO_RESULT_FAILED_PRECONDITION);

  return waiting_callback;
}

void WaitingCallback::Cancel() {
  if (watcher_.IsWatching())
    watcher_.Cancel();
}

WaitingCallback::WaitingCallback(v8::Isolate* isolate,
                                 v8::Handle<v8::Function> callback,
                                 bool one_shot)
    : one_shot_(one_shot),
      weak_factory_(this) {
  v8::Handle<v8::Context> context = isolate->GetCurrentContext();
  runner_ = gin::PerContextData::From(context)->runner()->GetWeakPtr();
  GetWrapper(isolate)
      ->SetPrivate(context, GetHiddenPropertyName(isolate), callback)
      .FromJust();
}

WaitingCallback::~WaitingCallback() {
  Cancel();
}

void WaitingCallback::OnHandleReady(MojoResult result) {
  if (!runner_)
    return;

  gin::Runner::Scope scope(runner_.get());
  v8::Isolate* isolate = runner_->GetContextHolder()->isolate();

  v8::Handle<v8::Value> hidden_value =
      GetWrapper(isolate)
          ->GetPrivate(runner_->GetContextHolder()->context(),
                       GetHiddenPropertyName(isolate))
          .ToLocalChecked();
  v8::Handle<v8::Function> callback;
  CHECK(gin::ConvertFromV8(isolate, hidden_value, &callback));

  v8::Handle<v8::Value> args[] = { gin::ConvertToV8(isolate, result) };
  runner_->Call(callback, runner_->global(), 1, args);

  if (one_shot_ || result == MOJO_RESULT_CANCELLED) {
    runner_.reset();
    Cancel();
  }
}

}  // namespace js
}  // namespace edk
}  // namespace mojo
