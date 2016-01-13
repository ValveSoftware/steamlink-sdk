// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_REQUEST_H_
#define MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_REQUEST_H_

#include "mojo/public/cpp/bindings/interface_ptr.h"

namespace mojo {

// Used in methods that return instances of remote objects.
template <typename Interface>
class InterfaceRequest {
  MOJO_MOVE_ONLY_TYPE_FOR_CPP_03(InterfaceRequest, RValue)
 public:
  InterfaceRequest() {}

  InterfaceRequest(RValue other) {
    handle_ = other.object->handle_.Pass();
  }
  InterfaceRequest& operator=(RValue other) {
    handle_ = other.object->handle_.Pass();
    return *this;
  }

  // Returns true if the request has yet to be completed.
  bool is_pending() const { return handle_.is_valid(); }

  void Bind(ScopedMessagePipeHandle handle) {
    handle_ = handle.Pass();
  }

  ScopedMessagePipeHandle PassMessagePipe() {
    return handle_.Pass();
  }

 private:
  ScopedMessagePipeHandle handle_;
};

template <typename Interface>
InterfaceRequest<Interface> MakeRequest(ScopedMessagePipeHandle handle) {
  InterfaceRequest<Interface> request;
  request.Bind(handle.Pass());
  return request.Pass();
}

// Used to construct a request that synchronously binds an InterfacePtr<..>,
// making it immediately usable upon return. The resulting request object may
// then be later bound to an InterfaceImpl<..> via BindToRequest.
//
// Given the following interface:
//
//   interface Foo {
//     CreateBar(Bar& bar);
//   }
//
// The caller of CreateBar would have code similar to the following:
//
//   InterfacePtr<Foo> foo = ...;
//   InterfacePtr<Bar> bar;
//   foo->CreateBar(Get(&bar));
//
// Upon return from CreateBar, |bar| is ready to have methods called on it.
//
template <typename Interface>
InterfaceRequest<Interface> Get(InterfacePtr<Interface>* ptr) {
  MessagePipe pipe;
  ptr->Bind(pipe.handle0.Pass());
  return MakeRequest<Interface>(pipe.handle1.Pass());
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_REQUEST_H_
