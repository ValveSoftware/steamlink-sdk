// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/public/cpp/interface_registry.h"

#include "services/shell/public/cpp/connection.h"

namespace shell {

InterfaceRegistry::InterfaceRegistry(Connection* connection)
    : binding_(this), connection_(connection), weak_factory_(this) {}
InterfaceRegistry::~InterfaceRegistry() {}

void InterfaceRegistry::Bind(
    mojom::InterfaceProviderRequest local_interfaces_request) {
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(local_interfaces_request));
}

base::WeakPtr<InterfaceRegistry> InterfaceRegistry::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool InterfaceRegistry::AddInterface(
    const std::string& name,
    const base::Callback<void(mojo::ScopedMessagePipeHandle)>& callback,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  return SetInterfaceBinderForName(
      base::WrapUnique(
          new internal::GenericCallbackBinder(callback, task_runner)),
      name);
}

void InterfaceRegistry::RemoveInterface(const std::string& name) {
  auto it = name_to_binder_.find(name);
  if (it != name_to_binder_.end())
    name_to_binder_.erase(it);
}

void InterfaceRegistry::PauseBinding() {
  DCHECK(!pending_request_.is_pending());
  DCHECK(binding_.is_bound());
  pending_request_ = binding_.Unbind();
}

void InterfaceRegistry::ResumeBinding() {
  DCHECK(pending_request_.is_pending());
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(pending_request_));
}

// mojom::InterfaceProvider:
void InterfaceRegistry::GetInterface(const mojo::String& interface_name,
                                     mojo::ScopedMessagePipeHandle handle) {
  auto iter = name_to_binder_.find(interface_name);
  if (iter != name_to_binder_.end()) {
    iter->second->BindInterface(connection_, interface_name, std::move(handle));
  } else if (connection_ && !connection_->AllowsInterface(interface_name)) {
    LOG(ERROR) << "Connection CapabilityFilter prevented binding to "
               << "interface: " << interface_name << " connection_name:"
               << connection_->GetConnectionName() << " remote_name:"
               << connection_->GetRemoteIdentity().name();
  }
}

bool InterfaceRegistry::SetInterfaceBinderForName(
    std::unique_ptr<InterfaceBinder> binder,
    const std::string& interface_name) {
  if (!connection_ ||
      (connection_ && connection_->AllowsInterface(interface_name))) {
    RemoveInterface(interface_name);
    name_to_binder_[interface_name] = std::move(binder);
    return true;
  }
  return false;
}

}  // namespace shell
