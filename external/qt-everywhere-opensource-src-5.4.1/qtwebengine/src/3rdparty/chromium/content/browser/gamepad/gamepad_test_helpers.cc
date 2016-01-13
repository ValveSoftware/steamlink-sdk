// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gamepad/gamepad_test_helpers.h"

#include "content/browser/gamepad/gamepad_service.h"

namespace content {

MockGamepadDataFetcher::MockGamepadDataFetcher(
    const blink::WebGamepads& test_data)
    : test_data_(test_data),
      read_data_(false, false) {
}

MockGamepadDataFetcher::~MockGamepadDataFetcher() {
}

void MockGamepadDataFetcher::GetGamepadData(blink::WebGamepads* pads,
                                            bool devices_changed_hint) {
  {
    base::AutoLock lock(lock_);
    *pads = test_data_;
  }
  read_data_.Signal();
}

void MockGamepadDataFetcher::WaitForDataRead() {
  return read_data_.Wait();
}

void MockGamepadDataFetcher::SetTestData(const blink::WebGamepads& new_data) {
  base::AutoLock lock(lock_);
  test_data_ = new_data;
}

GamepadTestHelper::GamepadTestHelper() {
}

GamepadTestHelper::~GamepadTestHelper() {
}

GamepadServiceTestConstructor::GamepadServiceTestConstructor(
    const blink::WebGamepads& test_data) {
  data_fetcher_ = new MockGamepadDataFetcher(test_data);
  gamepad_service_ =
      new GamepadService(scoped_ptr<GamepadDataFetcher>(data_fetcher_));
}

GamepadServiceTestConstructor::~GamepadServiceTestConstructor() {
  delete gamepad_service_;
}

}  // namespace content
