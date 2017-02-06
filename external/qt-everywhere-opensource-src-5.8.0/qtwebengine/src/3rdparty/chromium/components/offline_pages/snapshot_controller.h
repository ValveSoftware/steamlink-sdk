// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_SNAPSHOT_CONTROLLER_H_
#define COMPONENTS_OFFLINE_PAGES_SNAPSHOT_CONTROLLER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"

namespace offline_pages {

// Takes various signals and produces StartSnapshot calls following a specific
// policy. Can request snapshots multiple times per 'session'. Session can be
// ended and another one started by calling Reset().
// Main invariants:
// - It never starts overlapping snapshots, Client reports when previous
//   snapshot is done.
// - The currently worked on (pending) snapshot is always allowed to complete,
//   the new attempts to start a snapshot are ignored until it does.
// - Some signals prevent more snapshots to be taken.
//   OnLoad is currently such signal.
// - Once Reset() is called on the SnapshotController, the delayed tasks are
//   reset so no StartSnapshot calls is made 'cross-session'.
class SnapshotController {
 public:
  enum class State {
    READY,             // Listening to input, will start snapshot when needed.
    SNAPSHOT_PENDING,  // Snapshot is in progress, don't start another.
    STOPPED,           // Terminal state, no snapshots until reset.
  };

  // Client of the SnapshotController.
  class  Client {
   public:
    // Invoked at a good moment to start a snapshot. May be invoked multiple
    // times, but not in overlapping manner - waits until
    // PreviousSnapshotCompleted() before the next StatrSnapshot().
    // Client should overwrite the result of previous snapshot with the new one,
    // it is assumed that later snapshots are better then previous.
    virtual void StartSnapshot() = 0;

   protected:
    virtual ~Client() {}
  };

  SnapshotController(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      SnapshotController::Client* client);
  virtual ~SnapshotController();

  // Resets the 'session', returning controller to initial state.
  void Reset();

  // Stops current session, no more Client::StartSnapshot calls will be
  // invoked from the SnapshotController until current session is Reset().
  // Called by Client, for example when it encounters an error loading the page.
  void Stop();

  // The way for Client to report that previously started snapshot is
  // now completed (so the next one can be started).
  void PendingSnapshotCompleted();

  // Invoked from WebContentObserver::DocumentAvailableInMainFrame
  void DocumentAvailableInMainFrame();

  // Invoked from WebContentObserver::DocumentOnLoadCompletedInMainFrame
  void DocumentOnLoadCompletedInMainFrame();

  size_t GetDelayAfterDocumentAvailableForTest();

 private:
  void MaybeStartSnapshot();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  // Client owns this class.
  SnapshotController::Client* client_;
  SnapshotController::State state_;

  base::WeakPtrFactory<SnapshotController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SnapshotController);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_SNAPSHOT_CONTROLLER_H_
