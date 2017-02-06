/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/workers/DedicatedWorkerThread.h"

#include "core/workers/DedicatedWorkerGlobalScope.h"
#include "core/workers/InProcessWorkerObjectProxy.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

std::unique_ptr<DedicatedWorkerThread> DedicatedWorkerThread::create(PassRefPtr<WorkerLoaderProxy> workerLoaderProxy, InProcessWorkerObjectProxy& workerObjectProxy, double timeOrigin)
{
    return wrapUnique(new DedicatedWorkerThread(workerLoaderProxy, workerObjectProxy, timeOrigin));
}

DedicatedWorkerThread::DedicatedWorkerThread(PassRefPtr<WorkerLoaderProxy> workerLoaderProxy, InProcessWorkerObjectProxy& workerObjectProxy, double timeOrigin)
    : WorkerThread(workerLoaderProxy, workerObjectProxy)
    , m_workerBackingThread(WorkerBackingThread::create("DedicatedWorker Thread"))
    , m_workerObjectProxy(workerObjectProxy)
    , m_timeOrigin(timeOrigin)
{
}

DedicatedWorkerThread::~DedicatedWorkerThread()
{
}

WorkerGlobalScope* DedicatedWorkerThread::createWorkerGlobalScope(std::unique_ptr<WorkerThreadStartupData> startupData)
{
    return DedicatedWorkerGlobalScope::create(this, std::move(startupData), m_timeOrigin);
}

void DedicatedWorkerThread::postInitialize()
{
    // Notify the parent object of our current active state before the event
    // loop starts processing tasks.
    m_workerObjectProxy.reportPendingActivity(false);
}

} // namespace blink
