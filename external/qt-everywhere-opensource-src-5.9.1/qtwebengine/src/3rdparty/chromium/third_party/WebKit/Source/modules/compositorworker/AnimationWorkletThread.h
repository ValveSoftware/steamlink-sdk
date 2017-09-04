// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationWorkletThread_h
#define AnimationWorkletThread_h

#include "modules/ModulesExport.h"
#include "modules/compositorworker/AbstractAnimationWorkletThread.h"
#include <memory>

namespace blink {

class WorkerReportingProxy;

class MODULES_EXPORT AnimationWorkletThread final
    : public AbstractAnimationWorkletThread {
 public:
  static std::unique_ptr<AnimationWorkletThread> create(
      PassRefPtr<WorkerLoaderProxy>,
      WorkerReportingProxy&);
  ~AnimationWorkletThread() override;

 protected:
  WorkerOrWorkletGlobalScope* createWorkerGlobalScope(
      std::unique_ptr<WorkerThreadStartupData>) final;

 private:
  AnimationWorkletThread(PassRefPtr<WorkerLoaderProxy>, WorkerReportingProxy&);
};

}  // namespace blink

#endif  // AnimationWorkletThread_h
