// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/interface_registry.h"

#include <sstream>

#include "mojo/public/cpp/bindings/message.h"
#include "services/service_manager/public/cpp/connection.h"

namespace service_manager {
namespace {

// Returns the set of capabilities required from the target.
CapabilitySet GetRequestedCapabilities(const InterfaceProviderSpec& source_spec,
                                       const Identity& target) {
  CapabilitySet capabilities;

  // Start by looking for specs specific to the supplied identity.
  auto it = source_spec.requires.find(target.name());
  if (it != source_spec.requires.end()) {
    std::copy(it->second.begin(), it->second.end(),
              std::inserter(capabilities, capabilities.begin()));
  }

  // Apply wild card rules too.
  it = source_spec.requires.find("*");
  if (it != source_spec.requires.end()) {
    std::copy(it->second.begin(), it->second.end(),
              std::inserter(capabilities, capabilities.begin()));
  }
  return capabilities;
}

// Generates a single set of interfaces that is the union of all interfaces
// exposed by the target for the capabilities requested by the source.
InterfaceSet GetInterfacesToExpose(
    const InterfaceProviderSpec& source_spec,
    const Identity& target,
    const InterfaceProviderSpec& target_spec) {
  InterfaceSet exposed_interfaces;
  // TODO(beng): remove this once we can assert that an InterfaceRegistry must
  //             always be constructed with a valid identity.
  if (!target.IsValid()) {
    exposed_interfaces.insert("*");
    return exposed_interfaces;
  }
  CapabilitySet capabilities = GetRequestedCapabilities(source_spec, target);
  for (const auto& capability : capabilities) {
    auto it = target_spec.provides.find(capability);
    if (it != target_spec.provides.end()) {
      for (const auto& interface_name : it->second)
        exposed_interfaces.insert(interface_name);
    }
  }
  return exposed_interfaces;
}

void SerializeIdentity(const Identity& identity, std::stringstream* stream) {
  *stream << identity.name() << "@" << identity.instance() << " run as: "
          << identity.user_id();
}

void SerializeSpec(const InterfaceProviderSpec& spec,
                   std::stringstream* stream) {
  *stream << "  Providing:\n";
  for (const auto& entry : spec.provides) {
    *stream << "    capability: " << entry.first << " containing interfaces:\n";
    for (const auto& interface_name : entry.second)
      *stream << "      " << interface_name << "\n";
  }
  *stream << "\n  Requiring:\n";
  for (const auto& entry : spec.requires) {
    *stream << "    From: " << entry.first << ":\n";
    for (const auto& capability_name : entry.second)
      *stream << "      " << capability_name << "\n";
  }
}

}  // namespace

InterfaceRegistry::InterfaceRegistry(const std::string& name)
    : binding_(this),
      name_(name),
      weak_factory_(this) {}
InterfaceRegistry::~InterfaceRegistry() {}

void InterfaceRegistry::Bind(
    mojom::InterfaceProviderRequest local_interfaces_request,
    const Identity& local_identity,
    const InterfaceProviderSpec& local_interface_provider_spec,
    const Identity& remote_identity,
    const InterfaceProviderSpec& remote_interface_provider_spec) {
  DCHECK(!binding_.is_bound());
  local_identity_ = local_identity;
  local_interface_provider_spec_ = local_interface_provider_spec;
  remote_identity_ = remote_identity;
  remote_interface_provider_spec_ = remote_interface_provider_spec;
  RebuildExposedInterfaces();
  binding_.Bind(std::move(local_interfaces_request));
  binding_.set_connection_error_handler(base::Bind(
      &InterfaceRegistry::OnConnectionError, base::Unretained(this)));
}

void InterfaceRegistry::Serialize(std::stringstream* stream) {
  *stream << "\n\nInterfaceRegistry(" << name_ << "):\n";
  if (!binding_.is_bound()) {
    *stream << "\n  --> InterfaceRegistry is not yet bound to a pipe.\n\n";
    return;
  }

  *stream << "Owned by:\n  ";
  SerializeIdentity(local_identity_, stream);
  *stream << "\n\n";
  SerializeSpec(local_interface_provider_spec_, stream);

  *stream << "\n";

  *stream << "Bound to:\n  ";
  SerializeIdentity(remote_identity_, stream);
  *stream << "\n\n";
  SerializeSpec(remote_interface_provider_spec_, stream);

  *stream << "\nBinders registered for:\n";
  bool found_exposed = false;
  for (const auto& entry : name_to_binder_) {
    bool exposed = exposed_interfaces_.count(entry.first) > 0;
    found_exposed |= exposed;
    *stream << " " << (exposed ? "* " : "  ") << entry.first << "\n";
  }
  if (found_exposed)
    *stream << "\n * - denotes an interface exposed to remote per policy.\n";

  *stream << "\n\n";
  if (expose_all_interfaces_)
    *stream << "All interfaces exposed.\n\n";
}

base::WeakPtr<InterfaceRegistry> InterfaceRegistry::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool InterfaceRegistry::AddInterface(
    const std::string& name,
    const base::Callback<void(mojo::ScopedMessagePipeHandle)>& callback,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  return SetInterfaceBinderForName(
      base::MakeUnique<internal::GenericCallbackBinder>(callback, task_runner),
      name);
}

void InterfaceRegistry::RemoveInterface(const std::string& name) {
  auto it = name_to_binder_.find(name);
  if (it != name_to_binder_.end())
    name_to_binder_.erase(it);
}

void InterfaceRegistry::PauseBinding() {
  DCHECK(!is_paused_);
  is_paused_ = true;
}

void InterfaceRegistry::ResumeBinding() {
  DCHECK(is_paused_);
  is_paused_ = false;

  while (!pending_interface_requests_.empty()) {
    auto& request = pending_interface_requests_.front();
    GetInterface(request.first, std::move(request.second));
    pending_interface_requests_.pop();
  }
}

void InterfaceRegistry::GetInterfaceNames(
    std::set<std::string>* interface_names) {
  DCHECK(interface_names);
  for (auto& entry : name_to_binder_)
    interface_names->insert(entry.first);
}

void InterfaceRegistry::AddConnectionLostClosure(
    const base::Closure& connection_lost_closure) {
  connection_lost_closures_.push_back(connection_lost_closure);
}

// mojom::InterfaceProvider:
void InterfaceRegistry::GetInterface(const std::string& interface_name,
                                     mojo::ScopedMessagePipeHandle handle) {
  if (is_paused_) {
    pending_interface_requests_.emplace(interface_name, std::move(handle));
    return;
  }

  if (CanBindRequestForInterface(interface_name)) {
    auto iter = name_to_binder_.find(interface_name);
    if (iter != name_to_binder_.end()) {
      iter->second->BindInterface(remote_identity_,
                                  interface_name,
                                  std::move(handle));
    } else if (!default_binder_.is_null()) {
      default_binder_.Run(interface_name, std::move(handle));
    } else {
      std::stringstream ss;
      ss << "Failed to locate a binder for interface: " << interface_name
         << " requested by: " << remote_identity_.name() << " exposed by: "
         << local_identity_.name() << " via InterfaceProviderSpec \"" << name_
         << "\".";
      Serialize(&ss);
      LOG(ERROR) << ss.str();
    }
  } else {
    std::stringstream ss;
    ss << "InterfaceProviderSpec \"" << name_ << "\" prevented service: "
       << remote_identity_.name() << " from binding interface: "
       << interface_name << " exposed by: " << local_identity_.name();
    Serialize(&ss);
    LOG(ERROR) << ss.str();
    mojo::ReportBadMessage(ss.str());
  }
}

bool InterfaceRegistry::SetInterfaceBinderForName(
    std::unique_ptr<InterfaceBinder> binder,
    const std::string& interface_name) {
  if (CanBindRequestForInterface(interface_name)) {
    RemoveInterface(interface_name);
    name_to_binder_[interface_name] = std::move(binder);
    return true;
  }
  return false;
}

bool InterfaceRegistry::CanBindRequestForInterface(
    const std::string& interface_name) const {
  // Any interface may be registered before the registry is bound to a pipe. At
  // bind time, the interfaces exposed will be intersected with the requirements
  // of the source.
  if (!binding_.is_bound())
    return true;
  return expose_all_interfaces_ || exposed_interfaces_.count(interface_name);
}

void InterfaceRegistry::RebuildExposedInterfaces() {
  exposed_interfaces_ = GetInterfacesToExpose(remote_interface_provider_spec_,
                                              local_identity_,
                                              local_interface_provider_spec_);
  expose_all_interfaces_ =
      exposed_interfaces_.size() == 1 && exposed_interfaces_.count("*") == 1;
}

void InterfaceRegistry::OnConnectionError() {
  std::list<base::Closure> closures = connection_lost_closures_;
  for (const auto& closure : closures)
    closure.Run();
}

}  // namespace service_manager
