// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_CONNECT_PARAMS_H_
#define SERVICES_SHELL_CONNECT_PARAMS_H_

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "services/shell/public/cpp/identity.h"
#include "services/shell/public/interfaces/connector.mojom.h"
#include "services/shell/public/interfaces/interface_provider.mojom.h"

namespace shell {

// This class represents a request for the application manager to connect to an
// application.
class ConnectParams {
 public:
  ConnectParams();
  ~ConnectParams();

  void set_source(const Identity& source) { source_ = source;  }
  const Identity& source() const { return source_; }
  void set_target(const Identity& target) { target_ = target; }
  const Identity& target() const { return target_; }

  void set_remote_interfaces(mojom::InterfaceProviderRequest value) {
    remote_interfaces_ = std::move(value);
  }
  mojom::InterfaceProviderRequest TakeRemoteInterfaces() {
    return std::move(remote_interfaces_);
  }

  void set_local_interfaces(mojom::InterfaceProviderPtr value) {
    local_interfaces_ = std::move(value);
  }
  mojom::InterfaceProviderPtr TakeLocalInterfaces() {
    return std::move(local_interfaces_);
  }

  void set_client_process_connection(
      mojom::ClientProcessConnectionPtr client_process_connection) {
    client_process_connection_ = std::move(client_process_connection);
  }
  mojom::ClientProcessConnectionPtr TakeClientProcessConnection() {
    return std::move(client_process_connection_);
  }

  void set_connect_callback(const mojom::Connector::ConnectCallback& value) {
    connect_callback_ = value;
  }
  const mojom::Connector::ConnectCallback& connect_callback() const {
    return connect_callback_;
  }

 private:
  // It may be null (i.e., is_null() returns true) which indicates that there is
  // no source (e.g., for the first application or in tests).
  Identity source_;
  // The identity of the application being connected to.
  Identity target_;

  mojom::InterfaceProviderRequest remote_interfaces_;
  mojom::InterfaceProviderPtr local_interfaces_;
  mojom::ClientProcessConnectionPtr client_process_connection_;
  mojom::Connector::ConnectCallback connect_callback_;

  DISALLOW_COPY_AND_ASSIGN(ConnectParams);
};

}  // namespace shell

#endif  // SERVICES_SHELL_CONNECT_PARAMS_H_
