// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_ASSOCIATED_INTERFACE_PROVIDER_H_
#define CONTENT_PUBLIC_COMMON_ASSOCIATED_INTERFACE_PROVIDER_H_

#include <string>

#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "mojo/public/cpp/bindings/associated_interface_request.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace mojo {
class AssociatedGroup;
}

namespace content {

// A helper interface for connecting to remote Channel-associated interfaces.
//
// This is analogous to service_manager::InterfaceProvider in that it provides a
// means of
// binding proxies to remote interfaces, but this is specifically for interfaces
// which must be assocaited with an IPC::Channel, i.e. retain FIFO message
// ordering with respect to legacy IPC messages.
//
// The Channel with which the remote interfaces are associated depends on
// the configuration of the specific AssociatedInterfaceProvider instance. For
// example, RenderFrameHost exposes an instance of this class for which all
// interfaces are associted with the IPC::ChannelProxy to the render process
// which hosts its corresponding RenderFrame.
class AssociatedInterfaceProvider {
 public:
  virtual ~AssociatedInterfaceProvider() {}

  // Passes an associated endpoint handle to the remote end to be bound to a
  // Channel-associated interface named |name|.
  virtual void GetInterface(const std::string& name,
                            mojo::ScopedInterfaceEndpointHandle handle) = 0;

  // Returns an AssociatedGroup for the provider. This may be used to create
  // new associated endpoints for use with Channel-associated interfaces.
  virtual mojo::AssociatedGroup* GetAssociatedGroup() = 0;

  // Templated helper for GetInterface().
  template <typename Interface>
  void GetInterface(mojo::AssociatedInterfacePtr<Interface>* proxy) {
    mojo::AssociatedInterfaceRequest<Interface> request =
        mojo::GetProxy(proxy, GetAssociatedGroup());
    GetInterface(Interface::Name_, request.PassHandle());
  }

  virtual void OverrideBinderForTesting(
      const std::string& name,
      const base::Callback<void(mojo::ScopedInterfaceEndpointHandle)>&
          binder) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_ASSOCIATED_INTERFACE_PROVIDER_H_
