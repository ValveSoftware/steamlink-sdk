// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_time/network_time_tracker.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/client_update_protocol/ecdsa.h"
#include "components/network_time/network_time_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network_time {

class NetworkTimeTrackerTest : public testing::Test {
 public:
  ~NetworkTimeTrackerTest() override {}

  NetworkTimeTrackerTest()
      : io_thread_("IO thread"),
        clock_(new base::SimpleTestClock),
        tick_clock_(new base::SimpleTestTickClock),
        test_server_(new net::EmbeddedTestServer) {
    base::Thread::Options thread_options;
    thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
    EXPECT_TRUE(io_thread_.StartWithOptions(thread_options));
    NetworkTimeTracker::RegisterPrefs(pref_service_.registry());

    SetNetworkQueriesWithVariationsService(true, 0.0 /* query probability */);

    tracker_.reset(new NetworkTimeTracker(
        std::unique_ptr<base::Clock>(clock_),
        std::unique_ptr<base::TickClock>(tick_clock_), &pref_service_,
        new net::TestURLRequestContextGetter(io_thread_.task_runner())));

    // Do this to be sure that |is_null| returns false.
    clock_->Advance(base::TimeDelta::FromDays(111));
    tick_clock_->Advance(base::TimeDelta::FromDays(222));

    // Can not be smaller than 15, it's the NowFromSystemTime() resolution.
    resolution_ = base::TimeDelta::FromMilliseconds(17);
    latency_ = base::TimeDelta::FromMilliseconds(50);
    adjustment_ = 7 * base::TimeDelta::FromMilliseconds(kTicksResolutionMs);
  }

  void TearDown() override { io_thread_.Stop(); }

  // Replaces |tracker_| with a new object, while preserving the
  // testing clocks.
  void Reset() {
    base::SimpleTestClock* new_clock = new base::SimpleTestClock();
    new_clock->SetNow(clock_->Now());
    base::SimpleTestTickClock* new_tick_clock = new base::SimpleTestTickClock();
    new_tick_clock->SetNowTicks(tick_clock_->NowTicks());
    clock_ = new_clock;
    tick_clock_ = new_tick_clock;
    tracker_.reset(new NetworkTimeTracker(
        std::unique_ptr<base::Clock>(clock_),
        std::unique_ptr<base::TickClock>(tick_clock_), &pref_service_,
        new net::TestURLRequestContextGetter(io_thread_.task_runner())));
  }

  // Returns a valid time response.  Update as follows:
  //
  // curl http://clients2.google.com/time/1/current?cup2key=1:123123123
  //
  // where 1 is the key version and 123123123 is the nonce.  Copy the nonce, the
  // response, and the x-cup-server-proof header into the test.
  static std::unique_ptr<net::test_server::HttpResponse>
  GoodTimeResponseHandler(const net::test_server::HttpRequest& request) {
    net::test_server::BasicHttpResponse* response =
        new net::test_server::BasicHttpResponse();
    response->set_code(net::HTTP_OK);
    response->set_content(
        ")]}'\n"
        "{\"current_time_millis\":1461621971825,\"server_nonce\":-6."
        "006853099049523E85}");
    response->AddCustomHeader(
        "x-cup-server-proof",
        "304402202e0f24db1ea69f1bbe81da4108f381fcf7a2781c53cf7663cb47083cb5fe8e"
        "fd"
        "022009d2b67c0deceaaf849f7c529be96701ed5f15d5efcaf401a94e0801accc9832:"
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    return std::unique_ptr<net::test_server::HttpResponse>(response);
  }

  // Good signature over invalid data, though made with a non-production key.
  static std::unique_ptr<net::test_server::HttpResponse> BadDataResponseHandler(
      const net::test_server::HttpRequest& request) {
    net::test_server::BasicHttpResponse* response =
        new net::test_server::BasicHttpResponse();
    response->set_code(net::HTTP_OK);
    response->set_content(
        ")]}'\n"
        "{\"current_time_millis\":NaN,\"server_nonce\":9.420921002039447E182}");
    response->AddCustomHeader(
        "x-cup-server-proof",
        "3046022100a07aa437b24f1f6bb7ff6f6d1e004dd4bcb717c93e21d6bae5ef8d6d984c"
        "86a7022100e423419ff49fae37b421ef6cdeab348b45c63b236ab365f36f4cd3b4d4d6"
        "d852:"
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b85"
        "5");
    return std::unique_ptr<net::test_server::HttpResponse>(response);
  }

  static std::unique_ptr<net::test_server::HttpResponse>
  BadSignatureResponseHandler(const net::test_server::HttpRequest& request) {
    net::test_server::BasicHttpResponse* response =
        new net::test_server::BasicHttpResponse();
    response->set_code(net::HTTP_OK);
    response->set_content(
        ")]}'\n"
        "{\"current_time_millis\":1461621971825,\"server_nonce\":-6."
        "006853099049523E85}");
    response->AddCustomHeader("x-cup-server-proof", "dead:beef");
    return std::unique_ptr<net::test_server::HttpResponse>(response);
  }

  static std::unique_ptr<net::test_server::HttpResponse>
  ServerErrorResponseHandler(const net::test_server::HttpRequest& request) {
    net::test_server::BasicHttpResponse* response =
        new net::test_server::BasicHttpResponse();
    response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
    return std::unique_ptr<net::test_server::HttpResponse>(response);
  }

  static std::unique_ptr<net::test_server::HttpResponse>
  NetworkErrorResponseHandler(const net::test_server::HttpRequest& request) {
    return std::unique_ptr<net::test_server::HttpResponse>(
        new net::test_server::RawHttpResponse("", ""));
  }

  // Updates the notifier's time with the specified parameters.
  void UpdateNetworkTime(const base::Time& network_time,
                         const base::TimeDelta& resolution,
                         const base::TimeDelta& latency,
                         const base::TimeTicks& post_time) {
    tracker_->UpdateNetworkTime(
        network_time, resolution, latency, post_time);
  }

  // Advances both the system clock and the tick clock.  This should be used for
  // the normal passage of time, i.e. when neither clock is doing anything odd.
  void AdvanceBoth(const base::TimeDelta& delta) {
    tick_clock_->Advance(delta);
    clock_->Advance(delta);
  }

 protected:
  void SetNetworkQueriesWithVariationsService(bool enable,
                                              float query_probability) {
    const std::string kTrialName = "Trial";
    const std::string kGroupName = "group";
    const base::Feature kFeature{"NetworkTimeServiceQuerying",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

    // Clear all the things.
    base::FeatureList::ClearInstanceForTesting();
    variations::testing::ClearAllVariationParams();

    std::map<std::string, std::string> params;
    params["RandomQueryProbability"] = base::DoubleToString(query_probability);
    params["CheckTimeIntervalSeconds"] = base::Int64ToString(360);

    // There are 3 things here: a FieldTrial, a FieldTrialList, and a
    // FeatureList.  Don't get confused!  The FieldTrial is reference-counted,
    // and a reference is held by the FieldTrialList.  The FieldTrialList and
    // FeatureList are both singletons.  The authorized way to reset the former
    // for testing is to destruct it (above).  The latter, by contrast, is reset
    // with |ClearInstanceForTesting|, above.  If this comment was useful to you
    // please send me a postcard.

    field_trial_list_.reset();  // Averts a CHECK fail in constructor below.
    field_trial_list_.reset(
        new base::FieldTrialList(new base::MockEntropyProvider()));
    // refcounted, and reference held by field_trial_list_.
    base::FieldTrial* trial = base::FieldTrialList::FactoryGetFieldTrial(
        kTrialName, 100, kGroupName, 1971, 1, 1,
        base::FieldTrial::SESSION_RANDOMIZED,
        nullptr /* default_group_number */);
    ASSERT_TRUE(
        variations::AssociateVariationParams(kTrialName, kGroupName, params));

    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->RegisterFieldTrialOverride(
        kFeature.name, enable ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                              : base::FeatureList::OVERRIDE_DISABLE_FEATURE,
        trial);
    base::FeatureList::SetInstance(std::move(feature_list));
  }

  base::Thread io_thread_;
  base::MessageLoop message_loop_;
  base::TimeDelta resolution_;
  base::TimeDelta latency_;
  base::TimeDelta adjustment_;
  base::SimpleTestClock* clock_;
  base::SimpleTestTickClock* tick_clock_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<base::FieldTrialList> field_trial_list_;
  std::unique_ptr<NetworkTimeTracker> tracker_;
  std::unique_ptr<net::EmbeddedTestServer> test_server_;
};

TEST_F(NetworkTimeTrackerTest, Uninitialized) {
  base::Time network_time;
  base::TimeDelta uncertainty;
  EXPECT_FALSE(tracker_->GetNetworkTime(&network_time, &uncertainty));
}

TEST_F(NetworkTimeTrackerTest, LongPostingDelay) {
  // The request arrives at the server, which records the time.  Advance the
  // clock to simulate the latency of sending the reply, which we'll say for
  // convenience is half the total latency.
  base::Time in_network_time = clock_->Now();
  AdvanceBoth(latency_ / 2);

  // Record the tick counter at the time the reply is received.  At this point,
  // we would post UpdateNetworkTime to be run on the browser thread.
  base::TimeTicks posting_time = tick_clock_->NowTicks();

  // Simulate that it look a long time (1888us) for the browser thread to get
  // around to executing the update.
  AdvanceBoth(base::TimeDelta::FromMicroseconds(1888));
  UpdateNetworkTime(in_network_time, resolution_, latency_, posting_time);

  base::Time out_network_time;
  base::TimeDelta uncertainty;
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time,  &uncertainty));
  EXPECT_EQ(resolution_ + latency_ + adjustment_, uncertainty);
  EXPECT_EQ(clock_->Now(), out_network_time);
}

TEST_F(NetworkTimeTrackerTest, LopsidedLatency) {
  // Simulate that the server received the request instantaneously, and that all
  // of the latency was in sending the reply.  (This contradicts the assumption
  // in the code.)
  base::Time in_network_time = clock_->Now();
  AdvanceBoth(latency_);
  UpdateNetworkTime(in_network_time, resolution_, latency_,
                    tick_clock_->NowTicks());

  // But, the answer is still within the uncertainty bounds!
  base::Time out_network_time;
  base::TimeDelta uncertainty;
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time,  &uncertainty));
  EXPECT_LT(out_network_time - uncertainty / 2, clock_->Now());
  EXPECT_GT(out_network_time + uncertainty / 2, clock_->Now());
}

TEST_F(NetworkTimeTrackerTest, ClockIsWack) {
  // Now let's assume the system clock is completely wrong.
  base::Time in_network_time = clock_->Now() - base::TimeDelta::FromDays(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());

  base::Time out_network_time;
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time, nullptr));
  EXPECT_EQ(in_network_time, out_network_time);
}

TEST_F(NetworkTimeTrackerTest, ClocksDivergeSlightly) {
  // The two clocks are allowed to diverge a little bit.
  base::Time in_network_time = clock_->Now();
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());

  base::TimeDelta small = base::TimeDelta::FromSeconds(30);
  tick_clock_->Advance(small);
  base::Time out_network_time;
  base::TimeDelta out_uncertainty;
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time, &out_uncertainty));
  EXPECT_EQ(in_network_time + small, out_network_time);
  // The clock divergence should show up in the uncertainty.
  EXPECT_EQ(resolution_ + latency_ + adjustment_ + small, out_uncertainty);
}

TEST_F(NetworkTimeTrackerTest, NetworkTimeUpdates) {
  // Verify that the the tracker receives and properly handles updates to the
  // network time.
  base::Time out_network_time;
  base::TimeDelta uncertainty;

  UpdateNetworkTime(clock_->Now() - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time,  &uncertainty));
  EXPECT_EQ(clock_->Now(), out_network_time);
  EXPECT_EQ(resolution_ + latency_ + adjustment_, uncertainty);

  // Fake a wait to make sure we keep tracking.
  AdvanceBoth(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time,  &uncertainty));
  EXPECT_EQ(clock_->Now(), out_network_time);
  EXPECT_EQ(resolution_ + latency_ + adjustment_, uncertainty);

  // And one more time.
  UpdateNetworkTime(clock_->Now() - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());
  AdvanceBoth(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time,  &uncertainty));
  EXPECT_EQ(clock_->Now(), out_network_time);
  EXPECT_EQ(resolution_ + latency_ + adjustment_, uncertainty);
}

TEST_F(NetworkTimeTrackerTest, SpringForward) {
  // Simulate the wall clock advancing faster than the tick clock.
  UpdateNetworkTime(clock_->Now(), resolution_, latency_,
                    tick_clock_->NowTicks());
  tick_clock_->Advance(base::TimeDelta::FromSeconds(1));
  clock_->Advance(base::TimeDelta::FromDays(1));
  base::Time out_network_time;
  EXPECT_FALSE(tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, FallBack) {
  // Simulate the wall clock running backward.
  UpdateNetworkTime(clock_->Now(), resolution_, latency_,
                    tick_clock_->NowTicks());
  tick_clock_->Advance(base::TimeDelta::FromSeconds(1));
  clock_->Advance(base::TimeDelta::FromDays(-1));
  base::Time out_network_time;
  EXPECT_FALSE(tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, SuspendAndResume) {
  // Simulate the wall clock advancing while the tick clock stands still, as
  // would happen in a suspend+resume cycle.
  UpdateNetworkTime(clock_->Now(), resolution_, latency_,
                    tick_clock_->NowTicks());
  clock_->Advance(base::TimeDelta::FromHours(1));
  base::Time out_network_time;
  EXPECT_FALSE(tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, Serialize) {
  // Test that we can serialize and deserialize state and get consistent
  // results.
  base::Time in_network_time = clock_->Now() - base::TimeDelta::FromDays(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());
  base::Time out_network_time;
  base::TimeDelta out_uncertainty;
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time, &out_uncertainty));
  EXPECT_EQ(in_network_time, out_network_time);
  EXPECT_EQ(resolution_ + latency_ + adjustment_, out_uncertainty);

  // 6 days is just under the threshold for discarding data.
  base::TimeDelta delta = base::TimeDelta::FromDays(6);
  AdvanceBoth(delta);
  Reset();
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time, &out_uncertainty));
  EXPECT_EQ(in_network_time + delta, out_network_time);
  EXPECT_EQ(resolution_ + latency_ + adjustment_, out_uncertainty);
}

TEST_F(NetworkTimeTrackerTest, DeserializeOldFormat) {
  // Test that deserializing old data (which do not record the uncertainty and
  // tick clock) causes the serialized data to be ignored.
  base::Time in_network_time = clock_->Now() - base::TimeDelta::FromDays(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());

  base::Time out_network_time;
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time, nullptr));
  double local, network;
  const base::DictionaryValue* saved_prefs =
      pref_service_.GetDictionary(prefs::kNetworkTimeMapping);
  saved_prefs->GetDouble("local", &local);
  saved_prefs->GetDouble("network", &network);
  base::DictionaryValue prefs;
  prefs.SetDouble("local", local);
  prefs.SetDouble("network", network);
  pref_service_.Set(prefs::kNetworkTimeMapping, prefs);
  Reset();
  EXPECT_FALSE(tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, SerializeWithLongDelay) {
  // Test that if the serialized data are more than a week old, they are
  // discarded.
  base::Time in_network_time = clock_->Now() - base::TimeDelta::FromDays(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());
  base::Time out_network_time;
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time, nullptr));
  AdvanceBoth(base::TimeDelta::FromDays(8));
  Reset();
  EXPECT_FALSE(tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, SerializeWithTickClockAdvance) {
  // Test that serialized data are discarded if the wall clock and tick clock
  // have not advanced consistently since data were serialized.
  base::Time in_network_time = clock_->Now() - base::TimeDelta::FromDays(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());
  base::Time out_network_time;
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time, nullptr));
  tick_clock_->Advance(base::TimeDelta::FromDays(1));
  Reset();
  EXPECT_FALSE(tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, SerializeWithWallClockAdvance) {
  // Test that serialized data are discarded if the wall clock and tick clock
  // have not advanced consistently since data were serialized.
  base::Time in_network_time = clock_->Now() - base::TimeDelta::FromDays(90);
  UpdateNetworkTime(in_network_time - latency_ / 2, resolution_, latency_,
                    tick_clock_->NowTicks());

  base::Time out_network_time;
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time, nullptr));
  clock_->Advance(base::TimeDelta::FromDays(1));
  Reset();
  EXPECT_FALSE(tracker_->GetNetworkTime(&out_network_time, nullptr));
}

TEST_F(NetworkTimeTrackerTest, UpdateFromNetwork) {
  base::Time out_network_time;
  EXPECT_FALSE(tracker_->GetNetworkTime(&out_network_time, nullptr));
  // First query should happen soon.
  EXPECT_EQ(base::TimeDelta::FromMinutes(0),
            tracker_->GetTimerDelayForTesting());

  test_server_->RegisterRequestHandler(
      base::Bind(&NetworkTimeTrackerTest::GoodTimeResponseHandler));
  EXPECT_TRUE(test_server_->Start());
  tracker_->SetTimeServerURLForTesting(test_server_->GetURL("/"));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);

  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time, nullptr));
  EXPECT_EQ(base::Time::UnixEpoch() +
                base::TimeDelta::FromMilliseconds(1461621971825),
            out_network_time);
  // Should see no backoff in the success case.
  EXPECT_EQ(base::TimeDelta::FromMinutes(60),
            tracker_->GetTimerDelayForTesting());
}

TEST_F(NetworkTimeTrackerTest, NoNetworkQueryWhileSynced) {
  test_server_->RegisterRequestHandler(
      base::Bind(&NetworkTimeTrackerTest::GoodTimeResponseHandler));
  EXPECT_TRUE(test_server_->Start());
  tracker_->SetTimeServerURLForTesting(test_server_->GetURL("/"));

  SetNetworkQueriesWithVariationsService(true, 0.0);
  base::Time in_network_time = clock_->Now();
  UpdateNetworkTime(in_network_time, resolution_, latency_,
                    tick_clock_->NowTicks());

  // No query should be started so long as NetworkTimeTracker is synced, but the
  // next check should happen soon.
  EXPECT_FALSE(tracker_->QueryTimeServiceForTesting());
  EXPECT_EQ(base::TimeDelta::FromMinutes(6),
            tracker_->GetTimerDelayForTesting());

  SetNetworkQueriesWithVariationsService(true, 1.0);
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);
  EXPECT_EQ(base::TimeDelta::FromMinutes(60),
            tracker_->GetTimerDelayForTesting());
}

TEST_F(NetworkTimeTrackerTest, NoNetworkQueryWhileFeatureDisabled) {
  // Disable network time queries and check that a query is not sent.
  SetNetworkQueriesWithVariationsService(false, 0.0);
  EXPECT_FALSE(tracker_->QueryTimeServiceForTesting());
  EXPECT_EQ(base::TimeDelta::FromMinutes(6),
            tracker_->GetTimerDelayForTesting());

  // Enable time queries and check that a query is sent.
  SetNetworkQueriesWithVariationsService(true, 0.0);
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);
}

TEST_F(NetworkTimeTrackerTest, UpdateFromNetworkBadSignature) {
  test_server_->RegisterRequestHandler(
      base::Bind(&NetworkTimeTrackerTest::BadSignatureResponseHandler));
  EXPECT_TRUE(test_server_->Start());
  tracker_->SetTimeServerURLForTesting(test_server_->GetURL("/"));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);

  base::Time out_network_time;
  EXPECT_FALSE(tracker_->GetNetworkTime(&out_network_time, nullptr));
  EXPECT_EQ(base::TimeDelta::FromMinutes(120),
            tracker_->GetTimerDelayForTesting());
}

static const uint8_t kDevKeyPubBytes[] = {
    0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02,
    0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03,
    0x42, 0x00, 0x04, 0xe0, 0x6b, 0x0d, 0x76, 0x75, 0xa3, 0x99, 0x7d, 0x7c,
    0x1b, 0xd6, 0x3c, 0x73, 0xbb, 0x4b, 0xfe, 0x0a, 0xe7, 0x2f, 0x61, 0x3d,
    0x77, 0x0a, 0xaa, 0x14, 0xd8, 0x5a, 0xbf, 0x14, 0x60, 0xec, 0xf6, 0x32,
    0x77, 0xb5, 0xa7, 0xe6, 0x35, 0xa5, 0x61, 0xaf, 0xdc, 0xdf, 0x91, 0xce,
    0x45, 0x34, 0x5f, 0x36, 0x85, 0x2f, 0xb9, 0x53, 0x00, 0x5d, 0x86, 0xe7,
    0x04, 0x16, 0xe2, 0x3d, 0x21, 0x76, 0x2b};

TEST_F(NetworkTimeTrackerTest, UpdateFromNetworkBadData) {
  test_server_->RegisterRequestHandler(
      base::Bind(&NetworkTimeTrackerTest::BadDataResponseHandler));
  EXPECT_TRUE(test_server_->Start());
  base::StringPiece key = {reinterpret_cast<const char*>(kDevKeyPubBytes),
                           sizeof(kDevKeyPubBytes)};
  tracker_->SetPublicKeyForTesting(key);
  tracker_->SetTimeServerURLForTesting(test_server_->GetURL("/"));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);
  base::Time out_network_time;
  EXPECT_FALSE(tracker_->GetNetworkTime(&out_network_time, nullptr));
  EXPECT_EQ(base::TimeDelta::FromMinutes(120),
            tracker_->GetTimerDelayForTesting());
}

TEST_F(NetworkTimeTrackerTest, UpdateFromNetworkServerError) {
  test_server_->RegisterRequestHandler(
      base::Bind(&NetworkTimeTrackerTest::ServerErrorResponseHandler));
  EXPECT_TRUE(test_server_->Start());
  tracker_->SetTimeServerURLForTesting(test_server_->GetURL("/"));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);

  base::Time out_network_time;
  EXPECT_FALSE(tracker_->GetNetworkTime(&out_network_time, nullptr));
  // Should see backoff in the error case.
  EXPECT_EQ(base::TimeDelta::FromMinutes(120),
            tracker_->GetTimerDelayForTesting());
}

TEST_F(NetworkTimeTrackerTest, UpdateFromNetworNetworkError) {
  test_server_->RegisterRequestHandler(
      base::Bind(&NetworkTimeTrackerTest::NetworkErrorResponseHandler));
  EXPECT_TRUE(test_server_->Start());
  tracker_->SetTimeServerURLForTesting(test_server_->GetURL("/"));
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);

  base::Time out_network_time;
  EXPECT_FALSE(tracker_->GetNetworkTime(&out_network_time, nullptr));
  // Should see backoff in the error case.
  EXPECT_EQ(base::TimeDelta::FromMinutes(120),
            tracker_->GetTimerDelayForTesting());
}

TEST_F(NetworkTimeTrackerTest, UpdateFromNetworkLargeResponse) {
  test_server_->RegisterRequestHandler(
      base::Bind(&NetworkTimeTrackerTest::GoodTimeResponseHandler));
  EXPECT_TRUE(test_server_->Start());
  tracker_->SetTimeServerURLForTesting(test_server_->GetURL("/"));

  base::Time out_network_time;

  tracker_->SetMaxResponseSizeForTesting(3);
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);
  EXPECT_FALSE(tracker_->GetNetworkTime(&out_network_time, nullptr));

  tracker_->SetMaxResponseSizeForTesting(1024);
  EXPECT_TRUE(tracker_->QueryTimeServiceForTesting());
  tracker_->WaitForFetchForTesting(123123123);
  EXPECT_TRUE(tracker_->GetNetworkTime(&out_network_time, nullptr));
}

}  // namespace network_time
