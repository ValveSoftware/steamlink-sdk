// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_CLIENT_CONNECTION_MANAGER_H_
#define BLIMP_NET_CLIENT_CONNECTION_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "blimp/net/blimp_net_export.h"
#include "blimp/net/connection_error_observer.h"

namespace blimp {

class BlimpConnection;
class BlimpTransport;
class ConnectionHandler;

// Coordinates the channel creation and authentication workflows for
// outgoing (Client) network connections.
//
// TODO(haibinlu): cope with network changes that may potentially affect the
// endpoint that we're trying to connect to.
class BLIMP_NET_EXPORT ClientConnectionManager
    : public ConnectionErrorObserver {
 public:
  // Caller is responsible for ensuring that |connection_handler|
  // outlives |this|.
  explicit ClientConnectionManager(ConnectionHandler* connection_handler);

  ~ClientConnectionManager() override;

  // Adds a transport. All transports are expected to be added before invoking
  // |Connect|.
  void AddTransport(std::unique_ptr<BlimpTransport> transport);

  // Attempts to create a connection using any of the BlimpTransports in
  // |transports_|.
  // This will result in the handler being called back at-most-once.
  //
  // This is also a placeholder for automatic reconnection logic for common
  // cases such as network switches, online/offline changes.
  void Connect();

  // Sets the client token to use in the authentication message.
  void set_client_token(const std::string& client_token) {
    client_token_ = client_token;
  }

 private:
  // Tries to connect using the BlimpTransport specified at |transport_index|.
  void Connect(int transport_index);

  // Callback invoked by transports_[transport_index] to indicate that it has a
  // connection ready to be authenticated or there is an error.
  void OnConnectResult(int transport_index, int result);

  // Sends authentication message to the engine via |connection|.
  void SendAuthenticationMessage(std::unique_ptr<BlimpConnection> connection);

  // Invoked after the authentication message is sent to |connection|.
  // The result of the write operation is passed via |result|.
  void OnAuthenticationMessageSent(std::unique_ptr<BlimpConnection> connection,
                                   int result);

  // ConnectionErrorObserver implementation.
  void OnConnectionError(int error) override;

  std::string client_token_;
  ConnectionHandler* connection_handler_;
  std::vector<std::unique_ptr<BlimpTransport>> transports_;
  base::WeakPtrFactory<ClientConnectionManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ClientConnectionManager);
};

}  // namespace blimp

#endif  // BLIMP_NET_CLIENT_CONNECTION_MANAGER_H_
