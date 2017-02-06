// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gamepad/gamepad_service.h"

#include <string.h>

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "device/gamepad/gamepad_consumer.h"
#include "device/gamepad/gamepad_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
static const int kNumberOfGamepads = blink::WebGamepads::itemsLengthCap;
}

using blink::WebGamepads;

class ConnectionListener : public device::GamepadConsumer {
 public:
  ConnectionListener() {
    ClearCounters();
  }

  void OnGamepadConnected(unsigned index,
                          const blink::WebGamepad& gamepad) override {
    connected_counter_++;
  }
  void OnGamepadDisconnected(unsigned index,
                             const blink::WebGamepad& gamepad) override {
    disconnected_counter_++;
  }

  void ClearCounters() {
    connected_counter_ = 0;
    disconnected_counter_ = 0;
  }

  int connected_counter() const { return connected_counter_; }
  int disconnected_counter() const { return disconnected_counter_; }

 private:
  int connected_counter_;
  int disconnected_counter_;
};

class GamepadServiceTest : public testing::Test {
 protected:
  GamepadServiceTest();
  ~GamepadServiceTest() override;

  void SetPadsConnected(bool connected);
  void WaitForData();

  int GetConnectedCounter() const {
    return connection_listener_->connected_counter();
  }
  int GetDisconnectedCounter() const {
    return connection_listener_->disconnected_counter();
  }

  void SetUp() override;

 private:
  device::MockGamepadDataFetcher* fetcher_;
  GamepadService* service_;
  std::unique_ptr<ConnectionListener> connection_listener_;
  TestBrowserThreadBundle browser_thread_;
  WebGamepads test_data_;

  DISALLOW_COPY_AND_ASSIGN(GamepadServiceTest);
};

GamepadServiceTest::GamepadServiceTest()
    : browser_thread_(TestBrowserThreadBundle::IO_MAINLOOP) {
  memset(&test_data_, 0, sizeof(test_data_));

  // Set it so that we have user gesture.
  test_data_.items[0].buttonsLength = 1;
  test_data_.items[0].buttons[0].value = 1.f;
  test_data_.items[0].buttons[0].pressed = true;
}

GamepadServiceTest::~GamepadServiceTest() {
  delete service_;
}

void GamepadServiceTest::SetUp() {
  fetcher_ = new device::MockGamepadDataFetcher(test_data_);
  service_ = new GamepadService(
      std::unique_ptr<device::GamepadDataFetcher>(fetcher_));
  connection_listener_.reset((new ConnectionListener));
  service_->ConsumerBecameActive(connection_listener_.get());
}

void GamepadServiceTest::SetPadsConnected(bool connected) {
  for (int i = 0; i < kNumberOfGamepads; ++i) {
    test_data_.items[i].connected = connected;
  }
  fetcher_->SetTestData(test_data_);
}

void GamepadServiceTest::WaitForData() {
  connection_listener_->ClearCounters();
  fetcher_->WaitForDataReadAndCallbacksIssued();
  base::RunLoop().RunUntilIdle();
}

TEST_F(GamepadServiceTest, ConnectionsTest) {
  WaitForData();
  EXPECT_EQ(0, GetConnectedCounter());
  EXPECT_EQ(0, GetDisconnectedCounter());

  SetPadsConnected(true);
  WaitForData();
  EXPECT_EQ(kNumberOfGamepads, GetConnectedCounter());
  EXPECT_EQ(0, GetDisconnectedCounter());

  SetPadsConnected(false);
  WaitForData();
  EXPECT_EQ(0, GetConnectedCounter());
  EXPECT_EQ(kNumberOfGamepads, GetDisconnectedCounter());

  WaitForData();
  EXPECT_EQ(0, GetConnectedCounter());
  EXPECT_EQ(0, GetDisconnectedCounter());
}

}  // namespace content
