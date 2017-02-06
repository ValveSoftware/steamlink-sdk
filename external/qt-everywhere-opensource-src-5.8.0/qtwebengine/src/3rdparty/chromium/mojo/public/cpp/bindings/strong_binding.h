// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STRONG_BINDING_H_
#define MOJO_PUBLIC_CPP_BINDINGS_STRONG_BINDING_H_

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/lib/filter_chain.h"
#include "mojo/public/cpp/bindings/lib/router.h"
#include "mojo/public/cpp/bindings/message_header_validator.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {

// This connects an interface implementation strongly to a pipe. When a
// connection error is detected the implementation is deleted. Deleting the
// connector also closes the pipe.
//
// Example of an implementation that is always bound strongly to a pipe
//
//   class StronglyBound : public Foo {
//    public:
//     explicit StronglyBound(InterfaceRequest<Foo> request)
//         : binding_(this, std::move(request)) {}
//
//     // Foo implementation here
//
//    private:
//     StrongBinding<Foo> binding_;
//   };
//
//   class MyFooFactory : public InterfaceFactory<Foo> {
//    public:
//     void Create(..., InterfaceRequest<Foo> request) override {
//       new StronglyBound(std::move(request));  // The binding now owns the
//                                               // instance of StronglyBound.
//     }
//   };
//
// This class is thread hostile once it is bound to a message pipe. Until it is
// bound, it may be bound or destroyed on any thread.
template <typename Interface>
class StrongBinding {
 public:
  explicit StrongBinding(Interface* impl) : binding_(impl) {}

  StrongBinding(Interface* impl, ScopedMessagePipeHandle handle)
      : StrongBinding(impl) {
    Bind(std::move(handle));
  }

  StrongBinding(Interface* impl, InterfacePtr<Interface>* ptr)
      : StrongBinding(impl) {
    Bind(ptr);
  }

  StrongBinding(Interface* impl, InterfaceRequest<Interface> request)
      : StrongBinding(impl) {
    Bind(std::move(request));
  }

  ~StrongBinding() {}

  void Bind(ScopedMessagePipeHandle handle) {
    DCHECK(!binding_.is_bound());
    binding_.Bind(std::move(handle));
    binding_.set_connection_error_handler(
        base::Bind(&StrongBinding::OnConnectionError, base::Unretained(this)));
  }

  void Bind(InterfacePtr<Interface>* ptr) {
    DCHECK(!binding_.is_bound());
    binding_.Bind(ptr);
    binding_.set_connection_error_handler(
        base::Bind(&StrongBinding::OnConnectionError, base::Unretained(this)));
  }

  void Bind(InterfaceRequest<Interface> request) {
    DCHECK(!binding_.is_bound());
    binding_.Bind(std::move(request));
    binding_.set_connection_error_handler(
        base::Bind(&StrongBinding::OnConnectionError, base::Unretained(this)));
  }

  bool WaitForIncomingMethodCall() {
    return binding_.WaitForIncomingMethodCall();
  }

  // Note: The error handler must not delete the interface implementation.
  //
  // This method may only be called after this StrongBinding has been bound to a
  // message pipe.
  void set_connection_error_handler(const base::Closure& error_handler) {
    DCHECK(binding_.is_bound());
    connection_error_handler_ = error_handler;
  }

  Interface* impl() { return binding_.impl(); }
  // Exposed for testing, should not generally be used.
  internal::Router* internal_router() { return binding_.internal_router(); }

  void OnConnectionError() {
    if (!connection_error_handler_.is_null())
      connection_error_handler_.Run();
    delete binding_.impl();
  }

 private:
  base::Closure connection_error_handler_;
  Binding<Interface> binding_;

  DISALLOW_COPY_AND_ASSIGN(StrongBinding);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_STRONG_BINDING_H_
