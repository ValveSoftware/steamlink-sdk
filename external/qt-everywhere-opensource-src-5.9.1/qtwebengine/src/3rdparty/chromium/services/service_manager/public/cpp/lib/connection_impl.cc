// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/lib/connection_impl.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/interface_binder.h"

namespace service_manager {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// ConnectionImpl, public:

ConnectionImpl::ConnectionImpl()
    : weak_factory_(this) {}

ConnectionImpl::ConnectionImpl(const Identity& remote, State initial_state)
    : remote_(remote),
      state_(initial_state),
      weak_factory_(this) {
}

ConnectionImpl::~ConnectionImpl() {}

void ConnectionImpl::SetRemoteInterfaces(
    std::unique_ptr<InterfaceProvider> remote_interfaces) {
  remote_interfaces_owner_ = std::move(remote_interfaces);
  set_remote_interfaces(remote_interfaces_owner_.get());
}

service_manager::mojom::Connector::ConnectCallback
ConnectionImpl::GetConnectCallback() {
  return base::Bind(&ConnectionImpl::OnConnectionCompleted,
                    weak_factory_.GetWeakPtr());
}

////////////////////////////////////////////////////////////////////////////////
// ConnectionImpl, Connection implementation:

const Identity& ConnectionImpl::GetRemoteIdentity() const {
  return remote_;
}

void ConnectionImpl::SetConnectionLostClosure(const base::Closure& handler) {
  remote_interfaces_->SetConnectionLostClosure(handler);
}

service_manager::mojom::ConnectResult ConnectionImpl::GetResult() const {
  return result_;
}

bool ConnectionImpl::IsPending() const {
  return state_ == State::PENDING;
}

void ConnectionImpl::AddConnectionCompletedClosure(
    const base::Closure& callback) {
  if (IsPending())
    connection_completed_callbacks_.push_back(callback);
  else
    callback.Run();
}

InterfaceProvider* ConnectionImpl::GetRemoteInterfaces() {
  return remote_interfaces_;
}

base::WeakPtr<Connection> ConnectionImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

////////////////////////////////////////////////////////////////////////////////
// ConnectionImpl, private:

void ConnectionImpl::OnConnectionCompleted(
    service_manager::mojom::ConnectResult result,
    const std::string& target_user_id) {
  DCHECK(State::PENDING == state_);

  result_ = result;
  state_ = result_ == service_manager::mojom::ConnectResult::SUCCEEDED
               ? State::CONNECTED
               : State::DISCONNECTED;
  remote_.set_user_id(target_user_id);
  std::vector<base::Closure> callbacks;
  callbacks.swap(connection_completed_callbacks_);
  for (auto callback : callbacks)
    callback.Run();
}

}  // namespace internal
}  // namespace service_manager
