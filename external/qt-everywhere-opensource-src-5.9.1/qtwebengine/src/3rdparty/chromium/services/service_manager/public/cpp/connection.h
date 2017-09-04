// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_CONNECTION_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_CONNECTION_H_

#include "base/memory/weak_ptr.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/interfaces/connector.mojom.h"

namespace service_manager {

class InterfaceProvider;

// Represents a connection to another application. An implementation of this
// interface is returned from Connector::Connect().
class Connection {
 public:
  virtual ~Connection() {}

  enum class State {
    // The service manager has not yet processed the connection.
    PENDING,

    // The service manager processed the connection and it was established.
    // GetResult() returns mojom::ConnectionResult::SUCCESS.
    CONNECTED,

    // The service manager processed the connection and establishment was
    // prevented by an error, call GetResult().
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

  // Binds |ptr| to an implementation of Interface in the remote application.
  // |ptr| can immediately be used to start sending requests to the remote
  // interface.
  template <typename Interface>
  void GetInterface(mojo::InterfacePtr<Interface>* ptr) {
    GetRemoteInterfaces()->GetInterface(ptr);
  }
  template <typename Interface>
  void GetInterface(mojo::InterfaceRequest<Interface> request) {
    GetRemoteInterfaces()->GetInterface(std::move(request));
  }

  // Returns the remote identity. While the connection is in the pending state,
  // the user_id() field will be the value passed via Connect(). After the
  // connection is completed, it will change to the value assigned by the
  // service manager. Call AddConnectionCompletedClosure() to schedule a closure
  // to be run when the resolved user id is available.
  virtual const Identity& GetRemoteIdentity() const = 0;

  // Register a handler to receive an error notification on the pipe to the
  // remote application's InterfaceProvider.
  virtual void SetConnectionLostClosure(const base::Closure& handler) = 0;

  // Returns the result of the connection. This function should only be called
  // when the connection state is not pending. Call
  // AddConnectionCompletedClosure() to schedule a closure to be run when the
  // connection is processed by the service manager.
  virtual mojom::ConnectResult GetResult() const = 0;

  // Returns true if the connection has not yet been processed by the service
  // manager.
  virtual bool IsPending() const = 0;

  // Register a closure to be run when the connection has been completed by the
  // service manager and remote metadata is available. Useful only for
  // connections created
  // via Connector::Connect(). Once the connection is complete, metadata is
  // available immediately.
  virtual void AddConnectionCompletedClosure(const base::Closure& callback) = 0;

  // Returns an object encapsulating a remote InterfaceProvider.
  virtual InterfaceProvider* GetRemoteInterfaces() = 0;

 protected:
  virtual base::WeakPtr<Connection> GetWeakPtr() = 0;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_CONNECTION_H_
