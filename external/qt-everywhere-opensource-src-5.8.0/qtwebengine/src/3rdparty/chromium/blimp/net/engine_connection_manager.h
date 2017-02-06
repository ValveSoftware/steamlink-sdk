// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_ENGINE_CONNECTION_MANAGER_H_
#define BLIMP_NET_ENGINE_CONNECTION_MANAGER_H_

#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "blimp/net/blimp_net_export.h"
#include "blimp/net/connection_handler.h"

namespace blimp {

class BlimpConnection;
class BlimpTransport;

// Coordinates the channel creation and authentication workflows for
// incoming (Engine) network connections.
//
// TODO(kmarshall): Add rate limiting and abuse handling logic.
class BLIMP_NET_EXPORT EngineConnectionManager {
 public:
  // Caller is responsible for ensuring that |connection_handler| outlives
  // |this|.
  explicit EngineConnectionManager(ConnectionHandler* connection_handler);

  ~EngineConnectionManager();

  // Adds a transport and accepts new BlimpConnections from it as fast as they
  // arrive.
  void AddTransport(std::unique_ptr<BlimpTransport> transport);

 private:
  // Invokes transport->Connect to listen for a connection.
  void Connect(BlimpTransport* transport);

  // Callback invoked by |transport| to indicate that it has a connection
  // ready to be authenticated.
  void OnConnectResult(BlimpTransport* transport, int result);

  ConnectionHandler* connection_handler_;
  std::vector<std::unique_ptr<BlimpTransport>> transports_;

  DISALLOW_COPY_AND_ASSIGN(EngineConnectionManager);
};

}  // namespace blimp

#endif  // BLIMP_NET_ENGINE_CONNECTION_MANAGER_H_
