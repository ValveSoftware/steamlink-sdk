// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_throttler_manager.h"

#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_samples.h"
#include "base/pickle.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/statistics_delta_reader.h"
#include "base/time/time.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "net/url_request/url_request_throttler_header_interface.h"
#include "net/url_request/url_request_throttler_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using base::TimeTicks;

namespace net {

namespace {

const char kRequestThrottledHistogramName[] = "Throttling.RequestThrottled";

class MockURLRequestThrottlerEntry : public URLRequestThrottlerEntry {
 public:
  explicit MockURLRequestThrottlerEntry(
      net::URLRequestThrottlerManager* manager)
      : net::URLRequestThrottlerEntry(manager, std::string()),
        mock_backoff_entry_(&backoff_policy_) {
    InitPolicy();
  }
  MockURLRequestThrottlerEntry(
      net::URLRequestThrottlerManager* manager,
      const TimeTicks& exponential_backoff_release_time,
      const TimeTicks& sliding_window_release_time,
      const TimeTicks& fake_now)
      : net::URLRequestThrottlerEntry(manager, std::string()),
        fake_time_now_(fake_now),
        mock_backoff_entry_(&backoff_policy_) {
    InitPolicy();

    mock_backoff_entry_.set_fake_now(fake_now);
    set_exponential_backoff_release_time(exponential_backoff_release_time);
    set_sliding_window_release_time(sliding_window_release_time);
  }

  void InitPolicy() {
    // Some tests become flaky if we have jitter.
    backoff_policy_.jitter_factor = 0.0;

    // This lets us avoid having to make multiple failures initially (this
    // logic is already tested in the BackoffEntry unit tests).
    backoff_policy_.num_errors_to_ignore = 0;
  }

  virtual const BackoffEntry* GetBackoffEntry() const OVERRIDE {
    return &mock_backoff_entry_;
  }

  virtual BackoffEntry* GetBackoffEntry() OVERRIDE {
    return &mock_backoff_entry_;
  }

  static bool ExplicitUserRequest(int load_flags) {
    return URLRequestThrottlerEntry::ExplicitUserRequest(load_flags);
  }

  void ResetToBlank(const TimeTicks& time_now) {
    fake_time_now_ = time_now;
    mock_backoff_entry_.set_fake_now(time_now);

    GetBackoffEntry()->Reset();
    GetBackoffEntry()->SetCustomReleaseTime(time_now);
    set_sliding_window_release_time(time_now);
  }

  // Overridden for tests.
  virtual TimeTicks ImplGetTimeNow() const OVERRIDE { return fake_time_now_; }

  void set_exponential_backoff_release_time(
      const base::TimeTicks& release_time) {
    GetBackoffEntry()->SetCustomReleaseTime(release_time);
  }

  base::TimeTicks sliding_window_release_time() const {
    return URLRequestThrottlerEntry::sliding_window_release_time();
  }

  void set_sliding_window_release_time(
      const base::TimeTicks& release_time) {
    URLRequestThrottlerEntry::set_sliding_window_release_time(
        release_time);
  }

  TimeTicks fake_time_now_;
  MockBackoffEntry mock_backoff_entry_;

 protected:
  virtual ~MockURLRequestThrottlerEntry() {}
};

class MockURLRequestThrottlerManager : public URLRequestThrottlerManager {
 public:
  MockURLRequestThrottlerManager() : create_entry_index_(0) {}

  // Method to process the URL using URLRequestThrottlerManager protected
  // method.
  std::string DoGetUrlIdFromUrl(const GURL& url) { return GetIdFromUrl(url); }

  // Method to use the garbage collecting method of URLRequestThrottlerManager.
  void DoGarbageCollectEntries() { GarbageCollectEntries(); }

  // Returns the number of entries in the map.
  int GetNumberOfEntries() const { return GetNumberOfEntriesForTests(); }

  void CreateEntry(bool is_outdated) {
    TimeTicks time = TimeTicks::Now();
    if (is_outdated) {
      time -= TimeDelta::FromMilliseconds(
          MockURLRequestThrottlerEntry::kDefaultEntryLifetimeMs + 1000);
    }
    std::string fake_url_string("http://www.fakeurl.com/");
    fake_url_string.append(base::IntToString(create_entry_index_++));
    GURL fake_url(fake_url_string);
    OverrideEntryForTests(
        fake_url,
        new MockURLRequestThrottlerEntry(this, time, TimeTicks::Now(),
                                         TimeTicks::Now()));
  }

 private:
  int create_entry_index_;
};

struct TimeAndBool {
  TimeAndBool(const TimeTicks& time_value, bool expected, int line_num) {
    time = time_value;
    result = expected;
    line = line_num;
  }
  TimeTicks time;
  bool result;
  int line;
};

struct GurlAndString {
  GurlAndString(const GURL& url_value,
                const std::string& expected,
                int line_num) {
    url = url_value;
    result = expected;
    line = line_num;
  }
  GURL url;
  std::string result;
  int line;
};

}  // namespace

class URLRequestThrottlerEntryTest : public testing::Test {
 protected:
  URLRequestThrottlerEntryTest()
      : request_(GURL(), DEFAULT_PRIORITY, NULL, &context_) {}

  virtual void SetUp();

  TimeTicks now_;
  MockURLRequestThrottlerManager manager_;  // Dummy object, not used.
  scoped_refptr<MockURLRequestThrottlerEntry> entry_;

  TestURLRequestContext context_;
  TestURLRequest request_;
};

void URLRequestThrottlerEntryTest::SetUp() {
  request_.SetLoadFlags(0);

  now_ = TimeTicks::Now();
  entry_ = new MockURLRequestThrottlerEntry(&manager_);
  entry_->ResetToBlank(now_);
}

std::ostream& operator<<(std::ostream& out, const base::TimeTicks& time) {
  return out << time.ToInternalValue();
}

TEST_F(URLRequestThrottlerEntryTest, CanThrottleRequest) {
  TestNetworkDelegate d;
  context_.set_network_delegate(&d);
  entry_->set_exponential_backoff_release_time(
      entry_->fake_time_now_ + TimeDelta::FromMilliseconds(1));

  d.set_can_throttle_requests(false);
  EXPECT_FALSE(entry_->ShouldRejectRequest(request_));
  d.set_can_throttle_requests(true);
  EXPECT_TRUE(entry_->ShouldRejectRequest(request_));
}

TEST_F(URLRequestThrottlerEntryTest, InterfaceDuringExponentialBackoff) {
  base::StatisticsDeltaReader statistics_delta_reader;
  entry_->set_exponential_backoff_release_time(
      entry_->fake_time_now_ + TimeDelta::FromMilliseconds(1));
  EXPECT_TRUE(entry_->ShouldRejectRequest(request_));

  // Also end-to-end test the load flags exceptions.
  request_.SetLoadFlags(LOAD_MAYBE_USER_GESTURE);
  EXPECT_FALSE(entry_->ShouldRejectRequest(request_));

  scoped_ptr<base::HistogramSamples> samples(
      statistics_delta_reader.GetHistogramSamplesSinceCreation(
          kRequestThrottledHistogramName));
  ASSERT_EQ(1, samples->GetCount(0));
  ASSERT_EQ(1, samples->GetCount(1));
}

TEST_F(URLRequestThrottlerEntryTest, InterfaceNotDuringExponentialBackoff) {
  base::StatisticsDeltaReader statistics_delta_reader;
  entry_->set_exponential_backoff_release_time(entry_->fake_time_now_);
  EXPECT_FALSE(entry_->ShouldRejectRequest(request_));
  entry_->set_exponential_backoff_release_time(
      entry_->fake_time_now_ - TimeDelta::FromMilliseconds(1));
  EXPECT_FALSE(entry_->ShouldRejectRequest(request_));

  scoped_ptr<base::HistogramSamples> samples(
      statistics_delta_reader.GetHistogramSamplesSinceCreation(
          kRequestThrottledHistogramName));
  ASSERT_EQ(2, samples->GetCount(0));
  ASSERT_EQ(0, samples->GetCount(1));
}

TEST_F(URLRequestThrottlerEntryTest, InterfaceUpdateFailure) {
  MockURLRequestThrottlerHeaderAdapter failure_response(503);
  entry_->UpdateWithResponse(std::string(), &failure_response);
  EXPECT_GT(entry_->GetExponentialBackoffReleaseTime(), entry_->fake_time_now_)
      << "A failure should increase the release_time";
}

TEST_F(URLRequestThrottlerEntryTest, InterfaceUpdateSuccess) {
  MockURLRequestThrottlerHeaderAdapter success_response(200);
  entry_->UpdateWithResponse(std::string(), &success_response);
  EXPECT_EQ(entry_->GetExponentialBackoffReleaseTime(), entry_->fake_time_now_)
      << "A success should not add any delay";
}

TEST_F(URLRequestThrottlerEntryTest, InterfaceUpdateSuccessThenFailure) {
  MockURLRequestThrottlerHeaderAdapter failure_response(503);
  MockURLRequestThrottlerHeaderAdapter success_response(200);
  entry_->UpdateWithResponse(std::string(), &success_response);
  entry_->UpdateWithResponse(std::string(), &failure_response);
  EXPECT_GT(entry_->GetExponentialBackoffReleaseTime(), entry_->fake_time_now_)
      << "This scenario should add delay";
  entry_->UpdateWithResponse(std::string(), &success_response);
}

TEST_F(URLRequestThrottlerEntryTest, IsEntryReallyOutdated) {
  TimeDelta lifetime = TimeDelta::FromMilliseconds(
      MockURLRequestThrottlerEntry::kDefaultEntryLifetimeMs);
  const TimeDelta kFiveMs = TimeDelta::FromMilliseconds(5);

  TimeAndBool test_values[] = {
      TimeAndBool(now_, false, __LINE__),
      TimeAndBool(now_ - kFiveMs, false, __LINE__),
      TimeAndBool(now_ + kFiveMs, false, __LINE__),
      TimeAndBool(now_ - (lifetime - kFiveMs), false, __LINE__),
      TimeAndBool(now_ - lifetime, true, __LINE__),
      TimeAndBool(now_ - (lifetime + kFiveMs), true, __LINE__)};

  for (unsigned int i = 0; i < arraysize(test_values); ++i) {
    entry_->set_exponential_backoff_release_time(test_values[i].time);
    EXPECT_EQ(entry_->IsEntryOutdated(), test_values[i].result) <<
        "Test case #" << i << " line " << test_values[i].line << " failed";
  }
}

TEST_F(URLRequestThrottlerEntryTest, MaxAllowedBackoff) {
  for (int i = 0; i < 30; ++i) {
    MockURLRequestThrottlerHeaderAdapter response_adapter(503);
    entry_->UpdateWithResponse(std::string(), &response_adapter);
  }

  TimeDelta delay = entry_->GetExponentialBackoffReleaseTime() - now_;
  EXPECT_EQ(delay.InMilliseconds(),
            MockURLRequestThrottlerEntry::kDefaultMaximumBackoffMs);
}

TEST_F(URLRequestThrottlerEntryTest, MalformedContent) {
  MockURLRequestThrottlerHeaderAdapter response_adapter(503);
  for (int i = 0; i < 5; ++i)
    entry_->UpdateWithResponse(std::string(), &response_adapter);

  TimeTicks release_after_failures = entry_->GetExponentialBackoffReleaseTime();

  // Inform the entry that a response body was malformed, which is supposed to
  // increase the back-off time.  Note that we also submit a successful
  // UpdateWithResponse to pair with ReceivedContentWasMalformed() since that
  // is what happens in practice (if a body is received, then a non-500
  // response must also have been received).
  entry_->ReceivedContentWasMalformed(200);
  MockURLRequestThrottlerHeaderAdapter success_adapter(200);
  entry_->UpdateWithResponse(std::string(), &success_adapter);
  EXPECT_GT(entry_->GetExponentialBackoffReleaseTime(), release_after_failures);
}

TEST_F(URLRequestThrottlerEntryTest, SlidingWindow) {
  int max_send = URLRequestThrottlerEntry::kDefaultMaxSendThreshold;
  int sliding_window =
      URLRequestThrottlerEntry::kDefaultSlidingWindowPeriodMs;

  TimeTicks time_1 = entry_->fake_time_now_ +
      TimeDelta::FromMilliseconds(sliding_window / 3);
  TimeTicks time_2 = entry_->fake_time_now_ +
      TimeDelta::FromMilliseconds(2 * sliding_window / 3);
  TimeTicks time_3 = entry_->fake_time_now_ +
      TimeDelta::FromMilliseconds(sliding_window);
  TimeTicks time_4 = entry_->fake_time_now_ +
      TimeDelta::FromMilliseconds(sliding_window + 2 * sliding_window / 3);

  entry_->set_exponential_backoff_release_time(time_1);

  for (int i = 0; i < max_send / 2; ++i) {
    EXPECT_EQ(2 * sliding_window / 3,
              entry_->ReserveSendingTimeForNextRequest(time_2));
  }
  EXPECT_EQ(time_2, entry_->sliding_window_release_time());

  entry_->fake_time_now_ = time_3;

  for (int i = 0; i < (max_send + 1) / 2; ++i)
    EXPECT_EQ(0, entry_->ReserveSendingTimeForNextRequest(TimeTicks()));

  EXPECT_EQ(time_4, entry_->sliding_window_release_time());
}

TEST_F(URLRequestThrottlerEntryTest, ExplicitUserRequest) {
  ASSERT_FALSE(MockURLRequestThrottlerEntry::ExplicitUserRequest(0));
  ASSERT_TRUE(MockURLRequestThrottlerEntry::ExplicitUserRequest(
      LOAD_MAYBE_USER_GESTURE));
  ASSERT_FALSE(MockURLRequestThrottlerEntry::ExplicitUserRequest(
      ~LOAD_MAYBE_USER_GESTURE));
}

class URLRequestThrottlerManagerTest : public testing::Test {
 protected:
  URLRequestThrottlerManagerTest()
      : request_(GURL(), DEFAULT_PRIORITY, NULL, &context_) {}

  virtual void SetUp() {
    request_.SetLoadFlags(0);
  }

  // context_ must be declared before request_.
  TestURLRequestContext context_;
  TestURLRequest request_;
};

TEST_F(URLRequestThrottlerManagerTest, IsUrlStandardised) {
  MockURLRequestThrottlerManager manager;
  GurlAndString test_values[] = {
      GurlAndString(GURL("http://www.example.com"),
                    std::string("http://www.example.com/"),
                    __LINE__),
      GurlAndString(GURL("http://www.Example.com"),
                    std::string("http://www.example.com/"),
                    __LINE__),
      GurlAndString(GURL("http://www.ex4mple.com/Pr4c71c41"),
                    std::string("http://www.ex4mple.com/pr4c71c41"),
                    __LINE__),
      GurlAndString(GURL("http://www.example.com/0/token/false"),
                    std::string("http://www.example.com/0/token/false"),
                    __LINE__),
      GurlAndString(GURL("http://www.example.com/index.php?code=javascript"),
                    std::string("http://www.example.com/index.php"),
                    __LINE__),
      GurlAndString(GURL("http://www.example.com/index.php?code=1#superEntry"),
                    std::string("http://www.example.com/index.php"),
                    __LINE__),
      GurlAndString(GURL("http://www.example.com/index.php#superEntry"),
                    std::string("http://www.example.com/index.php"),
                    __LINE__),
      GurlAndString(GURL("http://www.example.com:1234/"),
                    std::string("http://www.example.com:1234/"),
                    __LINE__)};

  for (unsigned int i = 0; i < arraysize(test_values); ++i) {
    std::string temp = manager.DoGetUrlIdFromUrl(test_values[i].url);
    EXPECT_EQ(temp, test_values[i].result) <<
        "Test case #" << i << " line " << test_values[i].line << " failed";
  }
}

TEST_F(URLRequestThrottlerManagerTest, AreEntriesBeingCollected) {
  MockURLRequestThrottlerManager manager;

  manager.CreateEntry(true);  // true = Entry is outdated.
  manager.CreateEntry(true);
  manager.CreateEntry(true);
  manager.DoGarbageCollectEntries();
  EXPECT_EQ(0, manager.GetNumberOfEntries());

  manager.CreateEntry(false);
  manager.CreateEntry(false);
  manager.CreateEntry(false);
  manager.CreateEntry(true);
  manager.DoGarbageCollectEntries();
  EXPECT_EQ(3, manager.GetNumberOfEntries());
}

TEST_F(URLRequestThrottlerManagerTest, IsHostBeingRegistered) {
  MockURLRequestThrottlerManager manager;

  manager.RegisterRequestUrl(GURL("http://www.example.com/"));
  manager.RegisterRequestUrl(GURL("http://www.google.com/"));
  manager.RegisterRequestUrl(GURL("http://www.google.com/index/0"));
  manager.RegisterRequestUrl(GURL("http://www.google.com/index/0?code=1"));
  manager.RegisterRequestUrl(GURL("http://www.google.com/index/0#lolsaure"));

  EXPECT_EQ(3, manager.GetNumberOfEntries());
}

void ExpectEntryAllowsAllOnErrorIfOptedOut(
    net::URLRequestThrottlerEntryInterface* entry,
    bool opted_out,
    const URLRequest& request) {
  EXPECT_FALSE(entry->ShouldRejectRequest(request));
  MockURLRequestThrottlerHeaderAdapter failure_adapter(503);
  for (int i = 0; i < 10; ++i) {
    // Host doesn't really matter in this scenario so we skip it.
    entry->UpdateWithResponse(std::string(), &failure_adapter);
  }
  EXPECT_NE(opted_out, entry->ShouldRejectRequest(request));

  if (opted_out) {
    // We're not mocking out GetTimeNow() in this scenario
    // so add a 100 ms buffer to avoid flakiness (that should always
    // give enough time to get from the TimeTicks::Now() call here
    // to the TimeTicks::Now() call in the entry class).
    EXPECT_GT(TimeTicks::Now() + TimeDelta::FromMilliseconds(100),
              entry->GetExponentialBackoffReleaseTime());
  } else {
    // As above, add 100 ms.
    EXPECT_LT(TimeTicks::Now() + TimeDelta::FromMilliseconds(100),
              entry->GetExponentialBackoffReleaseTime());
  }
}

TEST_F(URLRequestThrottlerManagerTest, OptOutHeader) {
  MockURLRequestThrottlerManager manager;
  scoped_refptr<net::URLRequestThrottlerEntryInterface> entry =
      manager.RegisterRequestUrl(GURL("http://www.google.com/yodude"));

  // Fake a response with the opt-out header.
  MockURLRequestThrottlerHeaderAdapter response_adapter(
      std::string(),
      MockURLRequestThrottlerEntry::kExponentialThrottlingDisableValue,
      200);
  entry->UpdateWithResponse("www.google.com", &response_adapter);

  // Ensure that the same entry on error always allows everything.
  ExpectEntryAllowsAllOnErrorIfOptedOut(entry.get(), true, request_);

  // Ensure that a freshly created entry (for a different URL on an
  // already opted-out host) also gets "always allow" behavior.
  scoped_refptr<net::URLRequestThrottlerEntryInterface> other_entry =
      manager.RegisterRequestUrl(GURL("http://www.google.com/bingobob"));
  ExpectEntryAllowsAllOnErrorIfOptedOut(other_entry.get(), true, request_);

  // Fake a response with the opt-out header incorrectly specified.
  scoped_refptr<net::URLRequestThrottlerEntryInterface> no_opt_out_entry =
      manager.RegisterRequestUrl(GURL("http://www.nike.com/justdoit"));
  MockURLRequestThrottlerHeaderAdapter wrong_adapter(
      std::string(), "yesplease", 200);
  no_opt_out_entry->UpdateWithResponse("www.nike.com", &wrong_adapter);
  ExpectEntryAllowsAllOnErrorIfOptedOut(
      no_opt_out_entry.get(), false, request_);

  // A localhost entry should always be opted out.
  scoped_refptr<net::URLRequestThrottlerEntryInterface> localhost_entry =
      manager.RegisterRequestUrl(GURL("http://localhost/hello"));
  ExpectEntryAllowsAllOnErrorIfOptedOut(localhost_entry.get(), true, request_);
}

TEST_F(URLRequestThrottlerManagerTest, ClearOnNetworkChange) {
  for (int i = 0; i < 3; ++i) {
    MockURLRequestThrottlerManager manager;
    scoped_refptr<net::URLRequestThrottlerEntryInterface> entry_before =
        manager.RegisterRequestUrl(GURL("http://www.example.com/"));
    MockURLRequestThrottlerHeaderAdapter failure_adapter(503);
    for (int j = 0; j < 10; ++j) {
      // Host doesn't really matter in this scenario so we skip it.
      entry_before->UpdateWithResponse(std::string(), &failure_adapter);
    }
    EXPECT_TRUE(entry_before->ShouldRejectRequest(request_));

    switch (i) {
      case 0:
        manager.OnIPAddressChanged();
        break;
      case 1:
        manager.OnConnectionTypeChanged(
            net::NetworkChangeNotifier::CONNECTION_UNKNOWN);
        break;
      case 2:
        manager.OnConnectionTypeChanged(
            net::NetworkChangeNotifier::CONNECTION_NONE);
        break;
      default:
        FAIL();
    }

    scoped_refptr<net::URLRequestThrottlerEntryInterface> entry_after =
        manager.RegisterRequestUrl(GURL("http://www.example.com/"));
    EXPECT_FALSE(entry_after->ShouldRejectRequest(request_));
  }
}

}  // namespace net
