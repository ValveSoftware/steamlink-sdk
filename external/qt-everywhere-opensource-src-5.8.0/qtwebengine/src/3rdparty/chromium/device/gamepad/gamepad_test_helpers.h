// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAMEPAD_TEST_HELPERS_H_
#define DEVICE_GAMEPAD_GAMEPAD_TEST_HELPERS_H_

#include <memory>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_shared_buffer.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace device {

class GamepadService;

// Data fetcher that returns canned data for the gamepad provider.
class MockGamepadDataFetcher : public GamepadDataFetcher {
 public:
  // Initializes the fetcher with the given gamepad data, which will be
  // returned when the provider queries us.
  explicit MockGamepadDataFetcher(const blink::WebGamepads& test_data);

  ~MockGamepadDataFetcher() override;

  // GamepadDataFetcher.
  void GetGamepadData(blink::WebGamepads* pads,
                      bool devices_changed_hint) override;

  // Blocks the current thread until the GamepadProvider reads from this
  // fetcher on the background thread.
  void WaitForDataRead();

  // Blocks the current thread until the GamepadProvider reads from this
  // fetcher on the background thread and issued all callbacks related to the
  // read on the client's thread.
  void WaitForDataReadAndCallbacksIssued();

  // Updates the test data.
  void SetTestData(const blink::WebGamepads& new_data);

 private:
  base::Lock lock_;
  blink::WebGamepads test_data_;
  base::WaitableEvent read_data_;

  DISALLOW_COPY_AND_ASSIGN(MockGamepadDataFetcher);
};

class MockGamepadSharedBuffer : public GamepadSharedBuffer {
 public:
  MockGamepadSharedBuffer();

  base::SharedMemory* shared_memory() override;
  blink::WebGamepads* buffer() override;

  void WriteBegin() override;
  void WriteEnd() override;

 private:
  base::SharedMemory shared_memory_;

  DISALLOW_COPY_AND_ASSIGN(MockGamepadSharedBuffer);
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

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_TEST_HELPERS_H_
