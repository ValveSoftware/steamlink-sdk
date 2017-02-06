// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_PUBLIC_CPP_CONNECTION_H_
#define SERVICES_SHELL_PUBLIC_CPP_CONNECTION_H_

#include <stdint.h>

#include <string>
#include <utility>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "services/shell/public/cpp/connect.h"
#include "services/shell/public/cpp/identity.h"
#include "services/shell/public/cpp/interface_provider.h"
#include "services/shell/public/cpp/interface_registry.h"
#include "services/shell/public/interfaces/connector.mojom.h"
#include "services/shell/public/interfaces/interface_provider.mojom.h"

namespace shell {

class InterfaceBinder;
class InterfaceProvider;

// Represents a connection to another application. An instance of this class is
// returned from Shell's ConnectToApplication(), and passed to ShellClient's
// AcceptConnection() each time an incoming connection is received.
//
// Call AddService<T>(factory) to expose an interface to the remote application,
// and GetInterface(&interface_ptr) to consume an interface exposed by the
// remote application.
//
// Internally, this class wraps an InterfaceRegistry that accepts interfaces
// that may be exposed to a remote application. See documentation in
// interface_registry.h for more information.
//
// A Connection returned via Shell::ConnectToApplication() is owned by the
// caller.
// An Connection received via AcceptConnection is owned by the ShellConnection.
// To close a connection, call CloseConnection which will destroy this object.
class Connection {
 public:
  virtual ~Connection() {}

  enum class State {
    // The shell has not yet processed the connection.
    PENDING,

    // The shell processed the connection and it was established. GetResult()
    // returns mojom::ConnectionResult::SUCCESS.
    CONNECTED,

    // The shell processed the connection and establishment was prevented by
    // an error, call GetResult().
    DISCONNECTED
  };

  class TestApi {
   public:
    explicit TestApi(Connection* connection) : connection_(connection) {}
    base::WeakPtr<Connection> GetWeakPtr() {
      return connection_->GetWeakPtr();
    }

   private:
    Connection* connection_;
  };

  // Allow the remote application to request instances of Interface.
  // |factory| will create implementations of Interface on demand.
  // Returns true if the interface was exposed, false if capability filtering
  // from the shell prevented the interface from being exposed.
  template <typename Interface>
  bool AddInterface(InterfaceFactory<Interface>* factory) {
    return GetInterfaceRegistry()->AddInterface<Interface>(factory);
  }
  template <typename Interface>
  bool AddInterface(
      const base::Callback<void(mojo::InterfaceRequest<Interface>)>& callback,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
    return GetInterfaceRegistry()->AddInterface<Interface>(
        callback, task_runner);
  }

  // Binds |ptr| to an implementation of Interface in the remote application.
  // |ptr| can immediately be used to start sending requests to the remote
  // interface.
  template <typename Interface>
  void GetInterface(mojo::InterfacePtr<Interface>* ptr) {
    GetRemoteInterfaces()->GetInterface(ptr);
  }

  // Returns true if the remote application has the specified capability class
  // specified in its manifest. Only valid for inbound connections. Will return
  // false for outbound connections.
  virtual bool HasCapabilityClass(const std::string& class_name) const = 0;

  // Returns the name that was used by the source application to establish a
  // connection to the destination application.
  //
  // When Connection is representing and outgoing connection, this will be the
  // same as the value returned by GetRemoveApplicationName().
  virtual const std::string& GetConnectionName() = 0;

  // Returns the remote identity. While the connection is in the pending state,
  // the user_id() field will be the value passed via Connect(). After the
  // connection is completed, it will change to the value assigned by the shell.
  // Call AddConnectionCompletedClosure() to schedule a closure to be run when
  // the resolved user id is available.
  virtual const Identity& GetRemoteIdentity() const = 0;

  // Register a handler to receive an error notification on the pipe to the
  // remote application's InterfaceProvider.
  virtual void SetConnectionLostClosure(const base::Closure& handler) = 0;

  // Returns the result of the connection. This function should only be called
  // when the connection state is not pending. Call
  // AddConnectionCompletedClosure() to schedule a closure to be run when the
  // connection is processed by the shell.
  virtual mojom::ConnectResult GetResult() const = 0;

  // Returns true if the connection has not yet been processed by the shell.
  virtual bool IsPending() const = 0;

  // Returns the instance id of the remote application if it is known at the
  // time this function is called. When IsPending() returns true, this function
  // will return mojom::kInvalidInstanceID. Use
  // AddConnectionCompletedClosure() to schedule a closure to be run when the
  // connection is processed by the shell and remote id is available.
  virtual uint32_t GetRemoteInstanceID() const = 0;

  // Register a closure to be run when the connection has been completed by the
  // shell and remote metadata is available. Useful only for connections created
  // via Connector::Connect(). Once the connection is complete, metadata is
  // available immediately.
  virtual void AddConnectionCompletedClosure(const base::Closure& callback) = 0;

  // Returns true if the Shell allows |interface_name| to be exposed to the
  // remote application.
  virtual bool AllowsInterface(const std::string& interface_name) const = 0;

  // Returns a raw pointer to the InterfaceProvider at the remote end.
  virtual mojom::InterfaceProvider* GetRemoteInterfaceProvider() = 0;

  // Returns the InterfaceRegistry that implements the mojom::InterfaceProvider
  // exposed to the remote application.
  virtual InterfaceRegistry* GetInterfaceRegistry() = 0;

  // Returns an object encapsulating a remote InterfaceProvider.
  virtual InterfaceProvider* GetRemoteInterfaces() = 0;

 protected:
  virtual base::WeakPtr<Connection> GetWeakPtr() = 0;
};

}  // namespace shell

#endif  // SERVICES_SHELL_PUBLIC_CPP_CONNECTION_H_
