// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_ENGINE_AUTHENTICATION_HANDLER_H_
#define BLIMP_NET_ENGINE_AUTHENTICATION_HANDLER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "blimp/net/blimp_net_export.h"
#include "blimp/net/connection_handler.h"

namespace blimp {

class BlimpConnection;
class BlimpMessage;

// Authenticates connections and passes successfully authenticated connections
// to |connection_handler|.
class BLIMP_NET_EXPORT EngineAuthenticationHandler : public ConnectionHandler {
 public:
  // |client_token|: used to authenticate incoming connection.
  // |connection_handler|: a new connection is passed on to it after the
  //    connection is authenticate this handler.
  EngineAuthenticationHandler(ConnectionHandler* connection_handler,
                              const std::string& client_token);

  ~EngineAuthenticationHandler() override;

  // ConnectionHandler implementation.
  void HandleConnection(std::unique_ptr<BlimpConnection> connection) override;

 private:
  // Used to abandon pending authenticated connections if |this| is deleted.
  base::WeakPtrFactory<ConnectionHandler> connection_handler_weak_factory_;

  // Used to authenticate incoming connection. Engine is assigned to one client
  // only, and all connections from that client shall carry the same token.
  const std::string client_token_;

  DISALLOW_COPY_AND_ASSIGN(EngineAuthenticationHandler);
};

}  // namespace blimp

#endif  // BLIMP_NET_ENGINE_AUTHENTICATION_HANDLER_H_
