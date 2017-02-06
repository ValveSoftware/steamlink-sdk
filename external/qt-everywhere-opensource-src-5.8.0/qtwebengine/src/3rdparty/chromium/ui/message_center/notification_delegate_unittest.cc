// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notification_delegate.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace message_center {

class NotificationDelegateTest : public testing::Test {
 public:
  NotificationDelegateTest();
  ~NotificationDelegateTest() override;

  void ClickCallback();

  int GetClickedCallbackAndReset();

 private:
  int callback_count_;

  DISALLOW_COPY_AND_ASSIGN(NotificationDelegateTest);
};

NotificationDelegateTest::NotificationDelegateTest() : callback_count_(0) {}

NotificationDelegateTest::~NotificationDelegateTest() {}

void NotificationDelegateTest::ClickCallback() {
  ++callback_count_;
}

int NotificationDelegateTest::GetClickedCallbackAndReset() {
  int result = callback_count_;
  callback_count_ = 0;
  return result;
}

TEST_F(NotificationDelegateTest, ClickedDelegate) {
  scoped_refptr<HandleNotificationClickedDelegate> delegate(
      new HandleNotificationClickedDelegate(
          base::Bind(&NotificationDelegateTest::ClickCallback,
                     base::Unretained(this))));

  EXPECT_TRUE(delegate->HasClickedListener());
  delegate->Click();
  EXPECT_EQ(1, GetClickedCallbackAndReset());

  // ButtonClick doesn't call the callback.
  delegate->ButtonClick(0);
  EXPECT_EQ(0, GetClickedCallbackAndReset());
}

TEST_F(NotificationDelegateTest, NullClickedDelegate) {
  scoped_refptr<HandleNotificationClickedDelegate> delegate(
      new HandleNotificationClickedDelegate(base::Closure()));

  EXPECT_FALSE(delegate->HasClickedListener());
  delegate->Click();
  EXPECT_EQ(0, GetClickedCallbackAndReset());

  delegate->ButtonClick(0);
  EXPECT_EQ(0, GetClickedCallbackAndReset());
}

}
