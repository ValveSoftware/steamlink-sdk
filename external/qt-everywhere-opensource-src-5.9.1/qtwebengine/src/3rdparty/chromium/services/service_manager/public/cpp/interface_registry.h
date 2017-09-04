// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_REGISTRY_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_REGISTRY_H_

#include <list>
#include <memory>
#include <queue>
#include <set>
#include <utility>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/interface_provider_spec.h"
#include "services/service_manager/public/cpp/lib/callback_binder.h"
#include "services/service_manager/public/cpp/lib/interface_factory_binder.h"
#include "services/service_manager/public/interfaces/interface_provider.mojom.h"

namespace service_manager {
class Connection;
class InterfaceBinder;

// An implementation of mojom::InterfaceProvider that allows the user to
// register services to be exposed to another application.
//
// To use, define a class that implements your specific interface. Then
// implement an InterfaceFactory<Foo> that binds instances of FooImpl to
// InterfaceRequest<Foo>s and register that on the registry like this:
//
//   registry.AddInterface(&factory);
//
// Or, if you have multiple factories implemented by the same type, explicitly
// specify the interface to register the factory for:
//
//   registry.AddInterface<Foo>(&my_foo_and_bar_factory_);
//   registry.AddInterface<Bar>(&my_foo_and_bar_factory_);
//
// The InterfaceFactory must outlive the InterfaceRegistry.
//
// Additionally you may specify a default InterfaceBinder to handle requests for
// interfaces unhandled by any registered InterfaceFactory. Just as with
// InterfaceFactory, the default InterfaceBinder supplied must outlive
// InterfaceRegistry.
//
class InterfaceRegistry : public mojom::InterfaceProvider {
 public:
  using Binder = base::Callback<void(const std::string&,
                                     mojo::ScopedMessagePipeHandle)>;

  class TestApi {
   public:
    explicit TestApi(InterfaceRegistry* registry) : registry_(registry) {}
    ~TestApi() {}

    void SetInterfaceBinderForName(InterfaceBinder* binder,
                                   const std::string& interface_name) {
      registry_->SetInterfaceBinderForName(
          base::WrapUnique(binder), interface_name);
    }

    template <typename Interface>
    void GetLocalInterface(mojo::InterfaceRequest<Interface> request) {
      GetLocalInterface(Interface::Name_, request.PassMessagePipe());
    }

    void GetLocalInterface(const std::string& name,
                           mojo::ScopedMessagePipeHandle handle) {
      registry_->GetInterface(name, std::move(handle));
    }

   private:
    InterfaceRegistry* registry_;
    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  // Construct an unbound InterfaceRegistry. This object will not bind requests
  // for interfaces until Bind() is called. |name| is used for error reporting
  // and should reflect the name of the InterfaceProviderSpec pair that controls
  // which interfaces can be bound via this InterfaceRegistry.
  explicit InterfaceRegistry(const std::string& name);
  ~InterfaceRegistry() override;

  // Sets a default handler for incoming interface requests which are allowed by
  // capability filters but have no registered handler in this registry.
  void set_default_binder(const Binder& binder) { default_binder_ = binder; }

  // Binds a request for an InterfaceProvider from a remote source.
  // |remote_info| contains the the identity of the remote, and the remote's
  // InterfaceProviderSpec, which will be intersected with the local's exports
  // to determine what interfaces may be bound.
  void Bind(mojom::InterfaceProviderRequest request,
            const Identity& local_identity,
            const InterfaceProviderSpec& local_interface_provider_spec,
            const Identity& remote_identity,
            const InterfaceProviderSpec& remote_interface_provider_spec);

  // Serializes the contents of the registry (including the local and remote
  // specs) to a stringstream.
  void Serialize(std::stringstream* stream);

  base::WeakPtr<InterfaceRegistry> GetWeakPtr();

  // Allows |Interface| to be exposed via this registry. Requests to bind will
  // be handled by |factory|. Returns true if the interface was exposed, false
  // if Connection policy prevented exposure.
  template <typename Interface>
  bool AddInterface(InterfaceFactory<Interface>* factory) {
    return SetInterfaceBinderForName(
        base::MakeUnique<internal::InterfaceFactoryBinder<Interface>>(factory),
        Interface::Name_);
  }

  // Like AddInterface above, except supplies a callback to bind the MP instead
  // of an InterfaceFactory, and optionally provides a task runner where the
  // callback will be run.
  template <typename Interface>
  bool AddInterface(
      const base::Callback<void(mojo::InterfaceRequest<Interface>)>& callback,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner =
          nullptr) {
    return SetInterfaceBinderForName(
        base::MakeUnique<internal::CallbackBinder<Interface>>(callback,
                                                              task_runner),
        Interface::Name_);
  }
  bool AddInterface(
      const std::string& name,
      const base::Callback<void(mojo::ScopedMessagePipeHandle)>& callback,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner =
          nullptr);

  template <typename Interface>
  void RemoveInterface() {
    RemoveInterface(Interface::Name_);
  }
  void RemoveInterface(const std::string& name);

  // Temporarily prevent incoming interface requests from being bound. Incoming
  // requests will be queued internally and dispatched once ResumeBinding() is
  // called.
  void PauseBinding();

  // Resumes incoming interface request binding.
  void ResumeBinding();

  // Populates a set with the interface names this registry can bind.
  void GetInterfaceNames(std::set<std::string>* interface_names);

  // Sets a closure to be run when the InterfaceProvider pipe is closed. Note
  // that by the time any added closure is invoked, the InterfaceRegistry may
  // have been deleted.
  void AddConnectionLostClosure(const base::Closure& connection_lost_closure);

 private:
  using InterfaceNameToBinderMap =
      std::map<std::string, std::unique_ptr<InterfaceBinder>>;

  // mojom::InterfaceProvider:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle handle) override;

  // Returns true if the binder was set, false if it was not set (e.g. by
  // some filtering policy preventing this interface from being exposed).
  bool SetInterfaceBinderForName(std::unique_ptr<InterfaceBinder> binder,
                                 const std::string& name);

  // Returns true if |remote_identity_| is allowed to bind |interface_name|,
  // according to capability policy.
  bool CanBindRequestForInterface(const std::string& interface_name) const;

  // Called whenever |remote_interface_provider_spec_| changes to rebuild the
  // contents of |exposed_interfaces_| and |expose_all_interfaces_|.
  void RebuildExposedInterfaces();

  void OnConnectionError();

  mojom::InterfaceProviderRequest pending_request_;

  mojo::Binding<mojom::InterfaceProvider> binding_;

  std::string name_;

  // Initialized from static metadata in the host service's manifest.
  Identity local_identity_;
  InterfaceProviderSpec local_interface_provider_spec_;

  // Initialized from static metadata in the remote service's manifest.
  Identity remote_identity_;
  // Initialized from static metadata in the remote service's manifest. May be
  // mutated after the fact when a capability is dynamically granted via a call
  // to GrantCapability().
  InterfaceProviderSpec remote_interface_provider_spec_;

  // Metadata computed whenever |remote_interface_provider_spec_| changes.
  InterfaceSet exposed_interfaces_;
  bool expose_all_interfaces_ = false;

  // Contains every interface binder that has been registered with this
  // InterfaceRegistry. Not all binders may be reachable depending on the
  // capabilities requested by the remote. Only interfaces in
  // exposed_interfaces_ may be bound. When |expose_all_interfaces_| is true,
  // any interface may be bound.
  InterfaceNameToBinderMap name_to_binder_;
  Binder default_binder_;

  bool is_paused_ = false;

  // Pending interface requests which can accumulate if GetInterface() is called
  // while binding is paused.
  std::queue<std::pair<std::string, mojo::ScopedMessagePipeHandle>>
      pending_interface_requests_;

  std::list<base::Closure> connection_lost_closures_;

  base::WeakPtrFactory<InterfaceRegistry> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InterfaceRegistry);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_REGISTRY_H_
