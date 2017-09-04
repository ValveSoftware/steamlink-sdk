// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioWorkletThread_h
#define AudioWorkletThread_h

#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerThread.h"
#include "core/workers/WorkletThreadHolder.h"
#include "modules/ModulesExport.h"
#include <memory>

namespace blink {

class WorkerReportingProxy;

// AudioWorkletThread is a per-frame singleton object that represents the
// backing thread for the processing of AudioWorkletNode/AudioWorkletProcessor.
// It is supposed to run an instance of V8 isolate.

class MODULES_EXPORT AudioWorkletThread final : public WorkerThread {
 public:
  static std::unique_ptr<AudioWorkletThread> create(
      PassRefPtr<WorkerLoaderProxy>,
      WorkerReportingProxy&);
  ~AudioWorkletThread() override;

  WorkerBackingThread& workerBackingThread() override;

  // The backing thread is cleared by clearSharedBackingThread().
  void clearWorkerBackingThread() override {}

  // This may block the main thread.
  static void collectAllGarbage();

  static void ensureSharedBackingThread();
  static void clearSharedBackingThread();

  static void createSharedBackingThreadForTest();

 protected:
  WorkerOrWorkletGlobalScope* createWorkerGlobalScope(
      std::unique_ptr<WorkerThreadStartupData>) final;

  bool isOwningBackingThread() const override { return false; }

 private:
  AudioWorkletThread(PassRefPtr<WorkerLoaderProxy>, WorkerReportingProxy&);
};

}  // namespace blink

#endif  // AudioWorkletThread_h
