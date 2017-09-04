// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/associated_interface_provider_impl.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {

class AssociatedInterfaceProviderImpl::LocalProvider
    : public mojom::RouteProvider,
      mojom::AssociatedInterfaceProvider {
 public:
  explicit LocalProvider(mojom::AssociatedInterfaceProviderAssociatedPtr* proxy)
      : route_provider_binding_(this),
        associated_interface_provider_binding_(this) {
    route_provider_binding_.Bind(mojo::GetProxy(&route_provider_ptr_));
    route_provider_ptr_->GetRoute(
        0, mojo::GetProxy(proxy, route_provider_ptr_.associated_group()));
  }

  ~LocalProvider() override {}

  void SetBinderForName(
      const std::string& name,
      const base::Callback<void(mojo::ScopedInterfaceEndpointHandle)>& binder) {
    binders_[name] = binder;
  }

 private:
  // mojom::RouteProvider:
  void GetRoute(
      int32_t routing_id,
      mojom::AssociatedInterfaceProviderAssociatedRequest request) override {
    DCHECK(request.is_pending());
    associated_interface_provider_binding_.Bind(std::move(request));
  }

  // mojom::AssociatedInterfaceProvider:
  void GetAssociatedInterface(
      const std::string& name,
      mojom::AssociatedInterfaceAssociatedRequest request) override {
    auto it = binders_.find(name);
    if (it != binders_.end())
      it->second.Run(request.PassHandle());
  }

  using BinderMap =
      std::map<std::string,
               base::Callback<void(mojo::ScopedInterfaceEndpointHandle)>>;
  BinderMap binders_;

  mojom::RouteProviderPtr route_provider_ptr_;
  mojo::Binding<mojom::RouteProvider> route_provider_binding_;

  mojo::AssociatedBinding<mojom::AssociatedInterfaceProvider>
      associated_interface_provider_binding_;
};

AssociatedInterfaceProviderImpl::AssociatedInterfaceProviderImpl(
    mojom::AssociatedInterfaceProviderAssociatedPtr proxy)
    : proxy_(std::move(proxy)) {
  DCHECK(proxy_.is_bound());
}

AssociatedInterfaceProviderImpl::AssociatedInterfaceProviderImpl()
    : local_provider_(new LocalProvider(&proxy_)) {}

AssociatedInterfaceProviderImpl::~AssociatedInterfaceProviderImpl() {}

void AssociatedInterfaceProviderImpl::GetInterface(
    const std::string& name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  mojom::AssociatedInterfaceAssociatedRequest request;
  request.Bind(std::move(handle));
  return proxy_->GetAssociatedInterface(name, std::move(request));
}

mojo::AssociatedGroup* AssociatedInterfaceProviderImpl::GetAssociatedGroup() {
  return proxy_.associated_group();
}

void AssociatedInterfaceProviderImpl::OverrideBinderForTesting(
    const std::string& name,
    const base::Callback<void(mojo::ScopedInterfaceEndpointHandle)>& binder) {
  DCHECK(local_provider_);
  local_provider_->SetBinderForName(name, binder);
}

}  // namespace content
