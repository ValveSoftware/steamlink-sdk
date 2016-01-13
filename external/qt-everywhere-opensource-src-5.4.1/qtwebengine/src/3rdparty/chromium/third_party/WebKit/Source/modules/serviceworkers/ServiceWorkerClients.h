// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerClients_h
#define ServiceWorkerClients_h

#include "bindings/v8/ScriptWrappable.h"
#include "public/platform/WebServiceWorkerClientsInfo.h"
#include "wtf/Forward.h"

namespace WebCore {

class Client;
class ExecutionContext;
class ScriptState;
class ScriptPromise;

class ServiceWorkerClients FINAL : public RefCounted<ServiceWorkerClients>, public ScriptWrappable {
public:
    static PassRefPtr<ServiceWorkerClients> create();
    ~ServiceWorkerClients();

    // ServiceWorkerClients.idl
    ScriptPromise getServiced(ScriptState*);

private:
    ServiceWorkerClients();
};

} // namespace WebCore

#endif // ServiceWorkerClients_h
