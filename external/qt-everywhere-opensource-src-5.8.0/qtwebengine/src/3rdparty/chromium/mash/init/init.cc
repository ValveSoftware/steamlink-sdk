// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/init/init.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "mash/login/public/interfaces/login.mojom.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/connector.h"

namespace mash {
namespace init {

Init::Init()
    : connector_(nullptr) {}
Init::~Init() {}

void Init::Initialize(shell::Connector* connector,
                      const shell::Identity& identity,
                      uint32_t id) {
  connector_ = connector;
  connector_->Connect("mojo:mus");
  StartTracing();
  StartLogin();
}

bool Init::AcceptConnection(shell::Connection* connection) {
  connection->AddInterface<mojom::Init>(this);
  return true;
}

void Init::StartService(const mojo::String& name,
                        const mojo::String& user_id) {
  if (user_services_.find(user_id) == user_services_.end()) {
    shell::Connector::ConnectParams params(shell::Identity(name, user_id));
    std::unique_ptr<shell::Connection> connection =
        connector_->Connect(&params);
    connection->SetConnectionLostClosure(
        base::Bind(&Init::UserServiceQuit, base::Unretained(this), user_id));
    user_services_[user_id] = std::move(connection);
  }
}

void Init::StopServicesForUser(const mojo::String& user_id) {
  auto it = user_services_.find(user_id);
  if (it != user_services_.end())
    user_services_.erase(it);
}

void Init::Create(shell::Connection* connection, mojom::InitRequest request) {
  init_bindings_.AddBinding(this, std::move(request));
}

void Init::UserServiceQuit(const std::string& user_id) {
  auto it = user_services_.find(user_id);
  DCHECK(it != user_services_.end());
  user_services_.erase(it);
}

void Init::StartTracing() {
  connector_->Connect("mojo:tracing");
}

void Init::StartLogin() {
  login_connection_ = connector_->Connect("mojo:login");
  login_connection_->AddInterface<mojom::Init>(this);
  mash::login::mojom::LoginPtr login;
  login_connection_->GetInterface(&login);
  login->ShowLoginUI();
}

}  // namespace init
}  // namespace main
