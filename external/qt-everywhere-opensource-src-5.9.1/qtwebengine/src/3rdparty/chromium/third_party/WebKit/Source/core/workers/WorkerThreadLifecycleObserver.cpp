// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkerThreadLifecycleObserver.h"

#include "core/workers/WorkerThread.h"
#include "wtf/Assertions.h"
#include "wtf/WTF.h"

namespace blink {

WorkerThreadLifecycleObserver::WorkerThreadLifecycleObserver(
    WorkerThreadLifecycleContext* workerThreadLifecycleContext)
    : LifecycleObserver(workerThreadLifecycleContext),
      m_wasContextDestroyedBeforeObserverCreation(
          workerThreadLifecycleContext->m_wasContextDestroyed) {
  DCHECK(isMainThread());
}

WorkerThreadLifecycleObserver::~WorkerThreadLifecycleObserver() {}

}  // namespace blink
