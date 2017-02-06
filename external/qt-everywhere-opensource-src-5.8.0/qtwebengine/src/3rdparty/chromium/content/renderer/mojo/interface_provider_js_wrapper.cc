// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mojo/interface_provider_js_wrapper.h"

#include <memory>
#include <utility>

#include "mojo/edk/js/handle.h"
#include "services/shell/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {

gin::WrapperInfo InterfaceProviderJsWrapper::kWrapperInfo = {
    gin::kEmbedderNativeGin};
const char InterfaceProviderJsWrapper::kPerFrameModuleName[] =
    "content/public/renderer/frame_service_registry";
const char InterfaceProviderJsWrapper::kPerProcessModuleName[] =
    "content/public/renderer/service_registry";

InterfaceProviderJsWrapper::~InterfaceProviderJsWrapper() {
}

// static
gin::Handle<InterfaceProviderJsWrapper>
InterfaceProviderJsWrapper::Create(
    v8::Isolate* isolate,
    v8::Handle<v8::Context> context,
    shell::InterfaceProvider* remote_interfaces) {
  return gin::CreateHandle(
      isolate,
      new InterfaceProviderJsWrapper(
          isolate, context,
        remote_interfaces->GetWeakPtr()));
}

gin::ObjectTemplateBuilder
InterfaceProviderJsWrapper::GetObjectTemplateBuilder(v8::Isolate* isolate) {
  return Wrappable<InterfaceProviderJsWrapper>::GetObjectTemplateBuilder(
      isolate)
      .SetMethod("connectToService",
                 &InterfaceProviderJsWrapper::GetInterface)
      .SetMethod("addServiceOverrideForTesting",
                 &InterfaceProviderJsWrapper::AddOverrideForTesting)
      .SetMethod("clearServiceOverridesForTesting",
                 &InterfaceProviderJsWrapper::ClearOverridesForTesting);
}

mojo::Handle InterfaceProviderJsWrapper::GetInterface(
    const std::string& interface_name) {
  mojo::MessagePipe pipe;
  if (remote_interfaces_) {
    remote_interfaces_->GetInterface(
        interface_name, std::move(pipe.handle0));
  }
  return pipe.handle1.release();
}

void InterfaceProviderJsWrapper::AddOverrideForTesting(
    const std::string& interface_name,
    v8::Local<v8::Function> service_factory) {
  ScopedJsFactory factory(v8::Isolate::GetCurrent(), service_factory);
  shell::InterfaceProvider::TestApi test_api(remote_interfaces_.get());
  test_api.SetBinderForName(
      interface_name,
      base::Bind(&InterfaceProviderJsWrapper::CallJsFactory,
                 weak_factory_.GetWeakPtr(), factory));
}

void InterfaceProviderJsWrapper::ClearOverridesForTesting() {
  shell::InterfaceProvider::TestApi test_api(remote_interfaces_.get());
  test_api.ClearBinders();
}

InterfaceProviderJsWrapper::InterfaceProviderJsWrapper(
    v8::Isolate* isolate,
    v8::Handle<v8::Context> context,
    base::WeakPtr<shell::InterfaceProvider> remote_interfaces)
    : isolate_(isolate),
      context_(isolate, context),
      remote_interfaces_(remote_interfaces),
      weak_factory_(this) {
  context_.SetWeak(this, &InterfaceProviderJsWrapper::ClearContext,
                   v8::WeakCallbackType::kParameter);
}

void InterfaceProviderJsWrapper::CallJsFactory(
    const ScopedJsFactory& factory,
    mojo::ScopedMessagePipeHandle pipe) {
  if (context_.IsEmpty())
    return;

  v8::HandleScope handle_scope(isolate_);
  v8::Handle<v8::Context> context = context_.Get(isolate_);
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Value> argv[] = {
      gin::ConvertToV8(isolate_, mojo::Handle(pipe.release().value()))};
  blink::WebLocalFrame::frameForContext(context)
      ->callFunctionEvenIfScriptDisabled(factory.Get(isolate_),
                                         v8::Undefined(isolate_), 1, argv);
}

// static
void InterfaceProviderJsWrapper::ClearContext(
    const v8::WeakCallbackInfo<InterfaceProviderJsWrapper>& data) {
  InterfaceProviderJsWrapper* registry = data.GetParameter();
  registry->context_.Reset();
}

}  // namespace content
