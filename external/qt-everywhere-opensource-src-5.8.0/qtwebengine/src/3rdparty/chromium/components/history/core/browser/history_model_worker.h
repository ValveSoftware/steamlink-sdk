// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_MODEL_WORKER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_MODEL_WORKER_H_

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"

namespace history {
class HistoryService;
}

namespace browser_sync {

// A syncer::ModelSafeWorker for history models that accepts requests
// from the syncapi that need to be fulfilled on the history thread.
class HistoryModelWorker : public syncer::ModelSafeWorker {
 public:
  explicit HistoryModelWorker(
      const base::WeakPtr<history::HistoryService>& history_service,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
      syncer::WorkerLoopDestructionObserver* observer);

  // syncer::ModelSafeWorker implementation. Called on syncapi SyncerThread.
  void RegisterForLoopDestruction() override;
  syncer::ModelSafeGroup GetModelSafeGroup() override;

  // Called on history DB thread to register HistoryModelWorker to observe
  // destruction of history backend loop.
  void RegisterOnDBThread();

 protected:
  syncer::SyncerError DoWorkAndWaitUntilDoneImpl(
      const syncer::WorkCallback& work) override;

 private:
  ~HistoryModelWorker() override;

  const base::WeakPtr<history::HistoryService> history_service_;

  // A reference to the UI thread's task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> ui_thread_;

  // Helper object to make sure we don't leave tasks running on the history
  // thread.
  std::unique_ptr<base::CancelableTaskTracker> cancelable_tracker_;

  DISALLOW_COPY_AND_ASSIGN(HistoryModelWorker);
};

}  // namespace browser_sync

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_MODEL_WORKER_H_
