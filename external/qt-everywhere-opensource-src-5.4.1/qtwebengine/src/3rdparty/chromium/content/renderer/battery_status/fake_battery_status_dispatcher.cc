// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_battery_status_dispatcher.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "third_party/WebKit/public/platform/WebBatteryStatusListener.h"

namespace content {

FakeBatteryStatusDispatcher::FakeBatteryStatusDispatcher() : listener_(0) {
}

void FakeBatteryStatusDispatcher::SetListener(
    blink::WebBatteryStatusListener* listener) {
  listener_ = listener;
}

void FakeBatteryStatusDispatcher::PostBatteryStatusChange(
    const blink::WebBatteryStatus& status) {
  if (!listener_)
    return;

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&blink::WebBatteryStatusListener::updateBatteryStatus,
                 base::Unretained(listener_),
                 status));
}

}  // namespace content
