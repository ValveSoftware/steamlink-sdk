// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_SESSION_CLIENT_NETWORK_COMPONENTS_H_
#define BLIMP_CLIENT_CORE_SESSION_CLIENT_NETWORK_COMPONENTS_H_

#include <memory>

#include "base/threading/thread_checker.h"
#include "blimp/client/core/session/assignment_source.h"
#include "blimp/client/core/session/network_event_observer.h"
#include "blimp/net/blimp_connection.h"
#include "blimp/net/browser_connection_handler.h"
#include "blimp/net/client_connection_manager.h"

namespace blimp {
namespace client {

// This class's functions and destruction must all be invoked on the IO thread
// by the owner. It is OK to construct the object on the main thread. The
// ThreadChecker is initialized in the call to Initialize.
class ClientNetworkComponents : public ConnectionHandler,
                                public ConnectionErrorObserver {
 public:
  // Can be created on any thread.
  explicit ClientNetworkComponents(
      std::unique_ptr<NetworkEventObserver> observer);
  ~ClientNetworkComponents() override;

  // Sets up network components.
  void Initialize();

  // Starts the connection to the engine using the given |assignment|.
  // It is required to first call Initialize.
  void ConnectWithAssignment(const Assignment& assignment);

  BrowserConnectionHandler* GetBrowserConnectionHandler();

 private:
  // ConnectionHandler implementation.
  void HandleConnection(std::unique_ptr<BlimpConnection> connection) override;

  // ConnectionErrorObserver implementation.
  void OnConnectionError(int error) override;

  // The ThreadChecker is reset during the call to Initialize.
  base::ThreadChecker io_thread_checker_;

  BrowserConnectionHandler connection_handler_;
  std::unique_ptr<ClientConnectionManager> connection_manager_;
  std::unique_ptr<NetworkEventObserver> network_observer_;

  DISALLOW_COPY_AND_ASSIGN(ClientNetworkComponents);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_SESSION_CLIENT_NETWORK_COMPONENTS_H_
