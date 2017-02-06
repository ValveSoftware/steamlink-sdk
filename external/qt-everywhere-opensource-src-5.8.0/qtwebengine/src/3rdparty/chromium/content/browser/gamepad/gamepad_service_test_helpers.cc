// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gamepad/gamepad_service_test_helpers.h"

#include "content/browser/gamepad/gamepad_service.h"

namespace content {

GamepadServiceTestConstructor::GamepadServiceTestConstructor(
    const blink::WebGamepads& test_data) {
  data_fetcher_ = new device::MockGamepadDataFetcher(test_data);
  gamepad_service_ = new GamepadService(
      std::unique_ptr<device::GamepadDataFetcher>(data_fetcher_));
}

GamepadServiceTestConstructor::~GamepadServiceTestConstructor() {
  delete gamepad_service_;
}

}  // namespace content
