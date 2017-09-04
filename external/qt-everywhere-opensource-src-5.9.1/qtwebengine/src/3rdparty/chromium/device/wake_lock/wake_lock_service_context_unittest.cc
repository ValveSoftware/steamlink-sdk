// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/wake_lock/wake_lock_service_context.h"

#include <memory>

#include "base/message_loop/message_loop.h"
#include "base/process/kill.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class WakeLockServiceContextTest : public testing::Test {
 public:
  WakeLockServiceContextTest()
      : wake_lock_service_context_(
            base::ThreadTaskRunnerHandle::Get(),
            base::Bind(&WakeLockServiceContextTest::GetNativeView,
                       base::Unretained(this))) {}

 protected:
  void RequestWakeLock() { GetWakeLockServiceContext()->RequestWakeLock(); }

  void CancelWakeLock() { GetWakeLockServiceContext()->CancelWakeLock(); }

  WakeLockServiceContext* GetWakeLockServiceContext() {
    return &wake_lock_service_context_;
  }

  bool HasWakeLock() {
    return GetWakeLockServiceContext()->HasWakeLockForTests();
  }

 private:
  gfx::NativeView GetNativeView() { return nullptr; }

  base::MessageLoop message_loop_;
  WakeLockServiceContext wake_lock_service_context_;
};

TEST_F(WakeLockServiceContextTest, NoLockInitially) {
  EXPECT_FALSE(HasWakeLock());
}

TEST_F(WakeLockServiceContextTest, LockUnlock) {
  ASSERT_TRUE(GetWakeLockServiceContext());

  // Request wake lock.
  RequestWakeLock();

  // Should set the blocker.
  EXPECT_TRUE(HasWakeLock());

  // Remove wake lock request.
  CancelWakeLock();

  // Should remove the blocker.
  EXPECT_FALSE(HasWakeLock());
}

}  // namespace device
