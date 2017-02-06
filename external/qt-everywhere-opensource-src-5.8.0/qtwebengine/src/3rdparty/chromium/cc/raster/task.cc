// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/task.h"

#include "base/logging.h"

namespace cc {

TaskState::TaskState() : value_(Value::NEW) {}

TaskState::~TaskState() {
  DCHECK(value_ != Value::RUNNING)
      << "Running task should never get destroyed.";
  DCHECK(value_ == Value::FINISHED || value_ == Value::CANCELED)
      << "Task, if scheduled, should get concluded either in FINISHED or "
         "CANCELED state.";
}

bool TaskState::IsScheduled() const {
  return value_ == Value::SCHEDULED;
}

bool TaskState::IsRunning() const {
  return value_ == Value::RUNNING;
}

bool TaskState::IsFinished() const {
  return value_ == Value::FINISHED;
}

bool TaskState::IsCanceled() const {
  return value_ == Value::CANCELED;
}

void TaskState::Reset() {
  value_ = Value::NEW;
}

void TaskState::DidSchedule() {
  DCHECK(value_ == Value::NEW)
      << "Task should be in NEW state to get scheduled.";
  value_ = Value::SCHEDULED;
}

void TaskState::DidStart() {
  DCHECK(value_ == Value::SCHEDULED)
      << "Task should be only in SCHEDULED state to start, that is it should "
         "not be started or finished.";
  value_ = Value::RUNNING;
}

void TaskState::DidFinish() {
  DCHECK(value_ == Value::RUNNING)
      << "Task should be running and not finished earlier.";
  value_ = Value::FINISHED;
}

void TaskState::DidCancel() {
  DCHECK(value_ == Value::NEW || value_ == Value::SCHEDULED)
      << "Task should be either new or scheduled to get canceled.";
  value_ = Value::CANCELED;
}

Task::Task() {}

Task::~Task() {}

TaskGraph::TaskGraph() {}

TaskGraph::TaskGraph(const TaskGraph& other) = default;

TaskGraph::~TaskGraph() {}

void TaskGraph::Swap(TaskGraph* other) {
  nodes.swap(other->nodes);
  edges.swap(other->edges);
}

void TaskGraph::Reset() {
  nodes.clear();
  edges.clear();
}

}  // namespace cc
