// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SERVICE_MANAGER_CONNECTION_H_
#define CONTENT_PUBLIC_COMMON_SERVICE_MANAGER_CONNECTION_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/sequenced_task_runner.h"
#include "content/common/content_export.h"
#include "content/public/common/service_info.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/interfaces/service.mojom.h"

namespace service_manager {
class Connection;
class Connector;
class InterfaceProvider;
class InterfaceRegistry;
}

namespace content {

class ConnectionFilter;

// Encapsulates a connection to a //services/service_manager.
// Access a global instance on the thread the ServiceContext was bound by
// calling Holder::Get().
// Clients can add service_manager::Service implementations whose exposed
// interfaces
// will be exposed to inbound connections to this object's Service.
// Alternatively clients can define named services that will be constructed when
// requests for those service names are received.
// Clients must call any of the registration methods when receiving
// ContentBrowserClient::RegisterInProcessServices().
class CONTENT_EXPORT ServiceManagerConnection {
 public:
  using ServiceRequestHandler =
      base::Callback<void(service_manager::mojom::ServiceRequest)>;
  using OnConnectHandler =
      base::Callback<void(const service_manager::ServiceInfo&,
                          const service_manager::ServiceInfo&)>;
  using Factory =
      base::Callback<std::unique_ptr<ServiceManagerConnection>(void)>;

  // Stores an instance of |connection| in TLS for the current process. Must be
  // called on the thread the connection was created on.
  static void SetForProcess(
      std::unique_ptr<ServiceManagerConnection> connection);

  // Returns the per-process instance, or nullptr if the Service Manager
  // connection has not yet been bound. Must be called on the thread the
  // connection was created on.
  static ServiceManagerConnection* GetForProcess();

  // Destroys the per-process instance. Must be called on the thread the
  // connection was created on.
  static void DestroyForProcess();

  virtual ~ServiceManagerConnection();

  // Sets the factory used to create the ServiceManagerConnection. This must be
  // called before the ServiceManagerConnection has been created.
  static void SetFactoryForTest(Factory* factory);

  // Creates a ServiceManagerConnection from |request|. The connection binds
  // its interfaces and accept new connections on |io_task_runner| only. Note
  // that no incoming connections are accepted until Start() is called.
  static std::unique_ptr<ServiceManagerConnection> Create(
      service_manager::mojom::ServiceRequest request,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);

  // Begins accepting incoming connections. Connection filters MUST be added
  // before calling this in order to avoid races. See AddConnectionFilter()
  // below.
  virtual void Start() = 0;

  // Sets a closure to be invoked once the connection receives an Initialize()
  // request from the shell.
  virtual void SetInitializeHandler(const base::Closure& handler) = 0;

  // Returns the service_manager::Connector received via this connection's
  // Service
  // implementation. Use this to initiate connections as this object's Identity.
  virtual service_manager::Connector* GetConnector() = 0;

  // Returns this connection's identity with the Service Manager. Connections
  // initiated via the service_manager::Connector returned by GetConnector()
  // will use
  // this.
  virtual const service_manager::Identity& GetIdentity() const = 0;

  // Sets a closure that is called when the connection is lost. Note that
  // connection may already have been closed, in which case |closure| will be
  // run immediately before returning from this function.
  virtual void SetConnectionLostClosure(const base::Closure& closure) = 0;

  // Provides an InterfaceRegistry to forward incoming interface requests to
  // on the ServiceManagerConnection's own thread if they aren't bound by the
  // connection's internal InterfaceRegistry on the IO thread.
  //
  // Also configures |interface_provider| to forward all of its outgoing
  // interface requests to the connection's internal remote interface provider.
  //
  // Note that neither |interface_registry| or |interface_provider| is owned
  // and both MUST outlive the ServiceManagerConnection.
  //
  // TODO(rockot): Remove this. It's a temporary solution to avoid porting all
  // relevant code to ConnectionFilters at once.
  virtual void SetupInterfaceRequestProxies(
      service_manager::InterfaceRegistry* registry,
      service_manager::InterfaceProvider* provider) = 0;

  static const int kInvalidConnectionFilterId = 0;

  // Allows the caller to filter inbound connections and/or expose interfaces
  // on them. |filter| may be created on any thread, but will be used and
  // destroyed exclusively on the IO thread (the thread corresponding to
  // |io_task_runner| passed to Create() above.)
  //
  // Connection filters MUST be added before calling Start() in order to avoid
  // races.
  //
  // Returns a unique identifier that can be passed to RemoveConnectionFilter()
  // below.
  virtual int AddConnectionFilter(
      std::unique_ptr<ConnectionFilter> filter) = 0;

  // Removes a filter using the id value returned by AddConnectionFilter().
  // Removal (and destruction) happens asynchronously on the IO thread.
  virtual void RemoveConnectionFilter(int filter_id) = 0;

  // Adds an embedded service to this connection's ServiceFactory.
  // |info| provides details on how to construct new instances of the
  // service when an incoming connection is made to |name|.
  virtual void AddEmbeddedService(const std::string& name,
                                  const ServiceInfo& info) = 0;

  // Adds a generic ServiceRequestHandler for a given service name. This
  // will be used to satisfy any incoming calls to CreateService() which
  // reference the given name.
  //
  // For in-process services, it is preferable to use |AddEmbeddedService()| as
  // defined above.
  virtual void AddServiceRequestHandler(
      const std::string& name,
      const ServiceRequestHandler& handler) = 0;

  // Registers a callback to be run when the service_manager::Service
  // implementation on the IO thread receives OnConnect(). Returns an id that
  // can be passed to RemoveOnConnectHandler(), starting at 1.
  virtual int AddOnConnectHandler(const OnConnectHandler& handler) = 0;
  virtual void RemoveOnConnectHandler(int id) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SERVICE_MANAGER_CONNECTION_H_
