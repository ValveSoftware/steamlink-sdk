// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_PUBLIC_CPP_LIB_CONNECTOR_IMPL_H_
#define SERVICES_SHELL_PUBLIC_CPP_LIB_CONNECTOR_IMPL_H_

#include <memory>

#include "base/callback.h"
#include "base/threading/thread_checker.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/interfaces/connector.mojom.h"

namespace shell {

class ConnectorImpl : public Connector {
 public:
  explicit ConnectorImpl(mojom::ConnectorPtrInfo unbound_state);
  explicit ConnectorImpl(mojom::ConnectorPtr connector);
  ~ConnectorImpl() override;

 private:
  void OnConnectionError();

  // Connector:
  std::unique_ptr<Connection> Connect(const std::string& name) override;
  std::unique_ptr<Connection> Connect(ConnectParams* params) override;
  std::unique_ptr<Connector> Clone() override;

  mojom::ConnectorPtrInfo unbound_state_;
  mojom::ConnectorPtr connector_;

  std::unique_ptr<base::ThreadChecker> thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ConnectorImpl);
};

}  // namespace shell

#endif  // SERVICES_SHELL_PUBLIC_CPP_LIB_CONNECTOR_IMPL_H_
