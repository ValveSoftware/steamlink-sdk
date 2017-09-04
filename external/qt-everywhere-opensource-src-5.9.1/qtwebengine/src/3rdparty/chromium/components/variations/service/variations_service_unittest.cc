// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_service.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/json/json_string_value_serializer.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/histogram_tester.h"
#include "base/version.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/web_resource/resource_request_allowed_notifier_test_util.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

namespace {

class TestVariationsServiceClient : public VariationsServiceClient {
 public:
  TestVariationsServiceClient() {}
  ~TestVariationsServiceClient() override {}

  // variations::VariationsServiceClient:
  std::string GetApplicationLocale() override { return std::string(); }
  base::SequencedWorkerPool* GetBlockingPool() override { return nullptr; }
  base::Callback<base::Version(void)> GetVersionForSimulationCallback()
      override {
    return base::Callback<base::Version(void)>();
  }
  net::URLRequestContextGetter* GetURLRequestContext() override {
    return nullptr;
  }
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override {
    return nullptr;
  }
  version_info::Channel GetChannel() override {
    return version_info::Channel::UNKNOWN;
  }
  bool OverridesRestrictParameter(std::string* parameter) override {
    if (restrict_parameter_.empty())
      return false;
    *parameter = restrict_parameter_;
    return true;
  }
  void OnInitialStartup() override {}

  void set_restrict_parameter(const std::string& value) {
    restrict_parameter_ = value;
  }

 private:
  std::string restrict_parameter_;

  DISALLOW_COPY_AND_ASSIGN(TestVariationsServiceClient);
};

// A test class used to validate expected functionality in VariationsService.
class TestVariationsService : public VariationsService {
 public:
  TestVariationsService(
      std::unique_ptr<web_resource::TestRequestAllowedNotifier> test_notifier,
      PrefService* local_state)
      : VariationsService(base::WrapUnique(new TestVariationsServiceClient()),
                          std::move(test_notifier),
                          local_state,
                          NULL,
                          UIStringOverrider()),
        intercepts_fetch_(true),
        fetch_attempted_(false),
        seed_stored_(false),
        delta_compressed_seed_(false),
        gzip_compressed_seed_(false) {
    // Set this so StartRepeatedVariationsSeedFetch can be called in tests.
    SetCreateTrialsFromSeedCalledForTesting(true);
    set_variations_server_url(
        GetVariationsServerURL(local_state, std::string()));
  }

  ~TestVariationsService() override {}

  void set_intercepts_fetch(bool value) {
    intercepts_fetch_ = value;
  }

  bool fetch_attempted() const { return fetch_attempted_; }
  bool seed_stored() const { return seed_stored_; }
  const std::string& stored_country() const { return stored_country_; }
  bool delta_compressed_seed() const { return delta_compressed_seed_; }
  bool gzip_compressed_seed() const { return gzip_compressed_seed_; }

  void DoActualFetch() override {
    if (intercepts_fetch_) {
      fetch_attempted_ = true;
      return;
    }

    VariationsService::DoActualFetch();
  }

  bool StoreSeed(const std::string& seed_data,
                 const std::string& seed_signature,
                 const std::string& country_code,
                 const base::Time& date_fetched,
                 bool is_delta_compressed,
                 bool is_gzip_compressed) override {
    seed_stored_ = true;
    stored_seed_data_ = seed_data;
    stored_country_ = country_code;
    delta_compressed_seed_ = is_delta_compressed;
    gzip_compressed_seed_ = is_gzip_compressed;
    return true;
  }

  std::unique_ptr<const base::FieldTrial::EntropyProvider>
  CreateLowEntropyProvider() override {
    return std::unique_ptr<const base::FieldTrial::EntropyProvider>(nullptr);
  }

 private:
  bool LoadSeed(VariationsSeed* seed) override {
    if (!seed_stored_)
      return false;
    return seed->ParseFromString(stored_seed_data_);
  }

  bool intercepts_fetch_;
  bool fetch_attempted_;
  bool seed_stored_;
  std::string stored_seed_data_;
  std::string stored_country_;
  bool delta_compressed_seed_;
  bool gzip_compressed_seed_;

  DISALLOW_COPY_AND_ASSIGN(TestVariationsService);
};

class TestVariationsServiceObserver : public VariationsService::Observer {
 public:
  TestVariationsServiceObserver()
      : best_effort_changes_notified_(0),
        crticial_changes_notified_(0) {
  }
  ~TestVariationsServiceObserver() override {}

  void OnExperimentChangesDetected(Severity severity) override {
    switch (severity) {
      case BEST_EFFORT:
        ++best_effort_changes_notified_;
        break;
      case CRITICAL:
        ++crticial_changes_notified_;
        break;
    }
  }

  int best_effort_changes_notified() const {
    return best_effort_changes_notified_;
  }

  int crticial_changes_notified() const {
    return crticial_changes_notified_;
  }

 private:
  // Number of notification received with BEST_EFFORT severity.
  int best_effort_changes_notified_;

  // Number of notification received with CRITICAL severity.
  int crticial_changes_notified_;

  DISALLOW_COPY_AND_ASSIGN(TestVariationsServiceObserver);
};

// Constants used to create the test seed.
const char kTestSeedStudyName[] = "test";
const char kTestSeedExperimentName[] = "abc";
const int kTestSeedExperimentProbability = 100;
const char kTestSeedSerialNumber[] = "123";

// Populates |seed| with simple test data. The resulting seed will contain one
// study called "test", which contains one experiment called "abc" with
// probability weight 100. |seed|'s study field will be cleared before adding
// the new study.
variations::VariationsSeed CreateTestSeed() {
  variations::VariationsSeed seed;
  variations::Study* study = seed.add_study();
  study->set_name(kTestSeedStudyName);
  study->set_default_experiment_name(kTestSeedExperimentName);
  variations::Study_Experiment* experiment = study->add_experiment();
  experiment->set_name(kTestSeedExperimentName);
  experiment->set_probability_weight(kTestSeedExperimentProbability);
  seed.set_serial_number(kTestSeedSerialNumber);
  return seed;
}

// Serializes |seed| to protobuf binary format.
std::string SerializeSeed(const variations::VariationsSeed& seed) {
  std::string serialized_seed;
  seed.SerializeToString(&serialized_seed);
  return serialized_seed;
}

// Simulates a variations service response by setting a date header and the
// specified HTTP |response_code| on |fetcher|. Sets additional header |header|
// if it is not null.
scoped_refptr<net::HttpResponseHeaders> SimulateServerResponseWithHeader(
    int response_code,
    net::TestURLFetcher* fetcher,
    const std::string* header) {
  EXPECT_TRUE(fetcher);
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders("date:Wed, 13 Feb 2013 00:25:24 GMT\0\0"));
  if (header)
    headers->AddHeader(*header);
  fetcher->set_response_headers(headers);
  fetcher->set_response_code(response_code);
  return headers;
}

// Simulates a variations service response by setting a date header and the
// specified HTTP |response_code| on |fetcher|.
scoped_refptr<net::HttpResponseHeaders> SimulateServerResponse(
    int response_code,
    net::TestURLFetcher* fetcher) {
  return SimulateServerResponseWithHeader(response_code, fetcher, nullptr);
}

// Converts |list_value| to a string, to make it easier for debugging.
std::string ListValueToString(const base::ListValue& list_value) {
  std::string json;
  JSONStringValueSerializer serializer(&json);
  serializer.set_pretty_print(true);
  serializer.Serialize(list_value);
  return json;
}

}  // namespace

class VariationsServiceTest : public ::testing::Test {
 protected:
  VariationsServiceTest() {}

 private:
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(VariationsServiceTest);
};

TEST_F(VariationsServiceTest, CreateTrialsFromSeed) {
  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());

  // Create a local base::FieldTrialList, to hold the field trials created in
  // this test.
  base::FieldTrialList field_trial_list(nullptr);

  // Create a variations service.
  TestVariationsService service(
      base::MakeUnique<web_resource::TestRequestAllowedNotifier>(&prefs),
      &prefs);
  service.SetCreateTrialsFromSeedCalledForTesting(false);

  // Store a seed.
  service.StoreSeed(SerializeSeed(CreateTestSeed()), std::string(),
                    std::string(), base::Time::Now(), false, false);
  prefs.SetInt64(prefs::kVariationsLastFetchTime,
                 base::Time::Now().ToInternalValue());

  // Check that field trials are created from the seed. Since the test study has
  // only 1 experiment with 100% probability weight, we must be part of it.
  EXPECT_TRUE(service.CreateTrialsFromSeed(base::FeatureList::GetInstance()));
  EXPECT_EQ(kTestSeedExperimentName,
            base::FieldTrialList::FindFullName(kTestSeedStudyName));
}

TEST_F(VariationsServiceTest, CreateTrialsFromSeedNoLastFetchTime) {
  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());

  // Create a local base::FieldTrialList, to hold the field trials created in
  // this test.
  base::FieldTrialList field_trial_list(nullptr);

  // Create a variations service
  TestVariationsService service(
      base::MakeUnique<web_resource::TestRequestAllowedNotifier>(&prefs),
      &prefs);
  service.SetCreateTrialsFromSeedCalledForTesting(false);

  // Store a seed. To simulate a first run, |prefs::kVariationsLastFetchTime|
  // is left empty.
  service.StoreSeed(SerializeSeed(CreateTestSeed()), std::string(),
                    std::string(), base::Time::Now(), false, false);
  EXPECT_EQ(0, prefs.GetInt64(prefs::kVariationsLastFetchTime));

  // Check that field trials are created from the seed. Since the test study has
  // only 1 experiment with 100% probability weight, we must be part of it.
  EXPECT_TRUE(service.CreateTrialsFromSeed(base::FeatureList::GetInstance()));
  EXPECT_EQ(base::FieldTrialList::FindFullName(kTestSeedStudyName),
            kTestSeedExperimentName);
}

TEST_F(VariationsServiceTest, CreateTrialsFromOutdatedSeed) {
  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());

  // Create a local base::FieldTrialList, to hold the field trials created in
  // this test.
  base::FieldTrialList field_trial_list(nullptr);

  // Create a variations service.
  TestVariationsService service(
      base::MakeUnique<web_resource::TestRequestAllowedNotifier>(&prefs),
      &prefs);
  service.SetCreateTrialsFromSeedCalledForTesting(false);

  // Store a seed, with a fetch time 31 days in the past.
  const base::Time seed_date =
      base::Time::Now() - base::TimeDelta::FromDays(31);
  service.StoreSeed(SerializeSeed(CreateTestSeed()), std::string(),
                    std::string(), seed_date, false, false);
  prefs.SetInt64(prefs::kVariationsLastFetchTime, seed_date.ToInternalValue());

  // Check that field trials are not created from the seed.
  EXPECT_FALSE(service.CreateTrialsFromSeed(base::FeatureList::GetInstance()));
  EXPECT_TRUE(base::FieldTrialList::FindFullName(kTestSeedStudyName).empty());
}

TEST_F(VariationsServiceTest, GetVariationsServerURL) {
  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());
  const std::string default_variations_url =
      VariationsService::GetDefaultVariationsServerURLForTesting();

  std::string value;
  std::unique_ptr<TestVariationsServiceClient> client =
      base::MakeUnique<TestVariationsServiceClient>();
  TestVariationsServiceClient* raw_client = client.get();
  VariationsService service(
      std::move(client),
      base::MakeUnique<web_resource::TestRequestAllowedNotifier>(&prefs),
      &prefs, NULL, UIStringOverrider());
  GURL url = service.GetVariationsServerURL(&prefs, std::string());
  EXPECT_TRUE(base::StartsWith(url.spec(), default_variations_url,
                               base::CompareCase::SENSITIVE));
  EXPECT_FALSE(net::GetValueForKeyInQuery(url, "restrict", &value));

  prefs.SetString(prefs::kVariationsRestrictParameter, "restricted");
  url = service.GetVariationsServerURL(&prefs, std::string());
  EXPECT_TRUE(base::StartsWith(url.spec(), default_variations_url,
                               base::CompareCase::SENSITIVE));
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "restrict", &value));
  EXPECT_EQ("restricted", value);

  // A client override should take precedence over what's in prefs.
  raw_client->set_restrict_parameter("client");
  url = service.GetVariationsServerURL(&prefs, std::string());
  EXPECT_TRUE(base::StartsWith(url.spec(), default_variations_url,
                               base::CompareCase::SENSITIVE));
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "restrict", &value));
  EXPECT_EQ("client", value);

  // The override value passed to the method should take precedence over
  // what's in prefs and a client override.
  url = service.GetVariationsServerURL(&prefs, "override");
  EXPECT_TRUE(base::StartsWith(url.spec(), default_variations_url,
                               base::CompareCase::SENSITIVE));
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "restrict", &value));
  EXPECT_EQ("override", value);
}

TEST_F(VariationsServiceTest, VariationsURLHasOSNameParam) {
  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());
  TestVariationsService service(
      base::MakeUnique<web_resource::TestRequestAllowedNotifier>(&prefs),
      &prefs);
  const GURL url = service.GetVariationsServerURL(&prefs, std::string());

  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "osname", &value));
  EXPECT_FALSE(value.empty());
}

TEST_F(VariationsServiceTest, RequestsInitiallyNotAllowed) {
  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());

  // Pass ownership to TestVariationsService, but keep a weak pointer to
  // manipulate it for this test.
  std::unique_ptr<web_resource::TestRequestAllowedNotifier> test_notifier =
      base::MakeUnique<web_resource::TestRequestAllowedNotifier>(&prefs);
  web_resource::TestRequestAllowedNotifier* raw_notifier = test_notifier.get();
  TestVariationsService test_service(std::move(test_notifier), &prefs);

  // Force the notifier to initially disallow requests.
  raw_notifier->SetRequestsAllowedOverride(false);
  test_service.StartRepeatedVariationsSeedFetch();
  EXPECT_FALSE(test_service.fetch_attempted());

  raw_notifier->NotifyObserver();
  EXPECT_TRUE(test_service.fetch_attempted());
}

TEST_F(VariationsServiceTest, RequestsInitiallyAllowed) {
  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());

  // Pass ownership to TestVariationsService, but keep a weak pointer to
  // manipulate it for this test.
  std::unique_ptr<web_resource::TestRequestAllowedNotifier> test_notifier =
      base::MakeUnique<web_resource::TestRequestAllowedNotifier>(&prefs);
  web_resource::TestRequestAllowedNotifier* raw_notifier = test_notifier.get();
  TestVariationsService test_service(std::move(test_notifier), &prefs);

  raw_notifier->SetRequestsAllowedOverride(true);
  test_service.StartRepeatedVariationsSeedFetch();
  EXPECT_TRUE(test_service.fetch_attempted());
}

TEST_F(VariationsServiceTest, SeedStoredWhenOKStatus) {
  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());

  TestVariationsService service(
      base::MakeUnique<web_resource::TestRequestAllowedNotifier>(&prefs),
      &prefs);
  service.set_intercepts_fetch(false);

  net::TestURLFetcherFactory factory;
  service.DoActualFetch();

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  SimulateServerResponse(net::HTTP_OK, fetcher);
  fetcher->SetResponseString(SerializeSeed(CreateTestSeed()));

  EXPECT_FALSE(service.seed_stored());
  service.OnURLFetchComplete(fetcher);
  EXPECT_TRUE(service.seed_stored());
}

TEST_F(VariationsServiceTest, SeedNotStoredWhenNonOKStatus) {
  const int non_ok_status_codes[] = {
    net::HTTP_NO_CONTENT,
    net::HTTP_NOT_MODIFIED,
    net::HTTP_NOT_FOUND,
    net::HTTP_INTERNAL_SERVER_ERROR,
    net::HTTP_SERVICE_UNAVAILABLE,
  };

  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());

  TestVariationsService service(
      base::MakeUnique<web_resource::TestRequestAllowedNotifier>(&prefs),
      &prefs);
  service.set_intercepts_fetch(false);
  for (size_t i = 0; i < arraysize(non_ok_status_codes); ++i) {
    net::TestURLFetcherFactory factory;
    service.DoActualFetch();
    EXPECT_TRUE(prefs.FindPreference(prefs::kVariationsSeed)->IsDefaultValue());

    net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
    SimulateServerResponse(non_ok_status_codes[i], fetcher);
    service.OnURLFetchComplete(fetcher);

    EXPECT_TRUE(prefs.FindPreference(prefs::kVariationsSeed)->IsDefaultValue());
  }
}

TEST_F(VariationsServiceTest, RequestGzipCompressedSeed) {
  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());
  net::TestURLFetcherFactory factory;

  TestVariationsService service(
      base::MakeUnique<web_resource::TestRequestAllowedNotifier>(&prefs),
      &prefs);
  service.set_intercepts_fetch(false);
  service.DoActualFetch();

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  net::HttpRequestHeaders headers;
  fetcher->GetExtraRequestHeaders(&headers);
  std::string field;
  ASSERT_TRUE(headers.GetHeader("A-IM", &field));
  EXPECT_EQ("gzip", field);
}

TEST_F(VariationsServiceTest, InstanceManipulations) {
  struct  {
    std::string im;
    bool delta_compressed;
    bool gzip_compressed;
    bool seed_stored;
  } cases[] = {
    {"", false, false, true},
    {"IM:gzip", false, true, true},
    {"IM:x-bm", true, false, true},
    {"IM:x-bm,gzip", true, true, true},
    {"IM: x-bm, gzip", true, true, true},
    {"IM:gzip,x-bm", false, false, false},
    {"IM:deflate,x-bm,gzip", false, false, false},
  };

  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());
  std::string serialized_seed = SerializeSeed(CreateTestSeed());
  net::TestURLFetcherFactory factory;

  for (size_t i = 0; i < arraysize(cases); ++i) {
    TestVariationsService service(
        base::MakeUnique<web_resource::TestRequestAllowedNotifier>(&prefs),
        &prefs);
    service.set_intercepts_fetch(false);
    service.DoActualFetch();
    net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);

    if (cases[i].im.empty())
      SimulateServerResponse(net::HTTP_OK, fetcher);
    else
      SimulateServerResponseWithHeader(net::HTTP_OK, fetcher, &cases[i].im);
    fetcher->SetResponseString(serialized_seed);
    service.OnURLFetchComplete(fetcher);

    EXPECT_EQ(cases[i].seed_stored, service.seed_stored());
    EXPECT_EQ(cases[i].delta_compressed, service.delta_compressed_seed());
    EXPECT_EQ(cases[i].gzip_compressed, service.gzip_compressed_seed());
  }
}

TEST_F(VariationsServiceTest, CountryHeader) {
  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());

  TestVariationsService service(
      base::MakeUnique<web_resource::TestRequestAllowedNotifier>(&prefs),
      &prefs);
  service.set_intercepts_fetch(false);

  net::TestURLFetcherFactory factory;
  service.DoActualFetch();

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  scoped_refptr<net::HttpResponseHeaders> headers =
      SimulateServerResponse(net::HTTP_OK, fetcher);
  headers->AddHeader("X-Country: test");
  fetcher->SetResponseString(SerializeSeed(CreateTestSeed()));

  EXPECT_FALSE(service.seed_stored());
  service.OnURLFetchComplete(fetcher);
  EXPECT_TRUE(service.seed_stored());
  EXPECT_EQ("test", service.stored_country());
}

TEST_F(VariationsServiceTest, Observer) {
  TestingPrefServiceSimple prefs;
  VariationsService::RegisterPrefs(prefs.registry());
  VariationsService service(
      base::MakeUnique<TestVariationsServiceClient>(),
      base::MakeUnique<web_resource::TestRequestAllowedNotifier>(&prefs),
      &prefs, NULL, UIStringOverrider());

  struct {
    int normal_count;
    int best_effort_count;
    int critical_count;
    int expected_best_effort_notifications;
    int expected_crtical_notifications;
  } cases[] = {
      {0, 0, 0, 0, 0},
      {1, 0, 0, 0, 0},
      {10, 0, 0, 0, 0},
      {0, 1, 0, 1, 0},
      {0, 10, 0, 1, 0},
      {0, 0, 1, 0, 1},
      {0, 0, 10, 0, 1},
      {0, 1, 1, 0, 1},
      {1, 1, 1, 0, 1},
      {1, 1, 0, 1, 0},
      {1, 0, 1, 0, 1},
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    TestVariationsServiceObserver observer;
    service.AddObserver(&observer);

    variations::VariationsSeedSimulator::Result result;
    result.normal_group_change_count = cases[i].normal_count;
    result.kill_best_effort_group_change_count = cases[i].best_effort_count;
    result.kill_critical_group_change_count = cases[i].critical_count;
    service.NotifyObservers(result);

    EXPECT_EQ(cases[i].expected_best_effort_notifications,
              observer.best_effort_changes_notified()) << i;
    EXPECT_EQ(cases[i].expected_crtical_notifications,
              observer.crticial_changes_notified()) << i;

    service.RemoveObserver(&observer);
  }
}

TEST_F(VariationsServiceTest, LoadPermanentConsistencyCountry) {
  struct {
    // Comma separated list, NULL if the pref isn't set initially.
    const char* pref_value_before;
    const char* version;
    // NULL indicates that no latest country code is present.
    const char* latest_country_code;
    // Comma separated list.
    const char* expected_pref_value_after;
    std::string expected_country;
    VariationsService::LoadPermanentConsistencyCountryResult expected_result;
  } test_cases[] = {
      // Existing pref value present for this version.
      {"20.0.0.0,us", "20.0.0.0", "ca", "20.0.0.0,us", "us",
       VariationsService::LOAD_COUNTRY_HAS_BOTH_VERSION_EQ_COUNTRY_NEQ},
      {"20.0.0.0,us", "20.0.0.0", "us", "20.0.0.0,us", "us",
       VariationsService::LOAD_COUNTRY_HAS_BOTH_VERSION_EQ_COUNTRY_EQ},
      {"20.0.0.0,us", "20.0.0.0", nullptr, "20.0.0.0,us", "us",
       VariationsService::LOAD_COUNTRY_HAS_PREF_NO_SEED_VERSION_EQ},

      // Existing pref value present for a different version.
      {"19.0.0.0,ca", "20.0.0.0", "us", "20.0.0.0,us", "us",
       VariationsService::LOAD_COUNTRY_HAS_BOTH_VERSION_NEQ_COUNTRY_NEQ},
      {"19.0.0.0,us", "20.0.0.0", "us", "20.0.0.0,us", "us",
       VariationsService::LOAD_COUNTRY_HAS_BOTH_VERSION_NEQ_COUNTRY_EQ},
      {"19.0.0.0,ca", "20.0.0.0", nullptr, "19.0.0.0,ca", "",
       VariationsService::LOAD_COUNTRY_HAS_PREF_NO_SEED_VERSION_NEQ},

      // No existing pref value present.
      {nullptr, "20.0.0.0", "us", "20.0.0.0,us", "us",
       VariationsService::LOAD_COUNTRY_NO_PREF_HAS_SEED},
      {nullptr, "20.0.0.0", nullptr, "", "",
       VariationsService::LOAD_COUNTRY_NO_PREF_NO_SEED},
      {"", "20.0.0.0", "us", "20.0.0.0,us", "us",
       VariationsService::LOAD_COUNTRY_NO_PREF_HAS_SEED},
      {"", "20.0.0.0", nullptr, "", "",
       VariationsService::LOAD_COUNTRY_NO_PREF_NO_SEED},

      // Invalid existing pref value.
      {"20.0.0.0", "20.0.0.0", "us", "20.0.0.0,us", "us",
       VariationsService::LOAD_COUNTRY_INVALID_PREF_HAS_SEED},
      {"20.0.0.0", "20.0.0.0", nullptr, "", "",
       VariationsService::LOAD_COUNTRY_INVALID_PREF_NO_SEED},
      {"20.0.0.0,us,element3", "20.0.0.0", "us", "20.0.0.0,us", "us",
       VariationsService::LOAD_COUNTRY_INVALID_PREF_HAS_SEED},
      {"20.0.0.0,us,element3", "20.0.0.0", nullptr, "", "",
       VariationsService::LOAD_COUNTRY_INVALID_PREF_NO_SEED},
      {"badversion,ca", "20.0.0.0", "us", "20.0.0.0,us", "us",
       VariationsService::LOAD_COUNTRY_INVALID_PREF_HAS_SEED},
      {"badversion,ca", "20.0.0.0", nullptr, "", "",
       VariationsService::LOAD_COUNTRY_INVALID_PREF_NO_SEED},
  };

  for (const auto& test : test_cases) {
    TestingPrefServiceSimple prefs;
    VariationsService::RegisterPrefs(prefs.registry());
    VariationsService service(
        base::MakeUnique<TestVariationsServiceClient>(),
        base::MakeUnique<web_resource::TestRequestAllowedNotifier>(&prefs),
        &prefs, NULL, UIStringOverrider());

    if (test.pref_value_before) {
      base::ListValue list_value;
      for (const std::string& component :
           base::SplitString(test.pref_value_before, ",", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_ALL)) {
        list_value.AppendString(component);
      }
      prefs.Set(prefs::kVariationsPermanentConsistencyCountry, list_value);
    }

    variations::VariationsSeed seed(CreateTestSeed());
    std::string latest_country;
    if (test.latest_country_code)
      latest_country = test.latest_country_code;

    base::HistogramTester histogram_tester;
    EXPECT_EQ(test.expected_country,
              service.LoadPermanentConsistencyCountry(
                  base::Version(test.version), latest_country))
        << test.pref_value_before << ", " << test.version << ", "
        << test.latest_country_code;

    base::ListValue expected_list_value;
    for (const std::string& component :
         base::SplitString(test.expected_pref_value_after, ",",
                           base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      expected_list_value.AppendString(component);
    }
    const base::ListValue* pref_value =
        prefs.GetList(prefs::kVariationsPermanentConsistencyCountry);
    EXPECT_EQ(ListValueToString(expected_list_value),
              ListValueToString(*pref_value))
        << test.pref_value_before << ", " << test.version << ", "
        << test.latest_country_code;

    histogram_tester.ExpectUniqueSample(
        "Variations.LoadPermanentConsistencyCountryResult",
        test.expected_result, 1);
  }
}

TEST_F(VariationsServiceTest, OverrideStoredPermanentCountry) {
  const std::string kTestVersion = version_info::GetVersionNumber();
  const std::string kPrefCa = version_info::GetVersionNumber() + ",ca";
  const std::string kPrefUs = version_info::GetVersionNumber() + ",us";

  struct {
    // Comma separated list, empty string if the pref isn't set initially.
    const std::string pref_value_before;
    const std::string country_code_override;
    // Comma separated list.
    const std::string expected_pref_value_after;
    // Is the pref expected to be updated or not.
    const bool has_updated;
  } test_cases[] = {
      {kPrefUs, "ca", kPrefCa, true},
      {kPrefUs, "us", kPrefUs, false},
      {kPrefUs, "", kPrefUs, false},
      {"", "ca", kPrefCa, true},
      {"", "", "", false},
      {"19.0.0.0,us", "ca", kPrefCa, true},
      {"19.0.0.0,us", "us", "19.0.0.0,us", false},
  };

  for (const auto& test : test_cases) {
    TestingPrefServiceSimple prefs;
    VariationsService::RegisterPrefs(prefs.registry());
    TestVariationsService service(
        base::MakeUnique<web_resource::TestRequestAllowedNotifier>(&prefs),
        &prefs);

    if (!test.pref_value_before.empty()) {
      base::ListValue list_value;
      for (const std::string& component :
           base::SplitString(test.pref_value_before, ",", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_ALL)) {
        list_value.AppendString(component);
      }
      prefs.Set(prefs::kVariationsPermanentConsistencyCountry, list_value);
    }

    variations::VariationsSeed seed(CreateTestSeed());

    EXPECT_EQ(test.has_updated, service.OverrideStoredPermanentCountry(
                                    test.country_code_override))
        << test.pref_value_before << ", " << test.country_code_override;

    base::ListValue expected_list_value;
    for (const std::string& component :
         base::SplitString(test.expected_pref_value_after, ",",
                           base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      expected_list_value.AppendString(component);
    }
    const base::ListValue* pref_value =
        prefs.GetList(prefs::kVariationsPermanentConsistencyCountry);
    EXPECT_EQ(ListValueToString(expected_list_value),
              ListValueToString(*pref_value))
        << test.pref_value_before << ", " << test.country_code_override;
  }
}

}  // namespace variations
