// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GAMEPAD_GAMEPAD_TEST_HELPERS_H_
#define CONTENT_BROWSER_GAMEPAD_GAMEPAD_TEST_HELPERS_H_

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "content/browser/gamepad/gamepad_data_fetcher.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace content {

class GamepadService;

// Data fetcher that returns canned data for the gamepad provider.
class MockGamepadDataFetcher : public GamepadDataFetcher {
 public:
  // Initializes the fetcher with the given gamepad data, which will be
  // returned when the provider queries us.
  explicit MockGamepadDataFetcher(const blink::WebGamepads& test_data);

  virtual ~MockGamepadDataFetcher();

  // GamepadDataFetcher.
  virtual void GetGamepadData(blink::WebGamepads* pads,
                              bool devices_changed_hint) OVERRIDE;

  // Blocks the current thread until the GamepadProvider reads from this
  // fetcher on the background thread.
  void WaitForDataRead();

  // Updates the test data.
  void SetTestData(const blink::WebGamepads& new_data);

 private:
  base::Lock lock_;
  blink::WebGamepads test_data_;
  base::WaitableEvent read_data_;

  DISALLOW_COPY_AND_ASSIGN(MockGamepadDataFetcher);
};

// Base class for the other test helpers. This just sets up the system monitor.
class GamepadTestHelper {
 public:
  GamepadTestHelper();
  virtual ~GamepadTestHelper();

  base::MessageLoop& message_loop() { return message_loop_; }

 private:
  // This must be constructed before the system monitor.
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(GamepadTestHelper);
};

// Constructs a GamepadService with a mock data source. This bypasses the
// global singleton for the gamepad service.
class GamepadServiceTestConstructor : public GamepadTestHelper {
 public:
  explicit GamepadServiceTestConstructor(const blink::WebGamepads& test_data);
  virtual ~GamepadServiceTestConstructor();

  GamepadService* gamepad_service() { return gamepad_service_; }
  MockGamepadDataFetcher* data_fetcher() { return data_fetcher_; }

 private:
  // Owning pointer (can't be a scoped_ptr due to private destructor).
  GamepadService* gamepad_service_;

  // Pointer owned by the provider (which is owned by the gamepad service).
  MockGamepadDataFetcher* data_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(GamepadServiceTestConstructor);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GAMEPAD_GAMEPAD_TEST_HELPERS_H_
