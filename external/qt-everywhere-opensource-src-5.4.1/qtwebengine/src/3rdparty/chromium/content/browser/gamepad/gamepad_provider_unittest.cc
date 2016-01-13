// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/gamepad/gamepad_data_fetcher.h"
#include "content/browser/gamepad/gamepad_provider.h"
#include "content/browser/gamepad/gamepad_test_helpers.h"
#include "content/common/gamepad_hardware_buffer.h"
#include "content/common/gamepad_messages.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using blink::WebGamepads;

// Helper class to generate and record user gesture callbacks.
class UserGestureListener {
 public:
  UserGestureListener()
      : weak_factory_(this),
        has_user_gesture_(false) {
  }

  base::Closure GetClosure() {
    return base::Bind(&UserGestureListener::GotUserGesture,
                      weak_factory_.GetWeakPtr());
  }

  bool has_user_gesture() const { return has_user_gesture_; }

 private:
  void GotUserGesture() {
    has_user_gesture_ = true;
  }

  base::WeakPtrFactory<UserGestureListener> weak_factory_;
  bool has_user_gesture_;
};

// Main test fixture
class GamepadProviderTest : public testing::Test, public GamepadTestHelper {
 public:
  GamepadProvider* CreateProvider(const WebGamepads& test_data) {
    mock_data_fetcher_ = new MockGamepadDataFetcher(test_data);
    provider_.reset(new GamepadProvider(
        scoped_ptr<GamepadDataFetcher>(mock_data_fetcher_)));
    return provider_.get();
  }

 protected:
  GamepadProviderTest() {
  }

  scoped_ptr<GamepadProvider> provider_;

  // Pointer owned by the provider.
  MockGamepadDataFetcher* mock_data_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(GamepadProviderTest);
};

// Crashes. http://crbug.com/106163
TEST_F(GamepadProviderTest, PollingAccess) {
  WebGamepads test_data;
  test_data.length = 1;
  test_data.items[0].connected = true;
  test_data.items[0].timestamp = 0;
  test_data.items[0].buttonsLength = 1;
  test_data.items[0].axesLength = 2;
  test_data.items[0].buttons[0].value = 1.f;
  test_data.items[0].buttons[0].pressed = true;
  test_data.items[0].axes[0] = -1.f;
  test_data.items[0].axes[1] = .5f;

  GamepadProvider* provider = CreateProvider(test_data);
  provider->Resume();

  message_loop().RunUntilIdle();

  mock_data_fetcher_->WaitForDataRead();

  // Renderer-side, pull data out of poll buffer.
  base::SharedMemoryHandle handle = provider->GetSharedMemoryHandleForProcess(
      base::GetCurrentProcessHandle());
  scoped_ptr<base::SharedMemory> shared_memory(
      new base::SharedMemory(handle, true));
  EXPECT_TRUE(shared_memory->Map(sizeof(GamepadHardwareBuffer)));
  void* mem = shared_memory->memory();

  GamepadHardwareBuffer* hwbuf = static_cast<GamepadHardwareBuffer*>(mem);
  // See gamepad_hardware_buffer.h for details on the read discipline.
  WebGamepads output;

  base::subtle::Atomic32 version;
  do {
    version = hwbuf->sequence.ReadBegin();
    memcpy(&output, &hwbuf->buffer, sizeof(output));
  } while (hwbuf->sequence.ReadRetry(version));

  EXPECT_EQ(1u, output.length);
  EXPECT_EQ(1u, output.items[0].buttonsLength);
  EXPECT_EQ(1.f, output.items[0].buttons[0].value);
  EXPECT_EQ(true, output.items[0].buttons[0].pressed);
  EXPECT_EQ(2u, output.items[0].axesLength);
  EXPECT_EQ(-1.f, output.items[0].axes[0]);
  EXPECT_EQ(0.5f, output.items[0].axes[1]);
}

// Tests that waiting for a user gesture works properly.
TEST_F(GamepadProviderTest, UserGesture) {
  WebGamepads no_button_data;
  no_button_data.length = 1;
  no_button_data.items[0].connected = true;
  no_button_data.items[0].timestamp = 0;
  no_button_data.items[0].buttonsLength = 1;
  no_button_data.items[0].axesLength = 2;
  no_button_data.items[0].buttons[0].value = 0.f;
  no_button_data.items[0].buttons[0].pressed = false;
  no_button_data.items[0].axes[0] = -1.f;
  no_button_data.items[0].axes[1] = .5f;

  WebGamepads button_down_data = no_button_data;
  button_down_data.items[0].buttons[0].value = 1.f;
  button_down_data.items[0].buttons[0].pressed = true;

  UserGestureListener listener;
  GamepadProvider* provider = CreateProvider(no_button_data);
  provider->Resume();

  // Register for a user gesture and make sure the provider reads it twice
  // see below for why).
  provider->RegisterForUserGesture(listener.GetClosure());
  mock_data_fetcher_->WaitForDataRead();
  mock_data_fetcher_->WaitForDataRead();

  // It should not have issued our callback.
  message_loop().RunUntilIdle();
  EXPECT_FALSE(listener.has_user_gesture());

  // Set a button down and wait for it to be read twice.
  //
  // We wait for two reads before calling RunAllPending because the provider
  // will read the data on the background thread (setting the event) and *then*
  // will issue the callback on our thread. Waiting for it to read twice
  // ensures that it was able to issue callbacks for the first read (if it
  // issued one) before we try to check for it.
  mock_data_fetcher_->SetTestData(button_down_data);
  mock_data_fetcher_->WaitForDataRead();
  mock_data_fetcher_->WaitForDataRead();

  // It should have issued our callback.
  message_loop().RunUntilIdle();
  EXPECT_TRUE(listener.has_user_gesture());
}

}  // namespace

}  // namespace content
