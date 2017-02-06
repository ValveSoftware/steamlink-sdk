// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorWorkerMessagingProxy_h
#define CompositorWorkerMessagingProxy_h

#include "core/workers/InProcessWorkerMessagingProxy.h"
#include "wtf/Allocator.h"
#include <memory>

namespace blink {

class CompositorWorkerMessagingProxy final : public InProcessWorkerMessagingProxy {
    USING_FAST_MALLOC(CompositorWorkerMessagingProxy);
public:
    CompositorWorkerMessagingProxy(InProcessWorkerBase*, WorkerClients*);

protected:
    ~CompositorWorkerMessagingProxy() override;

    std::unique_ptr<WorkerThread> createWorkerThread(double originTime) override;
};

} // namespace blink

#endif // CompositorWorkerMessagingProxy_h
