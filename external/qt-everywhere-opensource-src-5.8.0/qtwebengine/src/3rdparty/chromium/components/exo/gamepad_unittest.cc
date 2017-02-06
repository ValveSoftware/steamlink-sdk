// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "base/command_line.h"
#include "base/test/test_simple_task_runner.h"
#include "components/exo/buffer.h"
#include "components/exo/gamepad.h"
#include "components/exo/gamepad_delegate.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "device/gamepad/gamepad_test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/focus_client.h"

namespace exo {
namespace {

class MockGamepadDelegate : public GamepadDelegate {
 public:
  MockGamepadDelegate() {}

  // Overridden from GamepadDelegate:
  MOCK_METHOD1(OnGamepadDestroying, void(Gamepad*));
  MOCK_CONST_METHOD1(CanAcceptGamepadEventsForSurface, bool(Surface*));
  MOCK_METHOD1(OnStateChange, void(bool));
  MOCK_METHOD2(OnAxis, void(int, double));
  MOCK_METHOD3(OnButton, void(int, bool, double));
  MOCK_METHOD0(OnFrame, void());
};

class GamepadTest : public test::ExoTestBase {
 public:
  GamepadTest() {}

  std::unique_ptr<device::GamepadDataFetcher> MockDataFetcherFactory() {
    blink::WebGamepads initial_data;
    std::unique_ptr<device::MockGamepadDataFetcher> fetcher(
        new device::MockGamepadDataFetcher(initial_data));
    mock_data_fetcher_ = fetcher.get();
    return std::move(fetcher);
  }

  void InitializeGamepad(MockGamepadDelegate* delegate) {
    polling_task_runner_ = new base::TestSimpleTaskRunner();
    gamepad_.reset(new Gamepad(delegate, polling_task_runner_.get(),
                               base::Bind(&GamepadTest::MockDataFetcherFactory,
                                          base::Unretained(this))));
    // Run the polling task runner to have it create the data fetcher.
    polling_task_runner_->RunPendingTasks();
  }

  void DestroyGamepad(MockGamepadDelegate* delegate) {
    EXPECT_CALL(*delegate, OnGamepadDestroying(testing::_)).Times(1);
    mock_data_fetcher_ = nullptr;
    gamepad_.reset();
    // Process tasks until polling is shut down.
    polling_task_runner_->RunPendingTasks();
    polling_task_runner_ = nullptr;
  }

  void SetDataAndPostToDelegate(const blink::WebGamepads& new_data) {
    ASSERT_TRUE(mock_data_fetcher_ != nullptr);
    mock_data_fetcher_->SetTestData(new_data);
    // Run one polling cycle, which will post a task to the origin task runner.
    polling_task_runner_->RunPendingTasks();
    // Run origin task runner to invoke delegate.
    base::MessageLoop::current()->RunUntilIdle();
  }

 protected:
  std::unique_ptr<Gamepad> gamepad_;

  // Task runner to simulate the polling thread.
  scoped_refptr<base::TestSimpleTaskRunner> polling_task_runner_;

  // Weak reference to the mock data fetcher provided by MockDataFetcherFactory.
  // This instance is valid until both gamepad_ and polling_task_runner_ are
  // shut down.
  device::MockGamepadDataFetcher* mock_data_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(GamepadTest);
};

TEST_F(GamepadTest, OnStateChange) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  testing::StrictMock<MockGamepadDelegate> delegate;
  EXPECT_CALL(delegate, CanAcceptGamepadEventsForSurface(testing::_))
      .WillOnce(testing::Return(true));

  InitializeGamepad(&delegate);

  // Gamepad connected.
  EXPECT_CALL(delegate, OnStateChange(true)).Times(1);
  blink::WebGamepads gamepad_connected;
  gamepad_connected.length = 1;
  gamepad_connected.items[0].connected = true;
  gamepad_connected.items[0].timestamp = 1;
  SetDataAndPostToDelegate(gamepad_connected);

  // Gamepad disconnected.
  blink::WebGamepads all_disconnected;
  EXPECT_CALL(delegate, OnStateChange(false)).Times(1);
  SetDataAndPostToDelegate(all_disconnected);

  DestroyGamepad(&delegate);
}

TEST_F(GamepadTest, OnAxis) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  testing::StrictMock<MockGamepadDelegate> delegate;
  EXPECT_CALL(delegate, CanAcceptGamepadEventsForSurface(testing::_))
      .WillOnce(testing::Return(true));

  InitializeGamepad(&delegate);

  blink::WebGamepads axis_moved;
  axis_moved.length = 1;
  axis_moved.items[0].connected = true;
  axis_moved.items[0].timestamp = 1;
  axis_moved.items[0].axesLength = 1;
  axis_moved.items[0].axes[0] = 1.0;

  EXPECT_CALL(delegate, OnStateChange(true)).Times(1);
  EXPECT_CALL(delegate, OnAxis(0, 1.0)).Times(1);
  EXPECT_CALL(delegate, OnFrame()).Times(1);
  SetDataAndPostToDelegate(axis_moved);

  DestroyGamepad(&delegate);
}

TEST_F(GamepadTest, OnButton) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  testing::StrictMock<MockGamepadDelegate> delegate;
  EXPECT_CALL(delegate, CanAcceptGamepadEventsForSurface(testing::_))
      .WillOnce(testing::Return(true));

  InitializeGamepad(&delegate);

  blink::WebGamepads axis_moved;
  axis_moved.length = 1;
  axis_moved.items[0].connected = true;
  axis_moved.items[0].timestamp = 1;
  axis_moved.items[0].buttonsLength = 1;
  axis_moved.items[0].buttons[0].pressed = true;
  axis_moved.items[0].buttons[0].value = 1.0;

  EXPECT_CALL(delegate, OnStateChange(true)).Times(1);
  EXPECT_CALL(delegate, OnButton(0, true, 1.0)).Times(1);
  EXPECT_CALL(delegate, OnFrame()).Times(1);
  SetDataAndPostToDelegate(axis_moved);

  DestroyGamepad(&delegate);
}

TEST_F(GamepadTest, OnWindowFocused) {
  // Create surface and move focus to it.
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  testing::StrictMock<MockGamepadDelegate> delegate;
  EXPECT_CALL(delegate, CanAcceptGamepadEventsForSurface(testing::_))
      .WillOnce(testing::Return(true));

  InitializeGamepad(&delegate);

  // In focus. Should be polling indefinitely, check a couple of time that the
  // poll task is re-posted.
  for (size_t i = 0; i < 5; ++i) {
    polling_task_runner_->RunPendingTasks();
    ASSERT_TRUE(polling_task_runner_->HasPendingTask());
  }

  // Remove focus from window.
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  // Run EnablePolling and OnPoll task, no more polls should be scheduled.
  // In the first round of RunPendingTasks we will execute
  // EnablePollingOnPollingThread, which will cause the polling to stop being
  // scheduled in the next round.
  polling_task_runner_->RunPendingTasks();
  polling_task_runner_->RunPendingTasks();
  ASSERT_FALSE(polling_task_runner_->HasPendingTask());

  DestroyGamepad(&delegate);
}

}  // namespace
}  // namespace exo
