// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorWorkerThread_h
#define CompositorWorkerThread_h

#include "modules/ModulesExport.h"
#include "modules/compositorworker/AbstractAnimationWorkletThread.h"
#include <memory>

namespace blink {

class InProcessWorkerObjectProxy;

class MODULES_EXPORT CompositorWorkerThread final
    : public AbstractAnimationWorkletThread {
 public:
  static std::unique_ptr<CompositorWorkerThread> create(
      PassRefPtr<WorkerLoaderProxy>,
      InProcessWorkerObjectProxy&,
      double timeOrigin);
  ~CompositorWorkerThread() override;

  InProcessWorkerObjectProxy& workerObjectProxy() const {
    return m_workerObjectProxy;
  }

 protected:
  WorkerOrWorkletGlobalScope* createWorkerGlobalScope(
      std::unique_ptr<WorkerThreadStartupData>) override;

 private:
  CompositorWorkerThread(PassRefPtr<WorkerLoaderProxy>,
                         InProcessWorkerObjectProxy&,
                         double timeOrigin);

  InProcessWorkerObjectProxy& m_workerObjectProxy;
  double m_timeOrigin;
};

}  // namespace blink

#endif  // CompositorWorkerThread_h
