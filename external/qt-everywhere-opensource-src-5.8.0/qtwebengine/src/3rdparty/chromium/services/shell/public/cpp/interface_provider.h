// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_PUBLIC_CPP_INTERFACE_PROVIDER_H_
#define SERVICES_SHELL_PUBLIC_CPP_INTERFACE_PROVIDER_H_

#include "services/shell/public/interfaces/interface_provider.mojom.h"

namespace shell {

// Encapsulates a mojom::InterfaceProviderPtr implemented in a remote
// application. Provides two main features:
// - a typesafe GetInterface() method for binding InterfacePtrs.
// - a testing API that allows local callbacks to be registered that bind
//   requests for remote interfaces.
// An instance of this class is used by the GetInterface() methods on
// Connection.
class InterfaceProvider {
 public:
  class TestApi {
   public:
    explicit TestApi(InterfaceProvider* provider) : provider_(provider) {}
    ~TestApi() {}

    void SetBinderForName(
        const std::string& name,
        const base::Callback<void(mojo::ScopedMessagePipeHandle)>& binder) {
      provider_->SetBinderForName(name, binder);
    }

    void ClearBinders() {
      provider_->ClearBinders();
    }

   private:
    InterfaceProvider* provider_;
    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  InterfaceProvider();
  ~InterfaceProvider();

  void Bind(mojom::InterfaceProviderPtr interface_provider);

  // Returns a raw pointer to the remote InterfaceProvider.
  mojom::InterfaceProvider* get() { return interface_provider_.get(); }

  // Sets a closure to be run when the remote InterfaceProvider pipe is closed.
  void SetConnectionLostClosure(const base::Closure& connection_lost_closure);

  base::WeakPtr<InterfaceProvider> GetWeakPtr();

  // Binds |ptr| to an implementation of Interface in the remote application.
  // |ptr| can immediately be used to start sending requests to the remote
  // interface.
  template <typename Interface>
  void GetInterface(mojo::InterfacePtr<Interface>* ptr) {
    mojo::MessagePipe pipe;
    ptr->Bind(mojo::InterfacePtrInfo<Interface>(std::move(pipe.handle0), 0u));

    GetInterface(Interface::Name_, std::move(pipe.handle1));
  }
  template <typename Interface>
  void GetInterface(mojo::InterfaceRequest<Interface> request) {
    GetInterface(Interface::Name_, std::move(request.PassMessagePipe()));
  }
  void GetInterface(const std::string& name,
                    mojo::ScopedMessagePipeHandle request_handle);

 private:
  void SetBinderForName(
      const std::string& name,
      const base::Callback<void(mojo::ScopedMessagePipeHandle)>& binder) {
    binders_[name] = binder;
  }
  void ClearBinders();

  using BinderMap = std::map<
      std::string, base::Callback<void(mojo::ScopedMessagePipeHandle)>>;
  BinderMap binders_;

  mojom::InterfaceProviderPtr interface_provider_;
  mojom::InterfaceProviderRequest pending_request_;

  base::WeakPtrFactory<InterfaceProvider> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InterfaceProvider);
};

}  // namespace shell

#endif  // SERVICES_SHELL_PUBLIC_CPP_INTERFACE_PROVIDER_H_
