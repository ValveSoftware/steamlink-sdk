// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_PUBLIC_CPP_LIB_CONNECTION_IMPL_H_
#define SERVICES_SHELL_PUBLIC_CPP_LIB_CONNECTION_IMPL_H_

#include <stdint.h>

#include <set>
#include <string>

#include "base/callback.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/shell/public/cpp/capabilities.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/identity.h"
#include "services/shell/public/interfaces/connector.mojom.h"
#include "services/shell/public/interfaces/interface_provider.mojom.h"

namespace shell {
namespace internal {

// A ConnectionImpl represents each half of a connection between two
// applications, allowing customization of which interfaces are published to the
// other.
class ConnectionImpl : public Connection {
 public:
  ConnectionImpl();
  // |allowed_interfaces| are the set of interfaces that the shell has allowed
  // an application to expose to another application. If this set contains only
  // the string value "*" all interfaces may be exposed.
  ConnectionImpl(const std::string& connection_name,
                 const Identity& remote,
                 uint32_t remote_id,
                 const CapabilityRequest& capability_request,
                 State initial_state);
  ~ConnectionImpl() override;

  // Sets the local registry & remote provider, transferring ownership to the
  // ConnectionImpl.
  void SetExposedInterfaces(
      std::unique_ptr<InterfaceRegistry> exposed_interfaces);
  void SetRemoteInterfaces(
      std::unique_ptr<InterfaceProvider> remote_interfaces);

  // Sets the local registry & remote provider, without transferring ownership.
  void set_exposed_interfaces(InterfaceRegistry* exposed_interfaces) {
    exposed_interfaces_ = exposed_interfaces;
  }
  void set_remote_interfaces(InterfaceProvider* remote_interfaces) {
    remote_interfaces_ = remote_interfaces;
  }

  shell::mojom::Connector::ConnectCallback GetConnectCallback();

 private:
  // Connection:
  bool HasCapabilityClass(const std::string& class_name) const override;
  const std::string& GetConnectionName() override;
  const Identity& GetRemoteIdentity() const override;
  void SetConnectionLostClosure(const base::Closure& handler) override;
  shell::mojom::ConnectResult GetResult() const override;
  bool IsPending() const override;
  uint32_t GetRemoteInstanceID() const override;
  void AddConnectionCompletedClosure(const base::Closure& callback) override;
  bool AllowsInterface(const std::string& interface_name) const override;
  mojom::InterfaceProvider* GetRemoteInterfaceProvider() override;
  InterfaceRegistry* GetInterfaceRegistry() override;
  InterfaceProvider* GetRemoteInterfaces() override;
  base::WeakPtr<Connection> GetWeakPtr() override;

  void OnConnectionCompleted(shell::mojom::ConnectResult result,
                             mojo::String target_user_id,
                             uint32_t target_application_id);

  const std::string connection_name_;
  Identity remote_;
  uint32_t remote_id_ = shell::mojom::kInvalidInstanceID;

  State state_;
  shell::mojom::ConnectResult result_ = shell::mojom::ConnectResult::SUCCEEDED;
  std::vector<base::Closure> connection_completed_callbacks_;

  InterfaceRegistry* exposed_interfaces_ = nullptr;
  InterfaceProvider* remote_interfaces_ = nullptr;

  std::unique_ptr<InterfaceRegistry> exposed_interfaces_owner_;
  std::unique_ptr<InterfaceProvider> remote_interfaces_owner_;

  const CapabilityRequest capability_request_;
  const bool allow_all_interfaces_;

  base::WeakPtrFactory<ConnectionImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionImpl);
};

}  // namespace internal
}  // namespace shell

#endif  // SERVICES_SHELL_PUBLIC_CPP_LIB_CONNECTION_IMPL_H_
