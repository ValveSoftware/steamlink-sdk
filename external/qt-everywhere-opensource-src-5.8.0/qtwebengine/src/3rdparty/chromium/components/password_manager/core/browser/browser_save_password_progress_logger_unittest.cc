// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"

#include "components/password_manager/core/browser/stub_log_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

const char kTestText[] = "test";

// The only purpose of TestLogger is to expose SendLog for the test.
class TestLogger : public BrowserSavePasswordProgressLogger {
 public:
  explicit TestLogger(LogManager* log_manager)
      : BrowserSavePasswordProgressLogger(log_manager) {}

  using BrowserSavePasswordProgressLogger::SendLog;
};

class MockLogManager : public StubLogManager {
 public:
  MOCK_CONST_METHOD1(LogSavePasswordProgress, void(const std::string& text));
};

}  // namespace

TEST(BrowserSavePasswordProgressLoggerTest, SendLog) {
  MockLogManager log_manager;
  TestLogger logger(&log_manager);
  EXPECT_CALL(log_manager, LogSavePasswordProgress(kTestText));
  logger.SendLog(kTestText);
}

}  // namespace password_manager
