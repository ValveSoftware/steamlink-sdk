// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/binding_state.h"

#include "mojo/public/cpp/bindings/lib/control_message_proxy.h"

namespace mojo {
namespace internal {

SimpleBindingState::SimpleBindingState() = default;

SimpleBindingState::~SimpleBindingState() = default;

void SimpleBindingState::AddFilter(std::unique_ptr<MessageReceiver> filter) {
  DCHECK(router_);
  router_->AddFilter(std::move(filter));
}

void SimpleBindingState::PauseIncomingMethodCallProcessing() {
  DCHECK(router_);
  router_->PauseIncomingMethodCallProcessing();
}
void SimpleBindingState::ResumeIncomingMethodCallProcessing() {
  DCHECK(router_);
  router_->ResumeIncomingMethodCallProcessing();
}

bool SimpleBindingState::WaitForIncomingMethodCall(MojoDeadline deadline) {
  DCHECK(router_);
  return router_->WaitForIncomingMessage(deadline);
}

void SimpleBindingState::Close() {
  if (!router_)
    return;

  router_->CloseMessagePipe();
  DestroyRouter();
}

void SimpleBindingState::CloseWithReason(uint32_t custom_reason,
                                         const std::string& description) {
  if (router_)
    router_->control_message_proxy()->SendDisconnectReason(custom_reason,
                                                           description);
  Close();
}

void SimpleBindingState::FlushForTesting() {
  router_->control_message_proxy()->FlushForTesting();
}

void SimpleBindingState::EnableTestingMode() {
  DCHECK(is_bound());
  router_->EnableTestingMode();
}

void SimpleBindingState::BindInternal(
    ScopedMessagePipeHandle handle,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const char* interface_name,
    std::unique_ptr<MessageReceiver> request_validator,
    bool has_sync_methods,
    MessageReceiverWithResponderStatus* stub,
    uint32_t interface_version) {
  FilterChain filters;
  filters.Append<MessageHeaderValidator>(interface_name);
  filters.Append(std::move(request_validator));

  router_ = new internal::Router(std::move(handle), std::move(filters),
                                 has_sync_methods, std::move(runner),
                                 interface_version);
  router_->set_incoming_receiver(stub);
}

void SimpleBindingState::DestroyRouter() {
  delete router_;
  router_ = nullptr;
}

// -----------------------------------------------------------------------------

MultiplexedBindingState::MultiplexedBindingState() = default;

MultiplexedBindingState::~MultiplexedBindingState() = default;

void MultiplexedBindingState::AddFilter(
    std::unique_ptr<MessageReceiver> filter) {
  DCHECK(endpoint_client_);
  endpoint_client_->AddFilter(std::move(filter));
}

bool MultiplexedBindingState::HasAssociatedInterfaces() const {
  return router_ ? router_->HasAssociatedEndpoints() : false;
}

void MultiplexedBindingState::PauseIncomingMethodCallProcessing() {
  DCHECK(router_);
  router_->PauseIncomingMethodCallProcessing();
}
void MultiplexedBindingState::ResumeIncomingMethodCallProcessing() {
  DCHECK(router_);
  router_->ResumeIncomingMethodCallProcessing();
}

bool MultiplexedBindingState::WaitForIncomingMethodCall(MojoDeadline deadline) {
  DCHECK(router_);
  return router_->WaitForIncomingMessage(deadline);
}

void MultiplexedBindingState::Close() {
  if (!router_)
    return;

  endpoint_client_.reset();
  router_->CloseMessagePipe();
  router_ = nullptr;
}

void MultiplexedBindingState::CloseWithReason(uint32_t custom_reason,
                                              const std::string& description) {
  if (endpoint_client_)
    endpoint_client_->control_message_proxy()->SendDisconnectReason(
        custom_reason, description);
  Close();
}

void MultiplexedBindingState::FlushForTesting() {
  endpoint_client_->control_message_proxy()->FlushForTesting();
}

void MultiplexedBindingState::EnableTestingMode() {
  DCHECK(is_bound());
  router_->EnableTestingMode();
}

void MultiplexedBindingState::BindInternal(
    ScopedMessagePipeHandle handle,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const char* interface_name,
    std::unique_ptr<MessageReceiver> request_validator,
    bool passes_associated_kinds,
    bool has_sync_methods,
    MessageReceiverWithResponderStatus* stub,
    uint32_t interface_version) {
  DCHECK(!router_);

  MultiplexRouter::Config config =
      passes_associated_kinds
          ? MultiplexRouter::MULTI_INTERFACE
          : (has_sync_methods
                 ? MultiplexRouter::SINGLE_INTERFACE_WITH_SYNC_METHODS
                 : MultiplexRouter::SINGLE_INTERFACE);
  router_ = new MultiplexRouter(std::move(handle), config, false, runner);
  router_->SetMasterInterfaceName(interface_name);

  endpoint_client_.reset(new InterfaceEndpointClient(
      router_->CreateLocalEndpointHandle(kMasterInterfaceId), stub,
      std::move(request_validator), has_sync_methods, std::move(runner),
      interface_version));
}

}  // namesapce internal
}  // namespace mojo
