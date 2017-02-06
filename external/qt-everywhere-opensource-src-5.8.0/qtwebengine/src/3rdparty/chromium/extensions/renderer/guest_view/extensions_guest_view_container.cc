// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/extensions_guest_view_container.h"

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/renderer/render_frame.h"
#include "ui/gfx/geometry/size.h"

namespace extensions {

ExtensionsGuestViewContainer::ExtensionsGuestViewContainer(
    content::RenderFrame* render_frame)
    : GuestViewContainer(render_frame),
      element_resize_isolate_(nullptr),
      weak_ptr_factory_(this) {
}

ExtensionsGuestViewContainer::~ExtensionsGuestViewContainer() {
}

void ExtensionsGuestViewContainer::OnDestroy(bool embedder_frame_destroyed) {
}

void ExtensionsGuestViewContainer::RegisterElementResizeCallback(
    v8::Local<v8::Function> callback,
    v8::Isolate* isolate) {
  element_resize_callback_.Reset(isolate, callback);
  element_resize_isolate_ = isolate;
}

void ExtensionsGuestViewContainer::DidResizeElement(const gfx::Size& new_size) {
  // Call the element resize callback, if one is registered.
  if (element_resize_callback_.IsEmpty())
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&ExtensionsGuestViewContainer::CallElementResizeCallback,
                 weak_ptr_factory_.GetWeakPtr(), new_size));
}

void ExtensionsGuestViewContainer::CallElementResizeCallback(
    const gfx::Size& new_size) {
  v8::HandleScope handle_scope(element_resize_isolate_);
  v8::Local<v8::Function> callback = v8::Local<v8::Function>::New(
      element_resize_isolate_, element_resize_callback_);
  v8::Local<v8::Context> context = callback->CreationContext();
  if (context.IsEmpty())
    return;

  const int argc = 2;
  v8::Local<v8::Value> argv[argc] = {
      v8::Integer::New(element_resize_isolate_, new_size.width()),
      v8::Integer::New(element_resize_isolate_, new_size.height())};

  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(
      element_resize_isolate_, v8::MicrotasksScope::kDoNotRunMicrotasks);

  callback->Call(context->Global(), argc, argv);
}

}  // namespace extensions
