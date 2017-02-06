// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_platform_data_fetcher.h"

#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace device {

GamepadDataFetcherEmpty::GamepadDataFetcherEmpty() {}

void GamepadDataFetcherEmpty::GetGamepadData(blink::WebGamepads* pads,
                                             bool devices_changed_hint) {
  pads->length = 0;
}

}  // namespace device
