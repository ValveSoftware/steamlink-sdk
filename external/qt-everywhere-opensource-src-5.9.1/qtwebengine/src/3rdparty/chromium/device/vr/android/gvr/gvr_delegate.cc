// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_delegate.h"

#include "base/logging.h"

namespace device {

GvrDelegateProvider* GvrDelegateProvider::delegate_provider_ = nullptr;

GvrDelegateProvider* GvrDelegateProvider::GetInstance() {
  return delegate_provider_;
}

void GvrDelegateProvider::SetInstance(GvrDelegateProvider* delegate_provider) {
  if (delegate_provider) {
    // Don't initialize the delegate_provider_ twice.  Re-enable
    // (crbug.com/655297)
    // DCHECK(!delegate_provider_);
  }
  delegate_provider_ = delegate_provider;
}

}  // namespace device
