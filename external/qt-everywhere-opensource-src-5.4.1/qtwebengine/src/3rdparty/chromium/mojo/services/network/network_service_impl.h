// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_NETWORK_SERVICE_IMPL_H_
#define MOJO_SERVICES_NETWORK_NETWORK_SERVICE_IMPL_H_

#include "base/compiler_specific.h"
#include "mojo/public/cpp/bindings/interface_impl.h"
#include "mojo/services/public/interfaces/network/network_service.mojom.h"

namespace mojo {
class NetworkContext;

class NetworkServiceImpl : public InterfaceImpl<NetworkService> {
 public:
  explicit NetworkServiceImpl(NetworkContext* context);
  virtual ~NetworkServiceImpl();

  // NetworkService methods:
  virtual void CreateURLLoader(InterfaceRequest<URLLoader> loader) OVERRIDE;

 private:
  NetworkContext* context_;
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_NETWORK_SERVICE_IMPL_H_
