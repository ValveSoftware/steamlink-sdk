// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_JS_HANDLE_H_
#define MOJO_EDK_JS_HANDLE_H_

#include <stdint.h>

#include "base/observer_list.h"
#include "gin/converter.h"
#include "gin/handle.h"
#include "gin/wrappable.h"
#include "mojo/edk/js/js_export.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {
namespace edk {
namespace js {

class HandleCloseObserver;

// Wrapper for mojo Handles exposed to JavaScript. This ensures the Handle
// is Closed when its JS object is garbage collected.
class MOJO_JS_EXPORT HandleWrapper : public gin::Wrappable<HandleWrapper> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static gin::Handle<HandleWrapper> Create(v8::Isolate* isolate,
                                           MojoHandle handle) {
    return gin::CreateHandle(isolate, new HandleWrapper(handle));
  }

  mojo::Handle get() const { return handle_.get(); }
  mojo::Handle release() { return handle_.release(); }
  void Close();

  void AddCloseObserver(HandleCloseObserver* observer);
  void RemoveCloseObserver(HandleCloseObserver* observer);

 protected:
  HandleWrapper(MojoHandle handle);
  ~HandleWrapper() override;
  void NotifyCloseObservers();

  mojo::ScopedHandle handle_;
  base::ObserverList<HandleCloseObserver> close_observers_;
};

}  // namespace js
}  // namespace edk
}  // namespace mojo

namespace gin {

// Note: It's important to use this converter rather than the one for
// MojoHandle, since that will do a simple int32_t conversion. It's unfortunate
// there's no way to prevent against accidental use.
// TODO(mpcomplete): define converters for all Handle subtypes.
template <>
struct MOJO_JS_EXPORT Converter<mojo::Handle> {
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate,
                                    const mojo::Handle& val);
  static bool FromV8(v8::Isolate* isolate, v8::Handle<v8::Value> val,
                     mojo::Handle* out);
};

template <>
struct MOJO_JS_EXPORT Converter<mojo::MessagePipeHandle> {
  static v8::Handle<v8::Value> ToV8(v8::Isolate* isolate,
                                    mojo::MessagePipeHandle val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Handle<v8::Value> val,
                     mojo::MessagePipeHandle* out);
};

// We need to specialize the normal gin::Handle converter in order to handle
// converting |null| to a wrapper for an empty mojo::Handle.
template <>
struct MOJO_JS_EXPORT Converter<gin::Handle<mojo::edk::js::HandleWrapper>> {
  static v8::Handle<v8::Value> ToV8(
      v8::Isolate* isolate,
      const gin::Handle<mojo::edk::js::HandleWrapper>& val) {
    return val.ToV8();
  }

  static bool FromV8(v8::Isolate* isolate,
                     v8::Handle<v8::Value> val,
                     gin::Handle<mojo::edk::js::HandleWrapper>* out) {
    if (val->IsNull()) {
      *out = mojo::edk::js::HandleWrapper::Create(isolate, MOJO_HANDLE_INVALID);
      return true;
    }

    mojo::edk::js::HandleWrapper* object = NULL;
    if (!Converter<mojo::edk::js::HandleWrapper*>::FromV8(isolate, val,
                                                          &object)) {
      return false;
    }
    *out = gin::Handle<mojo::edk::js::HandleWrapper>(val, object);
    return true;
  }
};

}  // namespace gin

#endif  // MOJO_EDK_JS_HANDLE_H_
