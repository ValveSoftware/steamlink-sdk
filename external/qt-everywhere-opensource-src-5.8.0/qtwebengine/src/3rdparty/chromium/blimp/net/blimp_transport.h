// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLIMP_TRANSPORT_H_
#define BLIMP_NET_BLIMP_TRANSPORT_H_

#include <memory>
#include <string>

#include "net/base/completion_callback.h"

namespace blimp {

class BlimpConnection;

// An interface which encapsulates the transport-specific code for
// establishing network connections between the client and engine.
// Subclasses of BlimpTransport are responsible for defining their own
// methods for receiving connection arguments.
class BlimpTransport {
 public:
  virtual ~BlimpTransport() {}

  // Initiate or listen for a connection.
  //
  // |callback| will be invoked with the connection outcome:
  //   * net::OK if a connection is established successful, the BlimpConnection
  //     can be taken by calling TakeConnection().
  //   * net::ERR_IO_PENDING will never be the outcome
  //   * Other error code indicates no connection was established.
  virtual void Connect(const net::CompletionCallback& callback) = 0;

  // Returns the connection object after a successful Connect().
  virtual std::unique_ptr<BlimpConnection> TakeConnection() = 0;

  // Gets transport name, e.g. "TCP", "SSL", "mock", etc.
  virtual const char* GetName() const = 0;
};

}  // namespace blimp

#endif  // BLIMP_NET_BLIMP_TRANSPORT_H_
