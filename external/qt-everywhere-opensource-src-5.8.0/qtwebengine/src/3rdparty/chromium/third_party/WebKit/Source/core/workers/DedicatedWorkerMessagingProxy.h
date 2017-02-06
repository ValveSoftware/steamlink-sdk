// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DedicatedWorkerMessagingProxy_h
#define DedicatedWorkerMessagingProxy_h

#include "core/CoreExport.h"
#include "core/workers/InProcessWorkerMessagingProxy.h"
#include <memory>

namespace blink {

class CORE_EXPORT DedicatedWorkerMessagingProxy final : public InProcessWorkerMessagingProxy {
    WTF_MAKE_NONCOPYABLE(DedicatedWorkerMessagingProxy);
    USING_FAST_MALLOC(DedicatedWorkerMessagingProxy);
public:
    DedicatedWorkerMessagingProxy(InProcessWorkerBase*, WorkerClients*);
    ~DedicatedWorkerMessagingProxy() override;

    std::unique_ptr<WorkerThread> createWorkerThread(double originTime) override;
};

} // namespace blink

#endif // DedicatedWorkerMessagingProxy_h
