// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MOJO_INTERFACE_PROVIDER_JS_WRAPPER_H_
#define CONTENT_RENDERER_MOJO_INTERFACE_PROVIDER_JS_WRAPPER_H_

#include <string>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "v8/include/v8.h"

namespace service_manager {
class InterfaceProvider;
}

namespace content {

// A JS wrapper around service_manager::InterfaceProvider that allows connecting
// to
// remote services.
class CONTENT_EXPORT InterfaceProviderJsWrapper
    : public gin::Wrappable<InterfaceProviderJsWrapper> {
 public:
  ~InterfaceProviderJsWrapper() override;
  static gin::Handle<InterfaceProviderJsWrapper> Create(
      v8::Isolate* isolate,
      v8::Handle<v8::Context> context,
      service_manager::InterfaceProvider* remote_interfaces);

  // gin::Wrappable<InterfaceProviderJsWrapper> overrides.
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  // JS interface implementation.
  void AddOverrideForTesting(const std::string& interface_name,
                             v8::Local<v8::Function> interface_factory);
  void ClearOverridesForTesting();
  mojo::Handle GetInterface(const std::string& interface_name);

  static gin::WrapperInfo kWrapperInfo;
  static const char kPerFrameModuleName[];
  static const char kPerProcessModuleName[];

 private:
  using ScopedJsFactory =
      v8::Persistent<v8::Function, v8::CopyablePersistentTraits<v8::Function>>;

  InterfaceProviderJsWrapper(
      v8::Isolate* isolate,
      v8::Handle<v8::Context> context,
      base::WeakPtr<service_manager::InterfaceProvider> remote_interfaces);

  void CallJsFactory(const ScopedJsFactory& factory,
                     mojo::ScopedMessagePipeHandle pipe);

  static void ClearContext(
      const v8::WeakCallbackInfo<InterfaceProviderJsWrapper>& data);

  v8::Isolate* isolate_;
  v8::Global<v8::Context> context_;
  base::WeakPtr<service_manager::InterfaceProvider> remote_interfaces_;

  base::WeakPtrFactory<InterfaceProviderJsWrapper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InterfaceProviderJsWrapper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MOJO_INTERFACE_PROVIDER_JS_WRAPPER_H_
