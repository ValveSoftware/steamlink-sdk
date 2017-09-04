// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_SESSION_CONNECTION_STATUS_H_
#define BLIMP_CLIENT_CORE_SESSION_CONNECTION_STATUS_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "blimp/client/core/session/network_event_observer.h"
#include "blimp/client/public/session/assignment.h"
#include "net/base/ip_endpoint.h"

namespace blimp {
namespace client {

// Provides Blimp engine connection status, and also broadcasts connection
// events to observers on main thread.
class ConnectionStatus : public NetworkEventObserver {
 public:
  ConnectionStatus();
  ~ConnectionStatus() override;

  // Return if we connected to the engine.
  bool is_connected() const { return is_connected_; }

  // Get engine IP and port.
  const net::IPEndPoint& engine_endpoint() const { return engine_endpoint_; }

  // Broadcast network events.
  void AddObserver(NetworkEventObserver* observer);
  void RemoveObserver(NetworkEventObserver* observer);

  // Set connection states.
  void OnAssignmentResult(int result, const Assignment& assignment);

  base::WeakPtr<ConnectionStatus> GetWeakPtr();

 private:
  // NetworkEventObserver implementation.
  void OnConnected() override;
  void OnDisconnected(int result) override;

  // Observers listen to session events.
  base::ObserverList<NetworkEventObserver> connection_observers_;

  // IP address and port of the engine.
  net::IPEndPoint engine_endpoint_;

  // If the client is currently connected.
  bool is_connected_;

  base::WeakPtrFactory<ConnectionStatus> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionStatus);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_SESSION_CONNECTION_STATUS_H_
