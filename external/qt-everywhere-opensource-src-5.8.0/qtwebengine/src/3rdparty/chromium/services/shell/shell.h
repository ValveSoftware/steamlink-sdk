// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_SHELL_H_
#define SERVICES_SHELL_SHELL_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/shell/connect_params.h"
#include "services/shell/native_runner.h"
#include "services/shell/public/cpp/capabilities.h"
#include "services/shell/public/cpp/identity.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/interfaces/connector.mojom.h"
#include "services/shell/public/interfaces/interface_provider.mojom.h"
#include "services/shell/public/interfaces/shell.mojom.h"
#include "services/shell/public/interfaces/shell_client.mojom.h"
#include "services/shell/public/interfaces/shell_client_factory.mojom.h"
#include "services/shell/public/interfaces/shell_resolver.mojom.h"

namespace shell {
class ShellConnection;

// Creates an identity for the Shell, used when the Shell connects to
// applications.
Identity CreateShellIdentity();

class Shell : public ShellClient {
 public:
  // API for testing.
  class TestAPI {
   public:
    explicit TestAPI(Shell* shell);
    ~TestAPI();

    // Returns true if there is a Instance for this name.
    bool HasRunningInstanceForName(const std::string& name) const;
   private:
    Shell* shell_;

    DISALLOW_COPY_AND_ASSIGN(TestAPI);
  };

  // |native_runner_factory| is an instance of an object capable of vending
  // implementations of NativeRunner, e.g. for in or out-of-process execution.
  // See native_runner.h and RunNativeApplication().
  // |file_task_runner| provides access to a thread to perform file copy
  // operations on.
  Shell(std::unique_ptr<NativeRunnerFactory> native_runner_factory,
        mojom::ShellClientPtr catalog);
  ~Shell() override;

  // Provide a callback to be notified whenever an instance is destroyed.
  // Typically the creator of the Shell will use this to determine when some set
  // of instances it created are destroyed, so it can shut down.
  void SetInstanceQuitCallback(base::Callback<void(const Identity&)> callback);

  // Completes a connection between a source and target application as defined
  // by |params|, exchanging InterfaceProviders between them. If no existing
  // instance of the target application is running, one will be loaded.
  void Connect(std::unique_ptr<ConnectParams> params);

  // Creates a new Instance identified as |name|. This is intended for use by
  // the Shell's embedder to register itself with the shell. This must only be
  // called once.
  mojom::ShellClientRequest InitInstanceForEmbedder(const std::string& name);

 private:
  class Instance;

  // ShellClient:
  bool AcceptConnection(Connection* connection) override;

  void InitCatalog(mojom::ShellClientPtr catalog);

  // Returns the resolver to use for the specified identity.
  // NOTE: ShellResolvers are cached to ensure we service requests in order. If
  // we use a separate ShellResolver for each request ordering is not
  // guaranteed and can lead to random flake.
  mojom::ShellResolver* GetResolver(const Identity& identity);

  // Destroys all Shell-ends of connections established with Applications.
  // Applications connected by this Shell will observe pipe errors and have a
  // chance to shutdown.
  void TerminateShellConnections();

  // Removes a Instance when it encounters an error.
  void OnInstanceError(Instance* instance);

  // Completes a connection between a source and target application as defined
  // by |params|, exchanging InterfaceProviders between them. If no existing
  // instance of the target application is running, one will be loaded.
  //
  // If |client| is not null, there must not be an instance of the target
  // application already running. The shell will create a new instance and use
  // |client| to control it.
  void Connect(std::unique_ptr<ConnectParams> params,
               mojom::ShellClientPtr client);

  // Returns a running instance matching |identity|. This might be an instance
  // running as a different user if one is available that services all users.
  Instance* GetExistingInstance(const Identity& identity) const;

  void NotifyPIDAvailable(uint32_t id, base::ProcessId pid);

  // Attempt to complete the connection requested by |params| by connecting to
  // an existing instance. If there is an existing instance, |params| is taken,
  // and this function returns true.
  bool ConnectToExistingInstance(std::unique_ptr<ConnectParams>* params);

  Instance* CreateInstance(const Identity& source,
                           const Identity& target,
                           const CapabilitySpec& spec);

  // Called from the instance implementing mojom::Shell.
  void AddInstanceListener(mojom::InstanceListenerPtr listener);

  void CreateShellClientWithFactory(const Identity& shell_client_factory,
                                    const std::string& name,
                                    mojom::ShellClientRequest request);
  // Returns a running ShellClientFactory for |shell_client_factory_identity|.
  // If there is not one running one is started for |source_identity|.
  mojom::ShellClientFactory* GetShellClientFactory(
      const Identity& shell_client_factory_identity);
  void OnShellClientFactoryLost(const Identity& which);

  // Callback when remote Catalog resolves mojo:foo to mojo:bar.
  // |params| are the params passed to Connect().
  // |client| if provided is a ShellClientPtr which should be used to manage the
  // new application instance. This may be null.
  // |result| contains the result of the resolve operation.
  void OnGotResolvedName(std::unique_ptr<ConnectParams> params,
                         mojom::ShellClientPtr client,
                         mojom::ResolveResultPtr result);

  base::WeakPtr<Shell> GetWeakPtr();

  std::map<Identity, Instance*> identity_to_instance_;

  // Tracks the names of instances that are allowed to field connection requests
  // from all users.
  std::set<std::string> singletons_;

  std::map<Identity, mojom::ShellClientFactoryPtr> shell_client_factories_;
  std::map<Identity, mojom::ShellResolverPtr> identity_to_resolver_;
  mojo::InterfacePtrSet<mojom::InstanceListener> instance_listeners_;
  base::Callback<void(const Identity&)> instance_quit_callback_;
  std::unique_ptr<NativeRunnerFactory> native_runner_factory_;
  std::unique_ptr<ShellConnection> shell_connection_;
  base::WeakPtrFactory<Shell> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Shell);
};

mojom::Connector::ConnectCallback EmptyConnectCallback();

}  // namespace shell

#endif  // SERVICES_SHELL_SHELL_H_
