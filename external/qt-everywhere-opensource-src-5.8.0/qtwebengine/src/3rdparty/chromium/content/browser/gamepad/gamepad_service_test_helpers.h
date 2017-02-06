// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GAMEPAD_GAMEPAD_SERVICE_TEST_HELPERS_H_
#define CONTENT_BROWSER_GAMEPAD_GAMEPAD_SERVICE_TEST_HELPERS_H_

#include "base/macros.h"
#include "device/gamepad/gamepad_test_helpers.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace content {

class GamepadService;

// Constructs a GamepadService with a mock data source. This bypasses the
// global singleton for the gamepad service.
class GamepadServiceTestConstructor : public device::GamepadTestHelper {
 public:
  explicit GamepadServiceTestConstructor(const blink::WebGamepads& test_data);
  ~GamepadServiceTestConstructor() override;

  GamepadService* gamepad_service() { return gamepad_service_; }
  device::MockGamepadDataFetcher* data_fetcher() { return data_fetcher_; }

 private:
  // Owning pointer (can't be a scoped_ptr due to private destructor).
  GamepadService* gamepad_service_;

  // Pointer owned by the provider (which is owned by the gamepad service).
  device::MockGamepadDataFetcher* data_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(GamepadServiceTestConstructor);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GAMEPAD_GAMEPAD_SERVICE_TEST_HELPERS_H_
