// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_SERVICE_MANAGER_H_
#define SERVICES_SERVICE_MANAGER_SERVICE_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/service_manager/connect_params.h"
#include "services/service_manager/native_runner.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/interface_provider_spec.h"
#include "services/service_manager/public/interfaces/connector.mojom.h"
#include "services/service_manager/public/interfaces/interface_provider.mojom.h"
#include "services/service_manager/public/interfaces/resolver.mojom.h"
#include "services/service_manager/public/interfaces/service.mojom.h"
#include "services/service_manager/public/interfaces/service_factory.mojom.h"
#include "services/service_manager/public/interfaces/service_manager.mojom.h"
#include "services/service_manager/service_overrides.h"

namespace service_manager {

class ServiceContext;

// Creates an identity for the Service Manager, used when the Service Manager
// connects to services.
Identity CreateServiceManagerIdentity();

class ServiceManager {
 public:
  // API for testing.
  class TestAPI {
   public:
    explicit TestAPI(ServiceManager* service_manager);
    ~TestAPI();

    // Returns true if there is a Instance for this name.
    bool HasRunningInstanceForName(const std::string& name) const;
   private:
    ServiceManager* service_manager_;

    DISALLOW_COPY_AND_ASSIGN(TestAPI);
  };

  // |native_runner_factory| is an instance of an object capable of vending
  // implementations of NativeRunner, e.g. for in or out-of-process execution.
  // See native_runner.h and RunNativeApplication().
  // |file_task_runner| provides access to a thread to perform file copy
  // operations on.
  ServiceManager(std::unique_ptr<NativeRunnerFactory> native_runner_factory,
                 mojom::ServicePtr catalog);
  ~ServiceManager();

  // Sets overrides for service executable and package resolution. Must be
  // called before any services are launched.
  void SetServiceOverrides(std::unique_ptr<ServiceOverrides> overrides);

  // Provide a callback to be notified whenever an instance is destroyed.
  // Typically the creator of the Service Manager will use this to determine
  // when some set of services it created are destroyed, so it can shut down.
  void SetInstanceQuitCallback(base::Callback<void(const Identity&)> callback);

  // Completes a connection between a source and target application as defined
  // by |params|, exchanging InterfaceProviders between them. If no existing
  // instance of the target application is running, one will be loaded.
  void Connect(std::unique_ptr<ConnectParams> params);

  // Creates a new Instance identified as |name|. This is intended for use by
  // the Service Manager's embedder to register itself. This must only be called
  // once.
  mojom::ServiceRequest StartEmbedderService(const std::string& name);

 private:
  class Instance;
  class ServiceImpl;

  void InitCatalog(mojom::ServicePtr catalog);

  // Returns the resolver to use for the specified identity.
  // NOTE: Resolvers are cached to ensure we service requests in order. If
  // we use a separate Resolver for each request ordering is not
  // guaranteed and can lead to random flake.
  mojom::Resolver* GetResolver(const Identity& identity);

  // Called when |instance| encounters an error. Deletes |instance|.
  void OnInstanceError(Instance* instance);

  // Called when |instance| becomes unreachable to new connections because it
  // no longer has any pipes to the ServiceManager.
  void OnInstanceUnreachable(Instance* instance);

  // Called by an Instance as it's being destroyed.
  void OnInstanceStopped(const Identity& identity);

  // Completes a connection between a source and target application as defined
  // by |params|, exchanging InterfaceProviders between them. If no existing
  // instance of the target application is running, one will be loaded.
  //
  // If |service| is not null, there must not be an instance of the target
  // application already running. The Service Manager will create a new instance
  // and use |service| to control it.
  //
  // If |instance| is not null, the lifetime of the connection request is
  // bounded by that of |instance|. The connection will be cancelled dropped if
  // |instance| is destroyed.
  void Connect(std::unique_ptr<ConnectParams> params,
               mojom::ServicePtr service,
               base::WeakPtr<Instance> source_instance);

  // Returns a running instance matching |identity|. This might be an instance
  // running as a different user if one is available that services all users.
  Instance* GetExistingInstance(const Identity& identity) const;

  void NotifyServiceStarted(const Identity& identity, base::ProcessId pid);
  void NotifyServiceFailedToStart(const Identity& identity);

  // Attempt to complete the connection requested by |params| by connecting to
  // an existing instance. If there is an existing instance, |params| is taken,
  // and this function returns true.
  bool ConnectToExistingInstance(std::unique_ptr<ConnectParams>* params);

  Instance* CreateInstance(const Identity& source,
                           const Identity& target,
                           const InterfaceProviderSpecMap& specs);

  // Called from the instance implementing mojom::ServiceManager.
  void AddListener(mojom::ServiceManagerListenerPtr listener);

  void CreateServiceWithFactory(const Identity& service_factory,
                                const std::string& name,
                                mojom::ServiceRequest request);
  // Returns a running ServiceFactory for |service_factory_identity|.
  // If there is not one running one is started for |source_identity|.
  mojom::ServiceFactory* GetServiceFactory(
      const Identity& service_factory_identity);
  void OnServiceFactoryLost(const Identity& which);

  // Callback when remote Catalog resolves mojo:foo to mojo:bar.
  // |params| are the params passed to Connect().
  // |service| if provided is a ServicePtr which should be used to manage the
  // new application instance. This may be null.
  // |result| contains the result of the resolve operation.
  void OnGotResolvedName(std::unique_ptr<ConnectParams> params,
                         mojom::ServicePtr service,
                         bool has_source_instance,
                         base::WeakPtr<Instance> source_instance,
                         mojom::ResolveResultPtr result);

  base::WeakPtr<ServiceManager> GetWeakPtr();

  std::unique_ptr<ServiceOverrides> service_overrides_;

  // Ownership of all root Instances. Non-root Instances are owned by their
  // parent Instance.
  using InstanceMap = std::map<Instance*, std::unique_ptr<Instance>>;
  InstanceMap root_instances_;

  // Maps service identities to reachable instances. Note that the Instance*
  // values here are NOT owned by this map.
  std::map<Identity, Instance*> identity_to_instance_;

  // Always points to the ServiceManager's own Instance. Note that this
  // Instance still has an entry in |root_instances_|.
  Instance* service_manager_instance_;

  // Tracks the names of instances that are allowed to field connection requests
  // from all users.
  std::set<std::string> singletons_;

  std::map<Identity, mojom::ServiceFactoryPtr> service_factories_;
  std::map<Identity, mojom::ResolverPtr> identity_to_resolver_;
  mojo::InterfacePtrSet<mojom::ServiceManagerListener> listeners_;
  base::Callback<void(const Identity&)> instance_quit_callback_;
  std::unique_ptr<NativeRunnerFactory> native_runner_factory_;
  std::unique_ptr<ServiceContext> service_context_;
  base::WeakPtrFactory<ServiceManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceManager);
};

mojom::Connector::ConnectCallback EmptyConnectCallback();

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_SERVICE_MANAGER_H_
