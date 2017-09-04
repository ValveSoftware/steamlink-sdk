// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDING_STATE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDING_STATE_H_

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/bindings_export.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "mojo/public/cpp/bindings/filter_chain.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_ptr_info.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/lib/multiplex_router.h"
#include "mojo/public/cpp/bindings/lib/router.h"
#include "mojo/public/cpp/bindings/message_header_validator.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {
namespace internal {

// Base class used for templated binding primitives which bind a pipe
// exclusively to a single interface.
class MOJO_CPP_BINDINGS_EXPORT SimpleBindingState {
 public:
  SimpleBindingState();
  ~SimpleBindingState();

  void AddFilter(std::unique_ptr<MessageReceiver> filter);

  bool HasAssociatedInterfaces() const { return false; }

  void PauseIncomingMethodCallProcessing();
  void ResumeIncomingMethodCallProcessing();

  bool WaitForIncomingMethodCall(
      MojoDeadline deadline = MOJO_DEADLINE_INDEFINITE);

  void Close();
  void CloseWithReason(uint32_t custom_reason, const std::string& description);

  void set_connection_error_handler(const base::Closure& error_handler) {
    DCHECK(is_bound());
    router_->set_connection_error_handler(error_handler);
  }

  void set_connection_error_with_reason_handler(
      const ConnectionErrorWithReasonCallback& error_handler) {
    DCHECK(is_bound());
    router_->set_connection_error_with_reason_handler(error_handler);
  }

  bool is_bound() const { return !!router_; }

  MessagePipeHandle handle() const {
    DCHECK(is_bound());
    return router_->handle();
  }

  AssociatedGroup* associated_group() { return nullptr; }

  void FlushForTesting();

  void EnableTestingMode();

 protected:
  void BindInternal(ScopedMessagePipeHandle handle,
                    scoped_refptr<base::SingleThreadTaskRunner> runner,
                    const char* interface_name,
                    std::unique_ptr<MessageReceiver> request_validator,
                    bool has_sync_methods,
                    MessageReceiverWithResponderStatus* stub,
                    uint32_t interface_version);

  void DestroyRouter();

  internal::Router* router_ = nullptr;
};

template <typename Interface, bool use_multiplex_router, typename ImplRefTraits>
class BindingState;

// Uses a single-threaded, dedicated router. If |Interface| doesn't have any
// methods to pass associated interface pointers or requests, there won't be
// multiple interfaces running on the underlying message pipe. In that case, we
// can use this specialization to reduce cost.
template <typename Interface, typename ImplRefTraits>
class BindingState<Interface, false, ImplRefTraits>
    : public SimpleBindingState {
 public:
  using ImplPointerType = typename ImplRefTraits::PointerType;

  explicit BindingState(ImplPointerType impl) {
    stub_.set_sink(std::move(impl));
  }

  ~BindingState() { Close(); }

  void Bind(ScopedMessagePipeHandle handle,
            scoped_refptr<base::SingleThreadTaskRunner> runner) {
    DCHECK(!router_);
    SimpleBindingState::BindInternal(
        std::move(handle), runner, Interface::Name_,
        base::MakeUnique<typename Interface::RequestValidator_>(),
        Interface::HasSyncMethods_, &stub_, Interface::Version_);
  }

  InterfaceRequest<Interface> Unbind() {
    InterfaceRequest<Interface> request =
        MakeRequest<Interface>(router_->PassMessagePipe());
    DestroyRouter();
    return std::move(request);
  }

  Interface* impl() { return ImplRefTraits::GetRawPointer(&stub_.sink()); }

 private:
  typename Interface::template Stub_<ImplRefTraits> stub_;

  DISALLOW_COPY_AND_ASSIGN(BindingState);
};

// Base class used for templated binding primitives which may bind a pipe to
// multiple interfaces.
class MOJO_CPP_BINDINGS_EXPORT MultiplexedBindingState {
 public:
  MultiplexedBindingState();
  ~MultiplexedBindingState();

  void AddFilter(std::unique_ptr<MessageReceiver> filter);

  bool HasAssociatedInterfaces() const;

  void PauseIncomingMethodCallProcessing();
  void ResumeIncomingMethodCallProcessing();

  bool WaitForIncomingMethodCall(
      MojoDeadline deadline = MOJO_DEADLINE_INDEFINITE);

  void Close();
  void CloseWithReason(uint32_t custom_reason, const std::string& description);

  void set_connection_error_handler(const base::Closure& error_handler) {
    DCHECK(is_bound());
    endpoint_client_->set_connection_error_handler(error_handler);
  }

  void set_connection_error_with_reason_handler(
      const ConnectionErrorWithReasonCallback& error_handler) {
    DCHECK(is_bound());
    endpoint_client_->set_connection_error_with_reason_handler(error_handler);
  }

  bool is_bound() const { return !!router_; }

  MessagePipeHandle handle() const {
    DCHECK(is_bound());
    return router_->handle();
  }

  AssociatedGroup* associated_group() {
    return endpoint_client_ ? endpoint_client_->associated_group() : nullptr;
  }

  void FlushForTesting();

  void EnableTestingMode();

 protected:
  void BindInternal(ScopedMessagePipeHandle handle,
                    scoped_refptr<base::SingleThreadTaskRunner> runner,
                    const char* interface_name,
                    std::unique_ptr<MessageReceiver> request_validator,
                    bool passes_associated_kinds,
                    bool has_sync_methods,
                    MessageReceiverWithResponderStatus* stub,
                    uint32_t interface_version);

  scoped_refptr<internal::MultiplexRouter> router_;
  std::unique_ptr<InterfaceEndpointClient> endpoint_client_;
};

// Uses a multiplexing router. If |Interface| has methods to pass associated
// interface pointers or requests, this specialization should be used.
template <typename Interface, typename ImplRefTraits>
class BindingState<Interface, true, ImplRefTraits>
    : public MultiplexedBindingState {
 public:
  using ImplPointerType = typename ImplRefTraits::PointerType;

  explicit BindingState(ImplPointerType impl) {
    stub_.set_sink(std::move(impl));
  }

  ~BindingState() { Close(); }

  void Bind(ScopedMessagePipeHandle handle,
            scoped_refptr<base::SingleThreadTaskRunner> runner) {
    MultiplexedBindingState::BindInternal(
        std::move(handle), runner, Interface::Name_,
        base::MakeUnique<typename Interface::RequestValidator_>(),
        Interface::PassesAssociatedKinds_, Interface::HasSyncMethods_, &stub_,
        Interface::Version_);
    if (Interface::PassesAssociatedKinds_)
      stub_.serialization_context()->group_controller = router_;
  }

  InterfaceRequest<Interface> Unbind() {
    endpoint_client_.reset();
    InterfaceRequest<Interface> request =
        MakeRequest<Interface>(router_->PassMessagePipe());
    router_ = nullptr;
    return request;
  }

  Interface* impl() { return ImplRefTraits::GetRawPointer(&stub_.sink()); }

 private:
  typename Interface::template Stub_<ImplRefTraits> stub_;

  DISALLOW_COPY_AND_ASSIGN(BindingState);
};

}  // namesapce internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDING_STATE_H_
