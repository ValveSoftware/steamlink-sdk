// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_APPLICATION_CONNECT_H_
#define MOJO_PUBLIC_CPP_APPLICATION_CONNECT_H_

#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"

namespace mojo {

template <typename Interface>
inline void ConnectToService(ServiceProvider* service_provider,
                             const std::string& url,
                             InterfacePtr<Interface>* ptr) {
  MessagePipe pipe;
  ptr->Bind(pipe.handle0.Pass());
  service_provider->ConnectToService(
      url, Interface::Name_, pipe.handle1.Pass(), std::string());
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_APPLICATION_CONNECT_H_
