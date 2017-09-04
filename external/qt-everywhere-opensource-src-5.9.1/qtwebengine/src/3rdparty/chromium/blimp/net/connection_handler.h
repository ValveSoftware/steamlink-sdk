// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_CONNECTION_HANDLER_H_
#define BLIMP_NET_CONNECTION_HANDLER_H_

#include <memory>

namespace blimp {

class BlimpConnection;

// Interface for objects that can take possession of BlimpConnections.
class ConnectionHandler {
 public:
  virtual ~ConnectionHandler() {}

  virtual void HandleConnection(
      std::unique_ptr<BlimpConnection> connection) = 0;
};

}  // namespace blimp

#endif  // BLIMP_NET_CONNECTION_HANDLER_H_
