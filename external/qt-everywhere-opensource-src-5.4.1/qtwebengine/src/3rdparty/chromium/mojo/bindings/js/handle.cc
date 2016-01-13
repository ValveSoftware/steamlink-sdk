// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/bindings/js/handle.h"

namespace gin {

gin::WrapperInfo HandleWrapper::kWrapperInfo = { gin::kEmbedderNativeGin };

HandleWrapper::HandleWrapper(MojoHandle handle)
    : handle_(mojo::Handle(handle)) {
}

HandleWrapper::~HandleWrapper() {
}

v8::Handle<v8::Value> Converter<mojo::Handle>::ToV8(v8::Isolate* isolate,
                                                    const mojo::Handle& val) {
  if (!val.is_valid())
    return v8::Null(isolate);
  return HandleWrapper::Create(isolate, val.value()).ToV8();
}

bool Converter<mojo::Handle>::FromV8(v8::Isolate* isolate,
                                     v8::Handle<v8::Value> val,
                                     mojo::Handle* out) {
  if (val->IsNull()) {
    *out = mojo::Handle();
    return true;
  }

  gin::Handle<HandleWrapper> handle;
  if (!Converter<gin::Handle<HandleWrapper> >::FromV8(isolate, val, &handle))
    return false;

  *out = handle->get();
  return true;
}

}  // namespace gin
