// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_BINDING_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_BINDING_H_

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/associated_group_controller.h"
#include "mojo/public/cpp/bindings/associated_interface_request.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/lib/control_message_proxy.h"
#include "mojo/public/cpp/bindings/raw_ptr_impl_ref_traits.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace mojo {

class MessageReceiver;

// Represents the implementation side of an associated interface. It is similar
// to Binding, except that it doesn't own a message pipe handle.
//
// When you bind this class to a request, optionally you can specify a
// base::SingleThreadTaskRunner. This task runner must belong to the same
// thread. It will be used to dispatch incoming method calls and connection
// error notification. It is useful when you attach multiple task runners to a
// single thread for the purposes of task scheduling. Please note that incoming
// synchrounous method calls may not be run from this task runner, when they
// reenter outgoing synchrounous calls on the same thread.
template <typename Interface,
          typename ImplRefTraits = RawPtrImplRefTraits<Interface>>
class AssociatedBinding {
 public:
  using ImplPointerType = typename ImplRefTraits::PointerType;

  // Constructs an incomplete associated binding that will use the
  // implementation |impl|. It may be completed with a subsequent call to the
  // |Bind| method. Does not take ownership of |impl|, which must outlive this
  // object.
  explicit AssociatedBinding(ImplPointerType impl) { stub_.set_sink(impl); }

  // Constructs a completed associated binding of |impl|. The output |ptr_info|
  // should be passed through the message pipe endpoint referred to by
  // |associated_group| to setup the corresponding asssociated interface
  // pointer. |impl| must outlive this object.
  AssociatedBinding(ImplPointerType impl,
                    AssociatedInterfacePtrInfo<Interface>* ptr_info,
                    AssociatedGroup* associated_group,
                    scoped_refptr<base::SingleThreadTaskRunner> runner =
                        base::ThreadTaskRunnerHandle::Get())
      : AssociatedBinding(std::move(impl)) {
    Bind(ptr_info, associated_group, std::move(runner));
  }

  // Constructs a completed associated binding of |impl|. |impl| must outlive
  // the binding.
  AssociatedBinding(ImplPointerType impl,
                    AssociatedInterfaceRequest<Interface> request,
                    scoped_refptr<base::SingleThreadTaskRunner> runner =
                        base::ThreadTaskRunnerHandle::Get())
      : AssociatedBinding(std::move(impl)) {
    Bind(std::move(request), std::move(runner));
  }

  ~AssociatedBinding() {}

  // Creates an associated inteface and sets up this object as the
  // implementation side. The output |ptr_info| should be passed through the
  // message pipe endpoint referred to by |associated_group| to setup the
  // corresponding asssociated interface pointer.
  void Bind(AssociatedInterfacePtrInfo<Interface>* ptr_info,
            AssociatedGroup* associated_group,
            scoped_refptr<base::SingleThreadTaskRunner> runner =
                base::ThreadTaskRunnerHandle::Get()) {
    AssociatedInterfaceRequest<Interface> request;
    associated_group->CreateAssociatedInterface(AssociatedGroup::WILL_PASS_PTR,
                                                ptr_info, &request);
    Bind(std::move(request), std::move(runner));
  }

  // Sets up this object as the implementation side of an associated interface.
  void Bind(AssociatedInterfaceRequest<Interface> request,
            scoped_refptr<base::SingleThreadTaskRunner> runner =
                base::ThreadTaskRunnerHandle::Get()) {
    ScopedInterfaceEndpointHandle handle = request.PassHandle();

    DCHECK(handle.is_local())
        << "The AssociatedInterfaceRequest is supposed to be used at the "
        << "other side of the message pipe.";

    if (!handle.is_valid() || !handle.is_local()) {
      endpoint_client_.reset();
      return;
    }

    endpoint_client_.reset(new InterfaceEndpointClient(
        std::move(handle), &stub_,
        base::WrapUnique(new typename Interface::RequestValidator_()),
        Interface::HasSyncMethods_, std::move(runner), Interface::Version_));

    if (Interface::PassesAssociatedKinds_) {
      stub_.serialization_context()->group_controller =
          endpoint_client_->group_controller();
    }
  }

  // Adds a message filter to be notified of each incoming message before
  // dispatch. If a filter returns |false| from Accept(), the message is not
  // dispatched and the pipe is closed. Filters cannot be removed.
  void AddFilter(std::unique_ptr<MessageReceiver> filter) {
    DCHECK(endpoint_client_);
    endpoint_client_->AddFilter(std::move(filter));
  }

  // Closes the associated interface. Puts this object into a state where it can
  // be rebound.
  void Close() {
    DCHECK(endpoint_client_);
    endpoint_client_.reset();
  }

  // Similar to the method above, but also specifies a disconnect reason.
  void CloseWithReason(uint32_t custom_reason, const std::string& description) {
    DCHECK(endpoint_client_);
    endpoint_client_->control_message_proxy()->SendDisconnectReason(
        custom_reason, description);
    Close();
  }

  // Unbinds and returns the associated interface request so it can be
  // used in another context, such as on another thread or with a different
  // implementation. Puts this object into a state where it can be rebound.
  AssociatedInterfaceRequest<Interface> Unbind() {
    DCHECK(endpoint_client_);

    AssociatedInterfaceRequest<Interface> request;
    request.Bind(endpoint_client_->PassHandle());

    endpoint_client_.reset();

    return request;
  }

  // Sets an error handler that will be called if a connection error occurs.
  //
  // This method may only be called after this AssociatedBinding has been bound
  // to a message pipe. The error handler will be reset when this
  // AssociatedBinding is unbound or closed.
  void set_connection_error_handler(const base::Closure& error_handler) {
    DCHECK(is_bound());
    endpoint_client_->set_connection_error_handler(error_handler);
  }

  void set_connection_error_with_reason_handler(
      const ConnectionErrorWithReasonCallback& error_handler) {
    DCHECK(is_bound());
    endpoint_client_->set_connection_error_with_reason_handler(error_handler);
  }

  // Returns the interface implementation that was previously specified.
  Interface* impl() { return ImplRefTraits::GetRawPointer(&stub_.sink()); }

  // Indicates whether the associated binding has been completed.
  bool is_bound() const { return !!endpoint_client_; }

  // Returns the associated group that this object belongs to. Returns null if
  // the object is not bound.
  AssociatedGroup* associated_group() {
    return endpoint_client_ ? endpoint_client_->associated_group() : nullptr;
  }

  // Sends a message on the underlying message pipe and runs the current
  // message loop until its response is received. This can be used in tests to
  // verify that no message was sent on a message pipe in response to some
  // stimulus.
  void FlushForTesting() {
    endpoint_client_->control_message_proxy()->FlushForTesting();
  }

 private:
  std::unique_ptr<InterfaceEndpointClient> endpoint_client_;
  typename Interface::template Stub_<ImplRefTraits> stub_;

  DISALLOW_COPY_AND_ASSIGN(AssociatedBinding);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_BINDING_H_
