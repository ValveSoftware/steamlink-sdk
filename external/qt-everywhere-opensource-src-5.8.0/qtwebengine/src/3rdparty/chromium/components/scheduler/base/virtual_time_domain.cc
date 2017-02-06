// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/virtual_time_domain.h"

#include "base/bind.h"
#include "components/scheduler/base/task_queue_impl.h"
#include "components/scheduler/base/task_queue_manager.h"
#include "components/scheduler/base/task_queue_manager_delegate.h"

namespace scheduler {

VirtualTimeDomain::VirtualTimeDomain(TimeDomain::Observer* observer,
                                     base::TimeTicks initial_time)
    : TimeDomain(observer), now_(initial_time), task_queue_manager_(nullptr) {}

VirtualTimeDomain::~VirtualTimeDomain() {}

void VirtualTimeDomain::OnRegisterWithTaskQueueManager(
    TaskQueueManager* task_queue_manager) {
  task_queue_manager_ = task_queue_manager;
  DCHECK(task_queue_manager_);
}

LazyNow VirtualTimeDomain::CreateLazyNow() const {
  base::AutoLock lock(lock_);
  return LazyNow(now_);
}

base::TimeTicks VirtualTimeDomain::Now() const {
  base::AutoLock lock(lock_);
  return now_;
}

void VirtualTimeDomain::RequestWakeup(base::TimeTicks now,
                                      base::TimeDelta delay) {
  // We don't need to do anything here because the caller of AdvanceTo is
  // responsible for calling TaskQueueManager::MaybeScheduleImmediateWork if
  // needed.
}

bool VirtualTimeDomain::MaybeAdvanceTime() {
  return false;
}

void VirtualTimeDomain::AsValueIntoInternal(
    base::trace_event::TracedValue* state) const {}

void VirtualTimeDomain::AdvanceTo(base::TimeTicks now) {
  base::AutoLock lock(lock_);
  DCHECK_GE(now, now_);
  now_ = now;
}

void VirtualTimeDomain::RequestDoWork() {
  task_queue_manager_->MaybeScheduleImmediateWork(FROM_HERE);
}

const char* VirtualTimeDomain::GetName() const {
  return "VirtualTimeDomain";
}

}  // namespace scheduler
