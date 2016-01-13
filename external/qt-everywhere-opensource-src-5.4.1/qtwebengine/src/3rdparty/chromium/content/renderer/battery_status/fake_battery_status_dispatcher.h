// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BATTERY_STATUS_FAKE_BATTERY_STATUS_DISPATCHER_H_
#define CONTENT_RENDERER_BATTERY_STATUS_FAKE_BATTERY_STATUS_DISPATCHER_H_

#include "base/macros.h"

namespace blink {
class WebBatteryStatus;
class WebBatteryStatusListener;
}

namespace content {

class FakeBatteryStatusDispatcher {
 public:
  FakeBatteryStatusDispatcher();

  void SetListener(blink::WebBatteryStatusListener* listener);
  void PostBatteryStatusChange(const blink::WebBatteryStatus& status);

 private:
  blink::WebBatteryStatusListener* listener_;

  DISALLOW_COPY_AND_ASSIGN(FakeBatteryStatusDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_BATTERY_STATUS_FAKE_BATTERY_STATUS_DISPATCHER_H_
