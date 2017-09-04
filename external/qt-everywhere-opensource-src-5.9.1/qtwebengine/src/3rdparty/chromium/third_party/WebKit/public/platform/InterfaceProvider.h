// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterfaceProvider_h
#define InterfaceProvider_h

#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "public/platform/WebCommon.h"

namespace blink {

// Implementations of blink::InterfaceProvider should be thread safe. As such it
// is okay to call |getInterface| from any thread, without the thread hopping
// that would have been necesary with service_manager::InterfaceProvider.
class BLINK_PLATFORM_EXPORT InterfaceProvider {
 public:
  virtual void getInterface(const char* name,
                            mojo::ScopedMessagePipeHandle) = 0;

  template <typename Interface>
  void getInterface(mojo::InterfaceRequest<Interface> ptr) {
    getInterface(Interface::Name_, ptr.PassMessagePipe());
  }

  static InterfaceProvider* getEmptyInterfaceProvider();
};

}  // namespace blink

#endif
