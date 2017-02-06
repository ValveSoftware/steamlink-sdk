// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_coordinator_event_logger.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

const char kNamespace[] = "last_n";
const char kStatus[] = "pending";
const int64_t kId = 1234;
const char kLogString[] =
    "Save page request for ID: 1234 and namespace: "
    "last_n has been updated with status pending";
const int kTimeLength = 21;

}  // namespace

TEST(RequestCoordinatorEventLoggerTest, RecordsWhenLoggingIsOn) {
  RequestCoordinatorEventLogger logger;
  std::vector<std::string> log;

  logger.SetIsLogging(true);
  logger.RecordSavePageRequestUpdated(kNamespace, kStatus, kId);
  logger.GetLogs(&log);

  EXPECT_EQ(1u, log.size());
  EXPECT_EQ(std::string(kLogString), log[0].substr(kTimeLength));
}

TEST(RequestCoordinatorEventLoggerTest, RecordsWhenLoggingIsOff) {
  RequestCoordinatorEventLogger logger;
  std::vector<std::string> log;

  logger.SetIsLogging(false);
  logger.RecordSavePageRequestUpdated(kNamespace, kStatus, kId);
  logger.GetLogs(&log);

  EXPECT_EQ(0u, log.size());
}

TEST(RequestCoordinatorEventLoggerTest, DoesNotExceedMaxSize) {
  RequestCoordinatorEventLogger logger;
  std::vector<std::string> log;

  logger.SetIsLogging(true);
  for (size_t i = 0; i < kMaxLogCount + 1; ++i) {
    logger.RecordSavePageRequestUpdated(kNamespace, kStatus, kId);
  }
  logger.GetLogs(&log);

  EXPECT_EQ(kMaxLogCount, log.size());
}

}  // namespace offline_pages
