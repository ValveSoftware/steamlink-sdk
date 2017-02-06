// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/snapshot_controller.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/time/time.h"

namespace {
// Delay, in milliseconds, between the main document parsed event and snapshot.
// Note if the "load" event fires before this delay is up, then the snapshot
// is taken immediately.
const size_t kDelayAfterDocumentAvailable = 7000;

}  // namespace

namespace offline_pages {

SnapshotController::SnapshotController(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    SnapshotController::Client* client)
    : task_runner_(task_runner),
      client_(client),
      state_(State::READY),
      weak_ptr_factory_(this) {
}

SnapshotController::~SnapshotController() {}

void SnapshotController::Reset() {
  // Cancel potentially delayed tasks that relate to the previous 'session'.
  weak_ptr_factory_.InvalidateWeakPtrs();
  state_ = State::READY;
}

void SnapshotController::Stop() {
  state_ = State::STOPPED;
}

void SnapshotController::PendingSnapshotCompleted() {
  // Unless the controller is "stopped", enable the subsequent snapshots.
  // Stopped state prevents any further snapshots form being started.
  if (state_ == State::STOPPED)
    return;
  state_ = State::READY;
}

void SnapshotController::DocumentAvailableInMainFrame() {
  // Post a delayed task. The snapshot will happen either when the delay
  // is up, or if the "load" event is dispatched in the main frame.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&SnapshotController::MaybeStartSnapshot,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kDelayAfterDocumentAvailable));
}

void SnapshotController::DocumentOnLoadCompletedInMainFrame() {
  MaybeStartSnapshot();
  // No more snapshots after onLoad (there still can be other events
  // or delayed tasks that can try to start another snapshot)
  Stop();
}

void SnapshotController::MaybeStartSnapshot() {
  if (state_ != State::READY)
    return;
  state_ = State::SNAPSHOT_PENDING;
  client_->StartSnapshot();
}

size_t SnapshotController::GetDelayAfterDocumentAvailableForTest() {
  return kDelayAfterDocumentAvailable;
}


}  // namespace offline_pages
