// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/CompositorProxyClient.h"

#include "platform/TraceEvent.h"

namespace blink {

CompositorProxyClient* CompositorProxyClient::from(WorkerClients* clients)
{
    return static_cast<CompositorProxyClient*>(Supplement<WorkerClients>::from(clients, supplementName()));
}

const char* CompositorProxyClient::supplementName()
{
    return "CompositorProxyClient";
}

void provideCompositorProxyClientTo(WorkerClients* clients, CompositorProxyClient* client)
{
    clients->provideSupplement(CompositorProxyClient::supplementName(), client);
}

} // namespace blink
