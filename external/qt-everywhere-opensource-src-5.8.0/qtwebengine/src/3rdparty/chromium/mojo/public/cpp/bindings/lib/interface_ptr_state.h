// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_STATE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_STATE_H_

#include <stdint.h>

#include <algorithm>  // For |std::swap()|.
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/cpp/bindings/interface_ptr_info.h"
#include "mojo/public/cpp/bindings/lib/control_message_proxy.h"
#include "mojo/public/cpp/bindings/lib/filter_chain.h"
#include "mojo/public/cpp/bindings/lib/multiplex_router.h"
#include "mojo/public/cpp/bindings/lib/router.h"
#include "mojo/public/cpp/bindings/message_header_validator.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace mojo {
namespace internal {

template <typename Interface, bool use_multiplex_router>
class InterfacePtrState;

// Uses a single-threaded, dedicated router. If |Interface| doesn't have any
// methods to pass associated interface pointers or requests, there won't be
// multiple interfaces running on the underlying message pipe. In that case, we
// can use this specialization to reduce cost.
template <typename Interface>
class InterfacePtrState<Interface, false> {
 public:
  InterfacePtrState() : proxy_(nullptr), router_(nullptr), version_(0u) {}

  ~InterfacePtrState() {
    // Destruction order matters here. We delete |proxy_| first, even though
    // |router_| may have a reference to it, so that destructors for any request
    // callbacks still pending can interact with the InterfacePtr.
    delete proxy_;
    delete router_;
  }

  Interface* instance() {
    ConfigureProxyIfNecessary();

    // This will be null if the object is not bound.
    return proxy_;
  }

  uint32_t version() const { return version_; }

  void QueryVersion(const base::Callback<void(uint32_t)>& callback) {
    ConfigureProxyIfNecessary();

    // Do a static cast in case the interface contains methods with the same
    // name. It is safe to capture |this| because the callback won't be run
    // after this object goes away.
    static_cast<ControlMessageProxy*>(proxy_)->QueryVersion(
        base::Bind(&InterfacePtrState::OnQueryVersion, base::Unretained(this),
                   callback));
  }

  void RequireVersion(uint32_t version) {
    ConfigureProxyIfNecessary();

    if (version <= version_)
      return;

    version_ = version;
    // Do a static cast in case the interface contains methods with the same
    // name.
    static_cast<ControlMessageProxy*>(proxy_)->RequireVersion(version);
  }

  void Swap(InterfacePtrState* other) {
    using std::swap;
    swap(other->proxy_, proxy_);
    swap(other->router_, router_);
    handle_.swap(other->handle_);
    runner_.swap(other->runner_);
    swap(other->version_, version_);
  }

  void Bind(InterfacePtrInfo<Interface> info,
            scoped_refptr<base::SingleThreadTaskRunner> runner) {
    DCHECK(!proxy_);
    DCHECK(!router_);
    DCHECK(!handle_.is_valid());
    DCHECK_EQ(0u, version_);
    DCHECK(info.is_valid());

    handle_ = info.PassHandle();
    version_ = info.version();
    runner_ = std::move(runner);
  }

  bool HasAssociatedInterfaces() const { return false; }

  bool WaitForIncomingResponse() {
    ConfigureProxyIfNecessary();

    DCHECK(router_);
    return router_->WaitForIncomingMessage(MOJO_DEADLINE_INDEFINITE);
  }

  // After this method is called, the object is in an invalid state and
  // shouldn't be reused.
  InterfacePtrInfo<Interface> PassInterface() {
    return InterfacePtrInfo<Interface>(
        router_ ? router_->PassMessagePipe() : std::move(handle_), version_);
  }

  bool is_bound() const { return handle_.is_valid() || router_; }

  bool encountered_error() const {
    return router_ ? router_->encountered_error() : false;
  }

  void set_connection_error_handler(const base::Closure& error_handler) {
    ConfigureProxyIfNecessary();

    DCHECK(router_);
    router_->set_connection_error_handler(error_handler);
  }

  // Returns true if bound and awaiting a response to a message.
  bool has_pending_callbacks() const {
    return router_ && router_->has_pending_responders();
  }

  AssociatedGroup* associated_group() { return nullptr; }

  void EnableTestingMode() {
    ConfigureProxyIfNecessary();
    router_->EnableTestingMode();
  }

 private:
  using Proxy = typename Interface::Proxy_;

  void ConfigureProxyIfNecessary() {
    // The proxy has been configured.
    if (proxy_) {
      DCHECK(router_);
      return;
    }
    // The object hasn't been bound.
    if (!handle_.is_valid())
      return;

    FilterChain filters;
    filters.Append<MessageHeaderValidator>(Interface::Name_);
    filters.Append<typename Interface::ResponseValidator_>();

    router_ = new Router(std::move(handle_), std::move(filters), false,
                         std::move(runner_));

    proxy_ = new Proxy(router_);
  }

  void OnQueryVersion(const base::Callback<void(uint32_t)>& callback,
                      uint32_t version) {
    version_ = version;
    callback.Run(version);
  }

  Proxy* proxy_;
  Router* router_;

  // |proxy_| and |router_| are not initialized until read/write with the
  // message pipe handle is needed. |handle_| is valid between the Bind() call
  // and the initialization of |proxy_| and |router_|.
  ScopedMessagePipeHandle handle_;
  scoped_refptr<base::SingleThreadTaskRunner> runner_;

  uint32_t version_;

  DISALLOW_COPY_AND_ASSIGN(InterfacePtrState);
};

// Uses a multiplexing router. If |Interface| has methods to pass associated
// interface pointers or requests, this specialization should be used.
template <typename Interface>
class InterfacePtrState<Interface, true> {
 public:
  InterfacePtrState() : version_(0u) {}

  ~InterfacePtrState() {
    endpoint_client_.reset();
    proxy_.reset();
    if (router_)
      router_->CloseMessagePipe();
  }

  Interface* instance() {
    ConfigureProxyIfNecessary();

    // This will be null if the object is not bound.
    return proxy_.get();
  }

  uint32_t version() const { return version_; }

  void QueryVersion(const base::Callback<void(uint32_t)>& callback) {
    ConfigureProxyIfNecessary();


    // Do a static cast in case the interface contains methods with the same
    // name. It is safe to capture |this| because the callback won't be run
    // after this object goes away.
    static_cast<ControlMessageProxy*>(proxy_.get())->QueryVersion(
        base::Bind(&InterfacePtrState::OnQueryVersion, base::Unretained(this),
                   callback));
  }

  void RequireVersion(uint32_t version) {
    ConfigureProxyIfNecessary();

    if (version <= version_)
      return;

    version_ = version;
    // Do a static cast in case the interface contains methods with the same
    // name.
    static_cast<ControlMessageProxy*>(proxy_.get())->RequireVersion(version);
  }

  void Swap(InterfacePtrState* other) {
    using std::swap;
    swap(other->router_, router_);
    swap(other->endpoint_client_, endpoint_client_);
    swap(other->proxy_, proxy_);
    handle_.swap(other->handle_);
    runner_.swap(other->runner_);
    swap(other->version_, version_);
  }

  void Bind(InterfacePtrInfo<Interface> info,
            scoped_refptr<base::SingleThreadTaskRunner> runner) {
    DCHECK(!router_);
    DCHECK(!endpoint_client_);
    DCHECK(!proxy_);
    DCHECK(!handle_.is_valid());
    DCHECK_EQ(0u, version_);
    DCHECK(info.is_valid());

    handle_ = info.PassHandle();
    version_ = info.version();
    runner_ = std::move(runner);
  }

  bool HasAssociatedInterfaces() const {
    return router_ ? router_->HasAssociatedEndpoints() : false;
  }

  bool WaitForIncomingResponse() {
    ConfigureProxyIfNecessary();

    DCHECK(router_);
    return router_->WaitForIncomingMessage(MOJO_DEADLINE_INDEFINITE);
  }

  // After this method is called, the object is in an invalid state and
  // shouldn't be reused.
  InterfacePtrInfo<Interface> PassInterface() {
    endpoint_client_.reset();
    proxy_.reset();
    return InterfacePtrInfo<Interface>(
        router_ ? router_->PassMessagePipe() : std::move(handle_), version_);
  }

  bool is_bound() const { return handle_.is_valid() || endpoint_client_; }

  bool encountered_error() const {
    return endpoint_client_ ? endpoint_client_->encountered_error() : false;
  }

  void set_connection_error_handler(const base::Closure& error_handler) {
    ConfigureProxyIfNecessary();

    DCHECK(endpoint_client_);
    endpoint_client_->set_connection_error_handler(error_handler);
  }

  // Returns true if bound and awaiting a response to a message.
  bool has_pending_callbacks() const {
    return endpoint_client_ && endpoint_client_->has_pending_responders();
  }

  AssociatedGroup* associated_group() {
    ConfigureProxyIfNecessary();
    return endpoint_client_->associated_group();
  }

  void EnableTestingMode() {
    ConfigureProxyIfNecessary();
    router_->EnableTestingMode();
  }

 private:
  using Proxy = typename Interface::Proxy_;

  void ConfigureProxyIfNecessary() {
    // The proxy has been configured.
    if (proxy_) {
      DCHECK(router_);
      DCHECK(endpoint_client_);
      return;
    }
    // The object hasn't been bound.
    if (!handle_.is_valid())
      return;

    router_ = new MultiplexRouter(true, std::move(handle_), runner_);
    router_->SetMasterInterfaceName(Interface::Name_);
    endpoint_client_.reset(new InterfaceEndpointClient(
        router_->CreateLocalEndpointHandle(kMasterInterfaceId), nullptr,
        base::WrapUnique(new typename Interface::ResponseValidator_()), false,
        std::move(runner_)));
    proxy_.reset(new Proxy(endpoint_client_.get()));
    proxy_->serialization_context()->group_controller =
        endpoint_client_->group_controller();
  }

  void OnQueryVersion(const base::Callback<void(uint32_t)>& callback,
                      uint32_t version) {
    version_ = version;
    callback.Run(version);
  }

  scoped_refptr<MultiplexRouter> router_;

  std::unique_ptr<InterfaceEndpointClient> endpoint_client_;
  std::unique_ptr<Proxy> proxy_;

  // |router_| (as well as other members above) is not initialized until
  // read/write with the message pipe handle is needed. |handle_| is valid
  // between the Bind() call and the initialization of |router_|.
  ScopedMessagePipeHandle handle_;
  scoped_refptr<base::SingleThreadTaskRunner> runner_;

  uint32_t version_;

  DISALLOW_COPY_AND_ASSIGN(InterfacePtrState);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_STATE_H_
