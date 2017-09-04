// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/snapshot_controller.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/time/time.h"

namespace {
// Default delay, in milliseconds, between the main document parsed event and
// snapshot. Note: this snapshot might not occur if the OnLoad event and
// OnLoad delay elapses first to trigger a final snapshot.
const size_t kDefaultDelayAfterDocumentAvailableMs = 7000;

// Default delay, in milliseconds, between the main document OnLoad event and
// snapshot.
const size_t kDelayAfterDocumentOnLoadCompletedMs = 1000;

}  // namespace

namespace offline_pages {

SnapshotController::SnapshotController(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    SnapshotController::Client* client)
    : task_runner_(task_runner),
      client_(client),
      state_(State::READY),
      delay_after_document_available_ms_(
          kDefaultDelayAfterDocumentAvailableMs),
      delay_after_document_on_load_completed_ms_(
          kDelayAfterDocumentOnLoadCompletedMs),
      weak_ptr_factory_(this) {}

SnapshotController::SnapshotController(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    SnapshotController::Client* client,
    size_t delay_after_document_available_ms,
    size_t delay_after_document_on_load_completed_ms)
    : task_runner_(task_runner),
      client_(client),
      state_(State::READY),
      delay_after_document_available_ms_(
          delay_after_document_available_ms),
      delay_after_document_on_load_completed_ms_(
          delay_after_document_on_load_completed_ms),
      weak_ptr_factory_(this) {}

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
  // Post a delayed task to snapshot.
  task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&SnapshotController::MaybeStartSnapshot,
                            weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(
          delay_after_document_available_ms_));
}

void SnapshotController::DocumentOnLoadCompletedInMainFrame() {
  // Post a delayed task to snapshot and then stop this controller.
  task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&SnapshotController::MaybeStartSnapshotThenStop,
                            weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(
          delay_after_document_on_load_completed_ms_));
}

void SnapshotController::MaybeStartSnapshot() {
  if (state_ != State::READY)
    return;
  state_ = State::SNAPSHOT_PENDING;
  client_->StartSnapshot();
}

void SnapshotController::MaybeStartSnapshotThenStop() {
  MaybeStartSnapshot();
  Stop();
}

size_t SnapshotController::GetDelayAfterDocumentAvailableForTest() {
  return delay_after_document_available_ms_;
}

size_t SnapshotController::GetDelayAfterDocumentOnLoadCompletedForTest() {
  return delay_after_document_on_load_completed_ms_;
}


}  // namespace offline_pages
