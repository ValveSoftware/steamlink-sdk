// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_time_tracker.h"

#include <stddef.h>
#include <stdint.h>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const base::TimeDelta kRequestDelta = base::TimeDelta::FromMilliseconds(100);
const base::TimeDelta kTinyDelay = base::TimeDelta::FromMilliseconds(1);
const base::TimeDelta kModerateDelay = base::TimeDelta::FromMilliseconds(25);
const base::TimeDelta kExcessiveDelay = base::TimeDelta::FromMilliseconds(75);

class ExtensionWebRequestTimeTrackerDelegateMock
    : public ExtensionWebRequestTimeTrackerDelegate {
 public:
  MOCK_METHOD4(NotifyExcessiveDelays,
      void (void*, size_t, size_t, const std::set<std::string>&));
  MOCK_METHOD4(NotifyModerateDelays,
      void (void*, size_t, size_t, const std::set<std::string>&));
};

}  // namespace

//class ExtensionWebRequestTimeTrackerTest : public testing::Test {};

TEST(ExtensionWebRequestTimeTrackerTest, Basic) {
  ExtensionWebRequestTimeTracker tracker;
  base::Time start;
  void* profile = NULL;

  tracker.LogRequestStartTime(42, start, GURL(), profile);
  EXPECT_EQ(1u, tracker.request_time_logs_.size());
  ASSERT_EQ(1u, tracker.request_ids_.size());
  EXPECT_EQ(42, tracker.request_ids_.front());
  tracker.LogRequestEndTime(42, start + kRequestDelta);
  EXPECT_EQ(1u, tracker.request_time_logs_.size());
  EXPECT_EQ(0u, tracker.moderate_delays_.size());
  EXPECT_EQ(0u, tracker.excessive_delays_.size());
}

TEST(ExtensionWebRequestTimeTrackerTest, CancelOrRedirect) {
  ExtensionWebRequestTimeTracker tracker;
  base::Time start;
  void* profile = NULL;

  tracker.LogRequestStartTime(1, start, GURL(), profile);
  EXPECT_EQ(1u, tracker.request_time_logs_.size());
  tracker.SetRequestCanceled(1);
  tracker.LogRequestEndTime(1, start + kRequestDelta);
  EXPECT_EQ(0u, tracker.request_time_logs_.size());

  tracker.LogRequestStartTime(2, start, GURL(), profile);
  EXPECT_EQ(1u, tracker.request_time_logs_.size());
  tracker.SetRequestRedirected(2);
  tracker.LogRequestEndTime(2, start + kRequestDelta);
  EXPECT_EQ(0u, tracker.request_time_logs_.size());
}

TEST(ExtensionWebRequestTimeTrackerTest, Delays) {
  ExtensionWebRequestTimeTracker tracker;
  base::Time start;
  std::string extension1_id("1");
  std::string extension2_id("2");
  void* profile = NULL;

  // Start 3 requests with different amounts of delay from 2 extensions.
  tracker.LogRequestStartTime(1, start, GURL(), profile);
  tracker.LogRequestStartTime(2, start, GURL(), profile);
  tracker.LogRequestStartTime(3, start, GURL(), profile);
  tracker.IncrementExtensionBlockTime(extension1_id, 1, kTinyDelay);
  tracker.IncrementExtensionBlockTime(extension1_id, 2, kTinyDelay);
  tracker.IncrementExtensionBlockTime(extension1_id, 3, kTinyDelay);
  tracker.IncrementExtensionBlockTime(extension2_id, 2, kModerateDelay);
  tracker.IncrementExtensionBlockTime(extension2_id, 3, kExcessiveDelay);
  tracker.IncrementTotalBlockTime(1, kTinyDelay);
  tracker.IncrementTotalBlockTime(2, kModerateDelay);
  tracker.IncrementTotalBlockTime(3, kExcessiveDelay);
  tracker.LogRequestEndTime(1, start + kRequestDelta);
  tracker.LogRequestEndTime(2, start + kRequestDelta);
  tracker.LogRequestEndTime(3, start + kRequestDelta);
  EXPECT_EQ(3u, tracker.request_time_logs_.size());
  EXPECT_EQ(1u, tracker.moderate_delays_.size());
  EXPECT_EQ(1u, tracker.moderate_delays_.count(2));
  EXPECT_EQ(1u, tracker.excessive_delays_.size());
  EXPECT_EQ(1u, tracker.excessive_delays_.count(3));

  // Now issue a bunch more requests and ensure that the old delays are
  // forgotten.
  for (int64_t i = 4; i < 500; ++i) {
    tracker.LogRequestStartTime(i, start, GURL(), profile);
    tracker.LogRequestEndTime(i, start + kRequestDelta);
  }
  EXPECT_EQ(0u, tracker.moderate_delays_.size());
  EXPECT_EQ(0u, tracker.excessive_delays_.size());
}

TEST(ExtensionWebRequestTimeTrackerTest, Delegate) {
  using testing::Mock;

  ExtensionWebRequestTimeTrackerDelegateMock* delegate(
      new ExtensionWebRequestTimeTrackerDelegateMock);
  ExtensionWebRequestTimeTracker tracker;
  tracker.SetDelegate(delegate);
  base::Time start;
  std::string extension1_id("1");
  void* profile = NULL;
  // Set of all extensions that blocked network requests.
  std::set<std::string> extensions;
  extensions.insert(extension1_id);

  const int num_moderate_delays = 51;
  const int num_excessive_delays = 11;
  int request_nr = 0;

  // Check that (only) the last moderate delay triggers the delegate callback.
  for (int64_t i = 0; i < num_moderate_delays; ++i) {
    request_nr++;
    if (i == num_moderate_delays-1) {
      EXPECT_CALL(*delegate,
                  NotifyModerateDelays(profile , i+1, request_nr, extensions));
    }
    tracker.LogRequestStartTime(request_nr, start, GURL(), profile);
    tracker.IncrementExtensionBlockTime(extension1_id, request_nr,
                                        kModerateDelay);
    tracker.IncrementTotalBlockTime(request_nr, kModerateDelay);
    tracker.LogRequestEndTime(request_nr, start + kRequestDelta);
    Mock::VerifyAndClearExpectations(delegate);
  }

  // Check that (only) the last excessive delay triggers the delegate callback.
  for (int64_t i = 0; i < num_excessive_delays; ++i) {
    request_nr++;
    if (i == num_excessive_delays-1) {
      EXPECT_CALL(*delegate,
                  NotifyExcessiveDelays(profile, i+1, request_nr, extensions));
    }
    tracker.LogRequestStartTime(request_nr, start, GURL(), profile);
    tracker.IncrementExtensionBlockTime(extension1_id, request_nr,
                                        kExcessiveDelay);
    tracker.IncrementTotalBlockTime(request_nr, kExcessiveDelay);
    tracker.LogRequestEndTime(request_nr, start + kRequestDelta);
    Mock::VerifyAndClearExpectations(delegate);
  }
}
