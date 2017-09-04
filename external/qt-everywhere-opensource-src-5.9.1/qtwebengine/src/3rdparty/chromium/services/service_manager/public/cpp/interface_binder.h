// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_BINDER_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_BINDER_H_

#include <string>

#include "mojo/public/cpp/system/message_pipe.h"

namespace service_manager {

class Identity;

class InterfaceBinder {
 public:
  virtual ~InterfaceBinder() {}

  // Asks the InterfaceBinder to bind an implementation of the specified
  // interface to the request passed via |handle|. If the InterfaceBinder binds
  // an implementation it must take ownership of the request handle.
  virtual void BindInterface(const Identity& remote_identity,
                             const std::string& interface_name,
                             mojo::ScopedMessagePipeHandle handle) = 0;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_BINDER_H_
