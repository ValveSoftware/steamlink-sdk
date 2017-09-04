// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AbstractAnimationWorkletThread_h
#define AbstractAnimationWorkletThread_h

#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerThread.h"
#include "modules/ModulesExport.h"
#include <memory>

namespace blink {

class WorkerReportingProxy;

// TODO(ikilpatrick): Remove this class up to AnimationWorkletThread once we no
// longer have CompositorWorker.
class MODULES_EXPORT AbstractAnimationWorkletThread : public WorkerThread {
 public:
  ~AbstractAnimationWorkletThread() override;

  WorkerBackingThread& workerBackingThread() override;

  // The backing thread is cleared by clearSharedBackingThread().
  void clearWorkerBackingThread() override {}

  // This may block the main thread.
  static void collectAllGarbage();

  static void ensureSharedBackingThread();
  static void clearSharedBackingThread();

  static void createSharedBackingThreadForTest();

 protected:
  AbstractAnimationWorkletThread(PassRefPtr<WorkerLoaderProxy>,
                                 WorkerReportingProxy&);

  bool isOwningBackingThread() const override { return false; }
};

}  // namespace blink

#endif  // AbstractAnimationWorkletThread_h
