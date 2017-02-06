// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_PUBLIC_CPP_INTERFACE_BINDER_H_
#define SERVICES_SHELL_PUBLIC_CPP_INTERFACE_BINDER_H_

#include <string>

#include "mojo/public/cpp/system/message_pipe.h"

namespace shell {

class Connection;

class InterfaceBinder {
 public:
  virtual ~InterfaceBinder() {}

  // Asks the InterfaceBinder to bind an implementation of the specified
  // interface to the request passed via |handle|. If the InterfaceBinder binds
  // an implementation it must take ownership of the request handle.
  virtual void BindInterface(Connection* connection,
                             const std::string& interface_name,
                             mojo::ScopedMessagePipeHandle handle) = 0;
};

}  // namespace shell

#endif  // SERVICES_SHELL_PUBLIC_CPP_INTERFACE_BINDER_H_
