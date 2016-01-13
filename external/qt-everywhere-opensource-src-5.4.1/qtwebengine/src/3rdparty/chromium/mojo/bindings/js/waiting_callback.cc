// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/bindings/js/waiting_callback.h"

#include "gin/per_context_data.h"
#include "mojo/public/cpp/environment/environment.h"

namespace mojo {
namespace js {

namespace {

v8::Handle<v8::String> GetHiddenPropertyName(v8::Isolate* isolate) {
  return gin::StringToSymbol(isolate, "::mojo::js::WaitingCallback");
}

}  // namespace

gin::WrapperInfo WaitingCallback::kWrapperInfo = { gin::kEmbedderNativeGin };

// static
gin::Handle<WaitingCallback> WaitingCallback::Create(
    v8::Isolate* isolate,
    v8::Handle<v8::Function> callback,
    mojo::Handle handle,
    MojoHandleSignals signals) {
  gin::Handle<WaitingCallback> waiting_callback =
      gin::CreateHandle(isolate, new WaitingCallback(isolate, callback));
  waiting_callback->wait_id_ = Environment::GetDefaultAsyncWaiter()->AsyncWait(
      handle.value(),
      signals,
      MOJO_DEADLINE_INDEFINITE,
      &WaitingCallback::CallOnHandleReady,
      waiting_callback.get());
  return waiting_callback;
}

void WaitingCallback::Cancel() {
  if (!wait_id_)
    return;

  Environment::GetDefaultAsyncWaiter()->CancelWait(wait_id_);
  wait_id_ = 0;
}

WaitingCallback::WaitingCallback(v8::Isolate* isolate,
                                 v8::Handle<v8::Function> callback)
      : wait_id_() {
  v8::Handle<v8::Context> context = isolate->GetCurrentContext();
  runner_ = gin::PerContextData::From(context)->runner()->GetWeakPtr();
  GetWrapper(isolate)->SetHiddenValue(GetHiddenPropertyName(isolate), callback);
}

WaitingCallback::~WaitingCallback() {
  Cancel();
}

// static
void WaitingCallback::CallOnHandleReady(void* closure, MojoResult result) {
  static_cast<WaitingCallback*>(closure)->OnHandleReady(result);
}

void WaitingCallback::OnHandleReady(MojoResult result) {
  wait_id_ = 0;

  if (!runner_)
    return;

  gin::Runner::Scope scope(runner_.get());
  v8::Isolate* isolate = runner_->GetContextHolder()->isolate();

  v8::Handle<v8::Value> hidden_value =
      GetWrapper(isolate)->GetHiddenValue(GetHiddenPropertyName(isolate));
  v8::Handle<v8::Function> callback;
  CHECK(gin::ConvertFromV8(isolate, hidden_value, &callback));

  v8::Handle<v8::Value> args[] = { gin::ConvertToV8(isolate, result) };
  runner_->Call(callback, runner_->global(), 1, args);
}

}  // namespace js
}  // namespace mojo
