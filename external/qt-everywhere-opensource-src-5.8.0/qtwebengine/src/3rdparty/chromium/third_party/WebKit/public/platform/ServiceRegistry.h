// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceRegistry_h
#define ServiceRegistry_h

#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "public/platform/WebCommon.h"

namespace blink {

// Implementations of blink::ServiceRegistry should be thread safe. As such it
// is okay to call |connectToRemoteService| from any thread, without the
// thread hopping that would have been necesary with content::ServiceRegistry.
class BLINK_PLATFORM_EXPORT ServiceRegistry {
public:
    virtual void connectToRemoteService(const char* name, mojo::ScopedMessagePipeHandle) = 0;

    template <typename Interface>
    void connectToRemoteService(mojo::InterfaceRequest<Interface> ptr)
    {
        connectToRemoteService(Interface::Name_, ptr.PassMessagePipe());
    }

    static ServiceRegistry* getEmptyServiceRegistry();
};

} // namespace blink

#endif
