// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/ServiceRegistry.h"

#include "wtf/StdLibExtras.h"

namespace blink {
namespace {

class EmptyServiceRegistry : public ServiceRegistry {
    void connectToRemoteService(const char* name, mojo::ScopedMessagePipeHandle) override {}
};

}

ServiceRegistry* ServiceRegistry::getEmptyServiceRegistry()
{
    DEFINE_STATIC_LOCAL(EmptyServiceRegistry, emptyServiceRegistry, ());
    return &emptyServiceRegistry;
}

}
