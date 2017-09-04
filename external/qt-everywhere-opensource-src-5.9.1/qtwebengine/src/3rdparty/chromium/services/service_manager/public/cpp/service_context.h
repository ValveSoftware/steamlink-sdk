// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_CONTEXT_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_CONTEXT_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/core.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/interfaces/connector.mojom.h"
#include "services/service_manager/public/interfaces/service.mojom.h"
#include "services/service_manager/public/interfaces/service_control.mojom.h"

namespace service_manager {

// Encapsulates a connection to the Service Manager in two parts:
// - a bound InterfacePtr to mojom::Connector, the primary mechanism
//   by which the instantiating service connects to other services,
//   brokered by the Service Manager.
// - a bound InterfaceRequest of mojom::Service, an interface used by the
//   Service Manager to inform this service of lifecycle events and
//   inbound connections brokered by it.
//
// This class owns an instance of a Service implementation, and there should be
// exactly one instance of this class for every logical service instance running
// in the system.
//
// This class is generally used to handle incoming mojom::ServiceRequests from
// the Service Manager. These can either come from ServiceRunner, from the
// command-line (in the form of a pipe token), from a mojom::ServiceFactory
// call, or from some other embedded service-running facility defined by the
// client.
class ServiceContext : public mojom::Service {
 public:
  // Creates a new ServiceContext bound to |request|. This connection may be
  // used immediately to make outgoing connections via connector().
  //
  // This ServiceContext routes all incoming mojom::Service messages and
  // error signals to |service|, which it owns.
  //
  // If either |connector| or |connector_request| is non-null both must be
  // non-null. If both are null, the context will create its own
  // Connector and request to pass to the Service Manager on initialization.
  //
  // TODO(rockot): Clean up the connector/connector_request junk.
  ServiceContext(std::unique_ptr<service_manager::Service> service,
                 mojom::ServiceRequest request,
                 std::unique_ptr<Connector> connector = nullptr,
                 mojom::ConnectorRequest connector_request = nullptr);

  ~ServiceContext() override;

  Connector* connector() { return connector_.get(); }
  const ServiceInfo& local_info() const { return local_info_; }
  const Identity& identity() const { return local_info_.identity; }

  // Specify a function to be called when the connection to the service manager
  // is lost. Note that if connection has already been lost, then |closure| is
  // called immediately.
  //
  // It is acceptable for |closure| to delete this ServiceContext.
  void SetConnectionLostClosure(const base::Closure& closure);

  // Informs the Service Manager that this instance is ready to terminate. If
  // the Service Manager has any outstanding connection requests for this
  // instance, the request is ignored; the instance will eventually receive
  // the pending request(s) and can then appropriately decide whether or not
  // it still wants to quit.
  //
  // If the request is granted, the Service Manager will service the connection
  // to this ServiceContext and Service::OnStop() will eventually be invoked.
  void RequestQuit();

  // Immediately severs the connection to the Service Manager.
  //
  // Note that calling this before the Service receives OnStop() can lead to
  // unpredictable behavior, specifically because clients may have inbound
  // connections in transit which may have already been brokered by the Service
  // Manager and thus will be irreparably broken on the client side.
  //
  // Use of this call before OnStop() should be reserved for exceptional cases.
  void DisconnectFromServiceManager();

  // Immediately severs the connection to the Service Manager.
  //
  // If a connection-lost closure was set, it is immediately invoked. Note that
  // it is never necessary or meaningful to call this after the Service
  // has received OnStop().
  //
  // See comments on DisconnectFromServiceManager() regarding abrupt
  // disconnection from the Service Manager.
  void QuitNow();

  // Simliar to QuitNow() above but also destroys the Service instance.
  void DestroyService();

 private:
  using InterfaceRegistryMap =
      std::map<InterfaceRegistry*, std::unique_ptr<InterfaceRegistry>>;

  // mojom::Service:
  void OnStart(const ServiceInfo& info,
               const OnStartCallback& callback) override;
  void OnConnect(const ServiceInfo& source_info,
                 mojom::InterfaceProviderRequest interfaces,
                 const OnConnectCallback& callback) override;

  void OnConnectionError();
  void OnRegistryConnectionError(InterfaceRegistry* registry);
  void DestroyConnectionInterfaceRegistry(InterfaceRegistry* registry);

  // We track the lifetime of incoming connection registries as a convenience
  // for the client.
  InterfaceRegistryMap connection_interface_registries_;

  // A pending Connector request which will eventually be passed to the Service
  // Manager.
  mojom::ConnectorRequest pending_connector_request_;

  std::unique_ptr<service_manager::Service> service_;
  mojo::Binding<mojom::Service> binding_;
  std::unique_ptr<Connector> connector_;
  service_manager::ServiceInfo local_info_;

  // This instance's control interface to the service manager. Note that this
  // is unbound and therefore invalid until OnStart() is called.
  mojom::ServiceControlAssociatedPtr service_control_;

  bool service_quit_ = false;

  base::Closure connection_lost_closure_;

  base::WeakPtrFactory<ServiceContext> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceContext);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_CONTEXT_H_
