// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/wake_lock/wake_lock_service_impl.h"

#include <utility>

#include "device/wake_lock/wake_lock_service_context.h"

namespace device {

WakeLockServiceImpl::WakeLockServiceImpl(
    base::WeakPtr<WakeLockServiceContext> context)
    : context_(context), wake_lock_request_outstanding_(false) {}

WakeLockServiceImpl::~WakeLockServiceImpl() {
  CancelWakeLock();
}

void WakeLockServiceImpl::RequestWakeLock() {
  if (!context_ || wake_lock_request_outstanding_)
    return;

  wake_lock_request_outstanding_ = true;
  context_->RequestWakeLock();
}

void WakeLockServiceImpl::CancelWakeLock() {
  if (!context_ || !wake_lock_request_outstanding_)
    return;

  wake_lock_request_outstanding_ = false;
  context_->CancelWakeLock();
}

}  // namespace device
