// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/interface_provider.h"

#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace service_manager {

InterfaceProvider::InterfaceProvider() : weak_factory_(this) {
  pending_request_ = GetProxy(&interface_provider_);
}

InterfaceProvider::~InterfaceProvider() {}

void InterfaceProvider::Bind(mojom::InterfaceProviderPtr interface_provider) {
  DCHECK(pending_request_.is_pending());
  DCHECK(forward_callback_.is_null());
  mojo::FuseInterface(std::move(pending_request_),
                      interface_provider.PassInterface());
}

void InterfaceProvider::Forward(const ForwardCallback& callback) {
  DCHECK(pending_request_.is_pending());
  DCHECK(forward_callback_.is_null());
  interface_provider_.reset();
  pending_request_.PassMessagePipe().reset();
  forward_callback_ = callback;
}

void InterfaceProvider::SetConnectionLostClosure(
    const base::Closure& connection_lost_closure) {
  interface_provider_.set_connection_error_handler(connection_lost_closure);
}

base::WeakPtr<InterfaceProvider> InterfaceProvider::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void InterfaceProvider::GetInterface(
    const std::string& name,
    mojo::ScopedMessagePipeHandle request_handle) {
  // Local binders can be registered via TestApi.
  auto it = binders_.find(name);
  if (it != binders_.end()) {
    it->second.Run(std::move(request_handle));
    return;
  }

  if (!forward_callback_.is_null()) {
    DCHECK(!interface_provider_.is_bound());
    forward_callback_.Run(name, std::move(request_handle));
  } else {
    DCHECK(interface_provider_.is_bound());
    interface_provider_->GetInterface(name, std::move(request_handle));
  }
}

void InterfaceProvider::ClearBinders() {
  binders_.clear();
}

}  // namespace service_manager
