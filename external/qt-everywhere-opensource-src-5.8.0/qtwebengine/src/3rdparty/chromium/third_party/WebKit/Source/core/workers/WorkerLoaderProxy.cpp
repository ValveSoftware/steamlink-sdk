// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkerLoaderProxy.h"

#include "core/dom/ExecutionContextTask.h"

namespace blink {

WorkerLoaderProxy::WorkerLoaderProxy(WorkerLoaderProxyProvider* loaderProxyProvider)
    : m_loaderProxyProvider(loaderProxyProvider)
{
}

WorkerLoaderProxy::~WorkerLoaderProxy()
{
    DCHECK(!m_loaderProxyProvider);
}

void WorkerLoaderProxy::detachProvider(WorkerLoaderProxyProvider* proxyProvider)
{
    MutexLocker locker(m_lock);
    DCHECK(proxyProvider == m_loaderProxyProvider);
    m_loaderProxyProvider = nullptr;
}

void WorkerLoaderProxy::postTaskToLoader(std::unique_ptr<ExecutionContextTask> task)
{
    MutexLocker locker(m_lock);
    if (!m_loaderProxyProvider)
        return;

    m_loaderProxyProvider->postTaskToLoader(std::move(task));
}

bool WorkerLoaderProxy::postTaskToWorkerGlobalScope(std::unique_ptr<ExecutionContextTask> task)
{
    MutexLocker locker(m_lock);
    if (!m_loaderProxyProvider)
        return false;

    return m_loaderProxyProvider->postTaskToWorkerGlobalScope(std::move(task));
}

} // namespace blink
