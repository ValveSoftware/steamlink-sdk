// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_scheduler/task_scheduler.h"

#include "base/logging.h"

namespace base {

namespace {

// |g_task_scheduler| is intentionally leaked on shutdown.
TaskScheduler* g_task_scheduler = nullptr;

}  // namespace

// static
void TaskScheduler::SetInstance(std::unique_ptr<TaskScheduler> task_scheduler) {
  delete g_task_scheduler;
  g_task_scheduler = task_scheduler.release();
}

// static
TaskScheduler* TaskScheduler::GetInstance() {
  return g_task_scheduler;
}

}  // namespace base
