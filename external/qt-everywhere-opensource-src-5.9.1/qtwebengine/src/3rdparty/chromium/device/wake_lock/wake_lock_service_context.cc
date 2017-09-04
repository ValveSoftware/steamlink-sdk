// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/wake_lock/wake_lock_service_context.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "device/power_save_blocker/power_save_blocker.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace device {

WakeLockServiceContext::WakeLockServiceContext(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    base::Callback<gfx::NativeView()> native_view_getter)
    : main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      file_task_runner_(file_task_runner),
      num_lock_requests_(0),
      native_view_getter_(native_view_getter),
      weak_factory_(this) {}

WakeLockServiceContext::~WakeLockServiceContext() {}

void WakeLockServiceContext::CreateService(
    mojo::InterfaceRequest<mojom::WakeLockService> request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<WakeLockServiceImpl>(weak_factory_.GetWeakPtr()),
      std::move(request));
}

void WakeLockServiceContext::RequestWakeLock() {
  DCHECK(main_task_runner_->RunsTasksOnCurrentThread());
  num_lock_requests_++;
  UpdateWakeLock();
}

void WakeLockServiceContext::CancelWakeLock() {
  DCHECK(main_task_runner_->RunsTasksOnCurrentThread());
  num_lock_requests_--;
  UpdateWakeLock();
}

bool WakeLockServiceContext::HasWakeLockForTests() const {
  return !!wake_lock_;
}

void WakeLockServiceContext::CreateWakeLock() {
  DCHECK(!wake_lock_);
  wake_lock_.reset(new device::PowerSaveBlocker(
      device::PowerSaveBlocker::kPowerSaveBlockPreventDisplaySleep,
      device::PowerSaveBlocker::kReasonOther, "Wake Lock API",
      main_task_runner_, file_task_runner_));

#if defined(OS_ANDROID)
  gfx::NativeView native_view = native_view_getter_.Run();
  if (native_view) {
    wake_lock_.get()->InitDisplaySleepBlocker(native_view);
  }
#endif
}

void WakeLockServiceContext::RemoveWakeLock() {
  DCHECK(wake_lock_);
  wake_lock_.reset();
}

void WakeLockServiceContext::UpdateWakeLock() {
  DCHECK(num_lock_requests_ >= 0);
  if (num_lock_requests_) {
    if (!wake_lock_)
      CreateWakeLock();
  } else {
    if (wake_lock_)
      RemoveWakeLock();
  }
}

}  // namespace device
