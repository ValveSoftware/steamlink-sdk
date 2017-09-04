// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_time/network_time_test_utils.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "components/variations/variations_associated_data.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network_time {

// Update as follows:
//
// curl http://clients2.google.com/time/1/current?cup2key=1:123123123
//
// where 1 is the key version and 123123123 is the nonce.  Copy the
// response and the x-cup-server-proof header into
// |kGoodTimeResponseBody| and |kGoodTimeResponseServerProofHeader|
// respectively, and the 'current_time_millis' value of the response
// into |kGoodTimeResponseHandlerJsTime|.
const char kGoodTimeResponseBody[] =
    ")]}'\n"
    "{\"current_time_millis\":1461621971825"
    ",\"server_nonce\":-6."
    "006853099049523E85}";
const char kGoodTimeResponseServerProofHeader[] =
    "304402202e0f24db1ea69f1bbe81da4108f381fcf7a2781c53cf7663cb47083cb5fe8e"
    "fd"
    "022009d2b67c0deceaaf849f7c529be96701ed5f15d5efcaf401a94e0801accc9832:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
const double kGoodTimeResponseHandlerJsTime = 1461621971825;

std::unique_ptr<net::test_server::HttpResponse> GoodTimeResponseHandler(
    const net::test_server::HttpRequest& request) {
  net::test_server::BasicHttpResponse* response =
      new net::test_server::BasicHttpResponse();
  response->set_code(net::HTTP_OK);
  response->set_content(kGoodTimeResponseBody);
  response->AddCustomHeader("x-cup-server-proof",
                            kGoodTimeResponseServerProofHeader);
  return std::unique_ptr<net::test_server::HttpResponse>(response);
}

FieldTrialTest::~FieldTrialTest() {}

FieldTrialTest* FieldTrialTest::CreateForUnitTest() {
  FieldTrialTest* test = new FieldTrialTest();
  test->create_field_trial_list_ = true;
  return test;
}

FieldTrialTest* FieldTrialTest::CreateForBrowserTest() {
  FieldTrialTest* test = new FieldTrialTest();
  test->create_field_trial_list_ = false;
  return test;
}

void FieldTrialTest::SetNetworkQueriesWithVariationsService(
    bool enable,
    float query_probability,
    NetworkTimeTracker::FetchBehavior fetch_behavior) {
  const std::string kTrialName = "Trial";
  const std::string kGroupName = "group";
  const base::Feature kFeature{"NetworkTimeServiceQuerying",
                               base::FEATURE_DISABLED_BY_DEFAULT};

  // Clear all the things.
  variations::testing::ClearAllVariationParams();

  std::map<std::string, std::string> params;
  params["RandomQueryProbability"] = base::DoubleToString(query_probability);
  params["CheckTimeIntervalSeconds"] = base::Int64ToString(360);
  std::string fetch_behavior_param;
  switch (fetch_behavior) {
    case NetworkTimeTracker::FETCH_BEHAVIOR_UNKNOWN:
      NOTREACHED();
      fetch_behavior_param = "unknown";
      break;
    case NetworkTimeTracker::FETCHES_IN_BACKGROUND_ONLY:
      fetch_behavior_param = "background-only";
      break;
    case NetworkTimeTracker::FETCHES_ON_DEMAND_ONLY:
      fetch_behavior_param = "on-demand-only";
      break;
    case NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND:
      fetch_behavior_param = "background-and-on-demand";
      break;
  }
  params["FetchBehavior"] = fetch_behavior_param;

  // There are 3 things here: a FieldTrial, a FieldTrialList, and a
  // FeatureList.  Don't get confused!  The FieldTrial is reference-counted,
  // and a reference is held by the FieldTrialList.  The FieldTrialList and
  // FeatureList are both singletons.  The authorized way to reset the former
  // for testing is to destruct it (above).  The latter, by contrast, should
  // should already start in a clean state and can be manipulated via the
  // ScopedFeatureList helper class. If this comment was useful to you
  // please send me a postcard.

  if (create_field_trial_list_) {
    field_trial_list_.reset();  // Averts a CHECK fail in constructor below.
    field_trial_list_.reset(new base::FieldTrialList(
        base::MakeUnique<base::MockEntropyProvider>()));
  }
  // refcounted, and reference held by the singleton FieldTrialList.
  base::FieldTrial* trial = base::FieldTrialList::FactoryGetFieldTrial(
      kTrialName, 100, kGroupName, 1971, 1, 1,
      base::FieldTrial::SESSION_RANDOMIZED, nullptr /* default_group_number */);
  ASSERT_TRUE(
      variations::AssociateVariationParams(kTrialName, kGroupName, params));

  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  feature_list->RegisterFieldTrialOverride(
      kFeature.name, enable ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                            : base::FeatureList::OVERRIDE_DISABLE_FEATURE,
      trial);
  scoped_feature_list_.reset(new base::test::ScopedFeatureList);
  scoped_feature_list_->InitWithFeatureList(std::move(feature_list));
}

FieldTrialTest::FieldTrialTest() {}

}  // namespace network_time
