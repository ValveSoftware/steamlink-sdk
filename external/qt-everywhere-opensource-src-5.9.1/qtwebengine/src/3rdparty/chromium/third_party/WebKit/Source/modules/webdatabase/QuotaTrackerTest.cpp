// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webdatabase/QuotaTracker.h"

#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/RefPtr.h"

namespace blink {
namespace {

TEST(QuotaTrackerTest, UpdateAndGetSizeAndSpaceAvailable) {
  QuotaTracker& tracker = QuotaTracker::instance();
  RefPtr<SecurityOrigin> origin =
      SecurityOrigin::createFromString("file:///a/b/c");

  const unsigned long long spaceAvailable = 12345678ULL;
  tracker.updateSpaceAvailableToOrigin(origin.get(), spaceAvailable);

  const String databaseName = "db";
  const unsigned long long databaseSize = 1234ULL;
  tracker.updateDatabaseSize(origin.get(), databaseName, databaseSize);

  unsigned long long used = 0;
  unsigned long long available = 0;
  tracker.getDatabaseSizeAndSpaceAvailableToOrigin(origin.get(), databaseName,
                                                   &used, &available);

  EXPECT_EQ(used, databaseSize);
  EXPECT_EQ(available, spaceAvailable);
}

TEST(QuotaTrackerTest, LocalAccessBlocked) {
  QuotaTracker& tracker = QuotaTracker::instance();
  RefPtr<SecurityOrigin> origin =
      SecurityOrigin::createFromString("file:///a/b/c");

  const unsigned long long spaceAvailable = 12345678ULL;
  tracker.updateSpaceAvailableToOrigin(origin.get(), spaceAvailable);

  const String databaseName = "db";
  const unsigned long long databaseSize = 1234ULL;
  tracker.updateDatabaseSize(origin.get(), databaseName, databaseSize);

  // QuotaTracker should not care about policy, just identity.
  origin->blockLocalAccessFromLocalOrigin();

  unsigned long long used = 0;
  unsigned long long available = 0;
  tracker.getDatabaseSizeAndSpaceAvailableToOrigin(origin.get(), databaseName,
                                                   &used, &available);

  EXPECT_EQ(used, databaseSize);
  EXPECT_EQ(available, spaceAvailable);
}

}  // namespace
}  // namespace blink
