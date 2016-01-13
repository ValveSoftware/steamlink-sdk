// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_PTR_H_
#define MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_PTR_H_

#include <assert.h>

#include <algorithm>

#include "mojo/public/cpp/bindings/error_handler.h"
#include "mojo/public/cpp/bindings/lib/interface_ptr_internal.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {
class ErrorHandler;

// InterfacePtr represents a proxy to a remote instance of an interface.
template <typename Interface>
class InterfacePtr {
  MOJO_MOVE_ONLY_TYPE_FOR_CPP_03(InterfacePtr, RValue)
 public:
  InterfacePtr() {}

  InterfacePtr(RValue other) {
    internal_state_.Swap(&other.object->internal_state_);
  }
  InterfacePtr& operator=(RValue other) {
    reset();
    internal_state_.Swap(&other.object->internal_state_);
    return *this;
  }

  ~InterfacePtr() {}

  Interface* get() const {
    return internal_state_.instance();
  }
  Interface* operator->() const { return get(); }
  Interface& operator*() const { return *get(); }

  void reset() {
    State doomed;
    internal_state_.Swap(&doomed);
  }

  // This method configures the InterfacePtr<..> to be a proxy to a remote
  // object on the other end of the given pipe.
  //
  // The proxy is bound to the current thread, which means its methods may
  // only be called on the current thread.
  //
  // To move a bound InterfacePtr<..> to another thread, call
  // ResetAndReturnMessagePipe. Then create a new InterfacePtr<..> on another
  // thread, and bind the new InterfacePtr<..> to the message pipe on that
  // thread.
  void Bind(
      ScopedMessagePipeHandle handle,
      const MojoAsyncWaiter* waiter = Environment::GetDefaultAsyncWaiter()) {
    reset();
    internal_state_.ConfigureProxy(handle.Pass(), waiter);
  }

  // The client interface may only be set after this InterfacePtr<..> is bound.
  void set_client(typename Interface::Client* client) {
    internal_state_.set_client(client);
  }

  // This method may be called to query if the underlying pipe has encountered
  // an error. If true, this means method calls made on this interface will be
  // dropped (and may have already been dropped) on the floor.
  bool encountered_error() const {
    assert(internal_state_.router());
    return internal_state_.router()->encountered_error();
  }

  // This method may be called to register an ErrorHandler to observe a
  // connection error on the underlying pipe. The callback runs asynchronously
  // from the current message loop.
  void set_error_handler(ErrorHandler* error_handler) {
    assert(internal_state_.router());
    internal_state_.router()->set_error_handler(error_handler);
  }

  // Returns the underlying message pipe handle (if any) and resets the
  // InterfacePtr<..> to its uninitialized state. This method is helpful if you
  // need to move a proxy to another thread. See related notes for Bind.
  ScopedMessagePipeHandle PassMessagePipe() {
    State state;
    internal_state_.Swap(&state);
    return state.router() ?
        state.router()->PassMessagePipe() : ScopedMessagePipeHandle();
  }

  // DO NOT USE. Exposed only for internal use and for testing.
  internal::InterfacePtrState<Interface>* internal_state() {
    return &internal_state_;
  }

 private:
  typedef internal::InterfacePtrState<Interface> State;
  State internal_state_;
};

// Takes a handle to the proxy end-point of a pipe. On the other end is
// presumed to be an interface implementation of type |Interface|. Returns a
// generated proxy to that interface, which may be used on the current thread.
// It is valid to call set_client on the returned InterfacePtr<..> to set an
// instance of Interface::Client.
template <typename Interface>
InterfacePtr<Interface> MakeProxy(
    ScopedMessagePipeHandle handle,
    const MojoAsyncWaiter* waiter = Environment::GetDefaultAsyncWaiter()) {
  InterfacePtr<Interface> ptr;
  if (handle.is_valid())
    ptr.Bind(handle.Pass(), waiter);
  return ptr.Pass();
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_PTR_H_
