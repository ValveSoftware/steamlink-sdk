// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "jingle/glue/task_pump.h"

namespace jingle_glue {

TaskPump::TaskPump()
    : posted_wake_(false),
      stopped_(false),
      weak_factory_(this) {
}

TaskPump::~TaskPump() {
  DCHECK(CalledOnValidThread());
}

void TaskPump::WakeTasks() {
  DCHECK(CalledOnValidThread());
  if (!stopped_ && !posted_wake_) {
    base::MessageLoop* current_message_loop = base::MessageLoop::current();
    CHECK(current_message_loop);
    // Do the requested wake up.
    current_message_loop->PostTask(
        FROM_HERE,
        base::Bind(&TaskPump::CheckAndRunTasks, weak_factory_.GetWeakPtr()));
    posted_wake_ = true;
  }
}

int64 TaskPump::CurrentTime() {
  DCHECK(CalledOnValidThread());
  // Only timeout tasks rely on this function.  Since we're not using
  // libjingle tasks for timeout, it's safe to return 0 here.
  return 0;
}

void TaskPump::Stop() {
  stopped_ = true;
}

void TaskPump::CheckAndRunTasks() {
  DCHECK(CalledOnValidThread());
  if (stopped_) {
    return;
  }
  posted_wake_ = false;
  // We shouldn't be using libjingle for timeout tasks, so we should
  // have no timeout tasks at all.

  // TODO(akalin): Add HasTimeoutTask() back in TaskRunner class and
  // uncomment this check.
  // DCHECK(!HasTimeoutTask())
  RunTasks();
}

}  // namespace jingle_glue
