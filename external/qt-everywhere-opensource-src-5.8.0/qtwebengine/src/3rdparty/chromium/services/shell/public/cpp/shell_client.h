// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_PUBLIC_CPP_SHELL_CLIENT_H_
#define SERVICES_SHELL_PUBLIC_CPP_SHELL_CLIENT_H_

#include <stdint.h>
#include <string>

#include "base/macros.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/identity.h"

namespace shell {

class Connector;

// An interface representing an instance "known to the Mojo Shell". The
// implementation receives lifecycle messages for the instance and gets the
// opportunity to handle inbound connections brokered by the Shell. Every client
// of ShellConnection must implement this interface, and instances of this
// interface must outlive the ShellConnection.
class ShellClient {
 public:
  ShellClient();
  virtual ~ShellClient();

  // Called once a bidirectional connection with the shell has been established.
  // |identity| is the identity of the instance.
  // |id| is a unique identifier the shell uses to identify this specific
  // instance of the application.
  // Called exactly once before any other method.
  virtual void Initialize(Connector* connector,
                          const Identity& identity,
                          uint32_t id);

  virtual InterfaceProvider* GetInterfaceProviderForConnection();
  virtual InterfaceRegistry* GetInterfaceRegistryForConnection();

  // Called when a connection to this client is brokered by the shell. Override
  // to expose services to the remote application. Return true if the connection
  // should succeed. Return false if the connection should be rejected and the
  // underlying pipe closed. The default implementation returns false.
  virtual bool AcceptConnection(Connection* connection);

  // Called when ShellConnection's ShellClient binding (i.e. the pipe the
  // Mojo Shell has to talk to us over) is closed. A shell client may use this
  // as a signal to terminate. Return true from this method to tell the
  // ShellConnection to run its connection lost closure if it has one, false to
  // prevent it from being run. The default implementation returns true.
  // When used in conjunction with ApplicationRunner, returning true here quits
  // the message loop created by ApplicationRunner, which results in the app
  // quitting.
  virtual bool ShellConnectionLost();

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellClient);
};

}  // namespace shell

#endif  // SERVICES_SHELL_PUBLIC_CPP_SHELL_CLIENT_H_
