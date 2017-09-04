// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_CONNECTOR_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_CONNECTOR_H_

#include <memory>

#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/interfaces/connector.mojom.h"
#include "services/service_manager/public/interfaces/service.mojom.h"
#include "services/service_manager/public/interfaces/service_manager.mojom.h"

namespace service_manager {

// An interface that encapsulates the Service Manager's brokering interface, by
// which
// connections between services are established. Once Connect() is called,
// this class is bound to the thread the call was made on and it cannot be
// passed to another thread without calling Clone().
//
// An instance of this class is created internally by ServiceContext for use
// on the thread ServiceContext is instantiated on.
//
// To use this interface on another thread, call Clone() and pass the new
// instance to the desired thread before calling Connect().
//
// While instances of this object are owned by the caller, the underlying
// connection with the service manager is bound to the lifetime of the instance
// that
// created it, i.e. when the application is terminated the Connector pipe is
// closed.
class Connector {
 public:
  virtual ~Connector() {}

  class ConnectParams {
   public:
    explicit ConnectParams(const Identity& target);
    explicit ConnectParams(const std::string& name);
    ~ConnectParams();

    const Identity& target() { return target_; }
    void set_target(const Identity& target) { target_ = target; }
    void set_client_process_connection(
        mojom::ServicePtr service,
        mojom::PIDReceiverRequest pid_receiver_request) {
      service_ = std::move(service);
      pid_receiver_request_ = std::move(pid_receiver_request);
    }
    void TakeClientProcessConnection(
        mojom::ServicePtr* service,
        mojom::PIDReceiverRequest* pid_receiver_request) {
      *service = std::move(service_);
      *pid_receiver_request = std::move(pid_receiver_request_);
    }
    InterfaceProvider* remote_interfaces() { return remote_interfaces_; }
    void set_remote_interfaces(InterfaceProvider* remote_interfaces) {
      remote_interfaces_ = remote_interfaces;
    }

   private:
    Identity target_;
    mojom::ServicePtr service_;
    mojom::PIDReceiverRequest pid_receiver_request_;
    InterfaceProvider* remote_interfaces_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(ConnectParams);
  };

  // Creates a new Connector instance and fills in |*request| with a request
  // for the other end the Connector's interface.
  static std::unique_ptr<Connector> Create(mojom::ConnectorRequest* request);

  // Requests a new connection to an application. Returns a pointer to the
  // connection if the connection is permitted by this application's delegate,
  // or nullptr otherwise. Caller takes ownership.
  // Once this method is called, this object is bound to the thread on which the
  // call took place. To pass to another thread, call Clone() and pass the
  // result.
  virtual std::unique_ptr<Connection> Connect(const std::string& name) = 0;
  virtual std::unique_ptr<Connection> Connect(ConnectParams* params) = 0;

  // Connect to application identified by |request->name| and connect to the
  // service implementation of the interface identified by |Interface|.
  template <typename Interface>
  void ConnectToInterface(ConnectParams* params,
                          mojo::InterfacePtr<Interface>* ptr) {
    std::unique_ptr<Connection> connection = Connect(params);
    if (connection)
      connection->GetInterface(ptr);
  }
  template <typename Interface>
  void ConnectToInterface(const Identity& target,
                          mojo::InterfacePtr<Interface>* ptr) {
    ConnectParams params(target);
    return ConnectToInterface(&params, ptr);
  }
  template <typename Interface>
  void ConnectToInterface(const std::string& name,
                          mojo::InterfacePtr<Interface>* ptr) {
    ConnectParams params(name);
    return ConnectToInterface(&params, ptr);
  }

  // Creates a new instance of this class which may be passed to another thread.
  // The returned object may be passed multiple times until Connect() is called,
  // at which point this method must be called again to pass again.
  virtual std::unique_ptr<Connector> Clone() = 0;

  // Binds a Connector request to the other end of this Connector.
  virtual void BindRequest(mojom::ConnectorRequest request) = 0;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_CONNECTOR_H_
