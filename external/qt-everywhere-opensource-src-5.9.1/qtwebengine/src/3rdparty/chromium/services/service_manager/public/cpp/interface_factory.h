// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_FACTORY_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_FACTORY_H_

#include "mojo/public/cpp/bindings/interface_request.h"

namespace service_manager {

class Identity;

// Implement this class to provide implementations of a given interface and
// bind them to incoming requests. The implementation of this class is
// responsible for managing the lifetime of the implementations of the
// interface.
template <typename Interface>
class InterfaceFactory {
 public:
  virtual ~InterfaceFactory() {}
  virtual void Create(const Identity& remote_identity,
                      mojo::InterfaceRequest<Interface> request) = 0;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_FACTORY_H_
