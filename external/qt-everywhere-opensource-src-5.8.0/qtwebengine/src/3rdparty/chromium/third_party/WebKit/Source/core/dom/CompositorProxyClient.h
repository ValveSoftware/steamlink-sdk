// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorProxyClient_h
#define CompositorProxyClient_h

#include "core/CoreExport.h"
#include "core/workers/WorkerClients.h"
#include "wtf/Noncopyable.h"

#include <v8.h>

namespace blink {

class CompositorProxy;
class Event;
class ScriptState;
class WorkerClients;
class WorkerGlobalScope;

class CORE_EXPORT CompositorProxyClient : public Supplement<WorkerClients> {
    WTF_MAKE_NONCOPYABLE(CompositorProxyClient);

public:
    CompositorProxyClient() {}

    static CompositorProxyClient* from(WorkerClients*);
    static const char* supplementName();

    virtual void setGlobalScope(WorkerGlobalScope*) = 0;
    virtual void requestAnimationFrame() = 0;
    virtual void registerCompositorProxy(CompositorProxy*) = 0;
    // It is not guaranteed to receive an unregister call for every registered
    // proxy. In fact we only receive one when a proxy is explicitly
    // disconnected otherwise we rely on oilpan collection process to remove the
    // weak reference to the proxy.
    virtual void unregisterCompositorProxy(CompositorProxy*) = 0;
};

CORE_EXPORT void provideCompositorProxyClientTo(WorkerClients*, CompositorProxyClient*);

} // namespace blink

#endif // CompositorProxyClient_h
