// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_CONNECT_UTIL_H_
#define SERVICES_SHELL_CONNECT_UTIL_H_

#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/system/handle.h"
#include "services/shell/public/cpp/identity.h"
#include "services/shell/public/interfaces/connector.mojom.h"

namespace shell {

class Shell;

mojo::ScopedMessagePipeHandle ConnectToInterfaceByName(
    Shell* shell,
    const Identity& source,
    const Identity& target,
    const std::string& interface_name);

// Must only be used by shell internals and test code as it does not forward
// capability filters. Runs |name| with a permissive capability filter.
template <typename Interface>
inline void ConnectToInterface(Shell* shell,
                               const Identity& source,
                               const Identity& target,
                               mojo::InterfacePtr<Interface>* ptr) {
  mojo::ScopedMessagePipeHandle service_handle =
      ConnectToInterfaceByName(shell, source, target, Interface::Name_);
  ptr->Bind(mojo::InterfacePtrInfo<Interface>(std::move(service_handle), 0u));
}

template <typename Interface>
inline void ConnectToInterface(Shell* shell,
                               const Identity& source,
                               const std::string& name,
                               mojo::InterfacePtr<Interface>* ptr) {
  mojo::ScopedMessagePipeHandle service_handle = ConnectToInterfaceByName(
      shell, source, Identity(name, mojom::kInheritUserID), Interface::Name_);
  ptr->Bind(mojo::InterfacePtrInfo<Interface>(std::move(service_handle), 0u));
}

}  // namespace shell

#endif  // SERVICES_SHELL_CONNECT_UTIL_H_
