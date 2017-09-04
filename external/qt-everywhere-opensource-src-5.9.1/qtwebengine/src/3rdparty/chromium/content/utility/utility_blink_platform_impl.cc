// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_blink_platform_impl.h"

#include "third_party/WebKit/public/platform/scheduler/utility/webthread_impl_for_utility_thread.h"

namespace content {

UtilityBlinkPlatformImpl::UtilityBlinkPlatformImpl()
    : main_thread_(new blink::scheduler::WebThreadImplForUtilityThread()) {}

UtilityBlinkPlatformImpl::~UtilityBlinkPlatformImpl() {
}

blink::WebThread* UtilityBlinkPlatformImpl::currentThread() {
  if (main_thread_->isCurrentThread())
    return main_thread_.get();
  return BlinkPlatformImpl::currentThread();
}

}  // namespace content
