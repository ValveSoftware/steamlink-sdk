// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/network_location_provider.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/geolocation/fake_access_token_store.h"
#include "content/browser/geolocation/location_arbitrator_impl.h"
#include "content/browser/geolocation/wifi_data_provider.h"
#include "net/base/net_errors.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// Constants used in multiple tests.
const char kTestServerUrl[] = "https://www.geolocation.test/service";
const char kAccessTokenString[] = "accessToken";

// Using #define so we can easily paste this into various other strings.
#define REFERENCE_ACCESS_TOKEN "2:k7j3G6LaL6u_lafw:4iXOeOpTh1glSXe"

// Stops the specified (nested) message loop when the listener is called back.
class MessageLoopQuitListener {
 public:
  MessageLoopQuitListener()
      : client_message_loop_(base::MessageLoop::current()),
        updated_provider_(NULL) {
    CHECK(client_message_loop_);
  }

  void OnLocationUpdate(const LocationProvider* provider,
                        const Geoposition& position) {
    EXPECT_EQ(client_message_loop_, base::MessageLoop::current());
    updated_provider_ = provider;
    client_message_loop_->QuitWhenIdle();
  }

  base::MessageLoop* client_message_loop_;
  const LocationProvider* updated_provider_;
};

// A mock implementation of WifiDataProvider for testing. Adapted from
// http://gears.googlecode.com/svn/trunk/gears/geolocation/geolocation_test.cc
class MockWifiDataProvider : public WifiDataProvider {
 public:
  // Factory method for use with WifiDataProvider::SetFactoryForTesting.
  static WifiDataProvider* GetInstance() {
    CHECK(instance_);
    return instance_;
  }

  static MockWifiDataProvider* CreateInstance() {
    CHECK(!instance_);
    instance_ = new MockWifiDataProvider;
    return instance_;
  }

  MockWifiDataProvider() : start_calls_(0), stop_calls_(0), got_data_(true) {}

  // WifiDataProvider implementation.
  void StartDataProvider() override { ++start_calls_; }

  void StopDataProvider() override { ++stop_calls_; }

  bool GetData(WifiData* data_out) override {
    CHECK(data_out);
    *data_out = data_;
    return got_data_;
  }

  void SetData(const WifiData& new_data) {
    got_data_ = true;
    const bool differs = data_.DiffersSignificantly(new_data);
    data_ = new_data;
    if (differs)
      this->RunCallbacks();
  }

  void set_got_data(bool got_data) { got_data_ = got_data; }
  int start_calls_;
  int stop_calls_;

 private:
  ~MockWifiDataProvider() override {
    CHECK(this == instance_);
    instance_ = NULL;
  }

  static MockWifiDataProvider* instance_;

  WifiData data_;
  bool got_data_;

  DISALLOW_COPY_AND_ASSIGN(MockWifiDataProvider);
};

MockWifiDataProvider* MockWifiDataProvider::instance_ = NULL;

// Main test fixture
class GeolocationNetworkProviderTest : public testing::Test {
 public:
  void SetUp() override {
    test_server_url_ = GURL(kTestServerUrl);
    access_token_store_ = new FakeAccessTokenStore;
    wifi_data_provider_ = MockWifiDataProvider::CreateInstance();
  }

  void TearDown() override {
    WifiDataProviderManager::ResetFactoryForTesting();
  }

  LocationProvider* CreateProvider(bool set_permission_granted) {
    LocationProvider* provider = NewNetworkLocationProvider(
        access_token_store_.get(),
        NULL,  // No URLContextGetter needed, as using test urlfecther factory.
        test_server_url_,
        access_token_store_->access_token_map_[test_server_url_]);
    if (set_permission_granted)
      provider->OnPermissionGranted();
    return provider;
  }

 protected:
  GeolocationNetworkProviderTest() {
    // TODO(joth): Really these should be in SetUp, not here, but they take no
    // effect on Mac OS Release builds if done there. I kid not. Figure out why.
    WifiDataProviderManager::SetFactoryForTesting(
        MockWifiDataProvider::GetInstance);
  }

  // Returns the current url fetcher (if any) and advances the id ready for the
  // next test step.
  net::TestURLFetcher* get_url_fetcher_and_advance_id() {
    net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(
            NetworkLocationRequest::url_fetcher_id_for_tests);
    if (fetcher)
      ++NetworkLocationRequest::url_fetcher_id_for_tests;
    return fetcher;
  }

  static int IndexToChannel(int index) { return index + 4; }

  // Creates wifi data containing the specified number of access points, with
  // some differentiating charactistics in each.
  static WifiData CreateReferenceWifiScanData(int ap_count) {
    WifiData data;
    for (int i = 0; i < ap_count; ++i) {
      AccessPointData ap;
      ap.mac_address =
          base::ASCIIToUTF16(base::StringPrintf("%02d-34-56-78-54-32", i));
      ap.radio_signal_strength = ap_count - i;
      ap.channel = IndexToChannel(i);
      ap.signal_to_noise = i + 42;
      ap.ssid = base::ASCIIToUTF16("Some nice+network|name\\");
      data.access_point_data.insert(ap);
    }
    return data;
  }

  static void CreateReferenceWifiScanDataJson(
      int ap_count, int start_index, base::ListValue* wifi_access_point_list) {
    std::vector<std::string> wifi_data;
    for (int i = 0; i < ap_count; ++i) {
      std::unique_ptr<base::DictionaryValue> ap(new base::DictionaryValue());
      ap->SetString("macAddress", base::StringPrintf("%02d-34-56-78-54-32", i));
      ap->SetInteger("signalStrength", start_index + ap_count - i);
      ap->SetInteger("age", 0);
      ap->SetInteger("channel", IndexToChannel(i));
      ap->SetInteger("signalToNoiseRatio", i + 42);
      wifi_access_point_list->Append(std::move(ap));
    }
  }

  static Geoposition CreateReferencePosition(int id) {
    Geoposition pos;
    pos.latitude = id;
    pos.longitude = -(id + 1);
    pos.altitude = 2 * id;
    pos.timestamp = base::Time::Now();
    return pos;
  }

  static std::string PrettyJson(const base::Value& value) {
    std::string pretty;
    base::JSONWriter::WriteWithOptions(
        value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &pretty);
    return pretty;
  }

  static testing::AssertionResult JsonGetList(
      const std::string& field,
      const base::DictionaryValue& dict,
      const base::ListValue** output_list) {
    if (!dict.GetList(field, output_list))
      return testing::AssertionFailure() << "Dictionary " << PrettyJson(dict)
                                         << " is missing list field " << field;
    return testing::AssertionSuccess();
  }

  static testing::AssertionResult JsonFieldEquals(
      const std::string& field,
      const base::DictionaryValue& expected,
      const base::DictionaryValue& actual) {
    const base::Value* expected_value;
    const base::Value* actual_value;
    if (!expected.Get(field, &expected_value))
      return testing::AssertionFailure()
          << "Expected dictionary " << PrettyJson(expected)
          << " is missing field " << field;
    if (!expected.Get(field, &actual_value))
      return testing::AssertionFailure()
          << "Actual dictionary " << PrettyJson(actual)
          << " is missing field " << field;
    if (!expected_value->Equals(actual_value))
      return testing::AssertionFailure()
          << "Field " << field << " mismatch: " << PrettyJson(*expected_value)
          << " != " << PrettyJson(*actual_value);
    return testing::AssertionSuccess();
  }

  static GURL UrlWithoutQuery(const GURL& url) {
    url::Replacements<char> replacements;
    replacements.ClearQuery();
    return url.ReplaceComponents(replacements);
  }

  testing::AssertionResult IsTestServerUrl(const GURL& request_url) {
    const GURL a(UrlWithoutQuery(test_server_url_));
    const GURL b(UrlWithoutQuery(request_url));
    if (a == b)
      return testing::AssertionSuccess();
    return testing::AssertionFailure() << a << " != " << b;
  }

  void CheckRequestIsValid(const net::TestURLFetcher& request,
                           int expected_routers,
                           int expected_wifi_aps,
                           int wifi_start_index,
                           const std::string& expected_access_token) {
    const GURL& request_url = request.GetOriginalURL();

    EXPECT_TRUE(IsTestServerUrl(request_url));

    // Check to see that the api key is being appended for the default
    // network provider url.
    bool is_default_url = UrlWithoutQuery(request_url) ==
        UrlWithoutQuery(LocationArbitratorImpl::DefaultNetworkProviderURL());
    EXPECT_EQ(is_default_url, !request_url.query().empty());

    const std::string& upload_data = request.upload_data();
    ASSERT_FALSE(upload_data.empty());
    std::string json_parse_error_msg;
    std::unique_ptr<base::Value> parsed_json =
        base::JSONReader::ReadAndReturnError(upload_data, base::JSON_PARSE_RFC,
                                             NULL, &json_parse_error_msg);
    EXPECT_TRUE(json_parse_error_msg.empty());
    ASSERT_TRUE(parsed_json.get() != NULL);

    const base::DictionaryValue* request_json;
    ASSERT_TRUE(parsed_json->GetAsDictionary(&request_json));

    if (!is_default_url) {
      if (expected_access_token.empty())
        ASSERT_FALSE(request_json->HasKey(kAccessTokenString));
      else {
        std::string access_token;
        EXPECT_TRUE(request_json->GetString(kAccessTokenString, &access_token));
        EXPECT_EQ(expected_access_token, access_token);
      }
    }

    if (expected_wifi_aps) {
      base::ListValue expected_wifi_aps_json;
      CreateReferenceWifiScanDataJson(
          expected_wifi_aps,
          wifi_start_index,
          &expected_wifi_aps_json);
      EXPECT_EQ(size_t(expected_wifi_aps), expected_wifi_aps_json.GetSize());

      const base::ListValue* wifi_aps_json;
      ASSERT_TRUE(JsonGetList("wifiAccessPoints", *request_json,
                              &wifi_aps_json));
      for (size_t i = 0; i < expected_wifi_aps_json.GetSize(); ++i ) {
        const base::DictionaryValue* expected_json;
        ASSERT_TRUE(expected_wifi_aps_json.GetDictionary(i, &expected_json));
        const base::DictionaryValue* actual_json;
        ASSERT_TRUE(wifi_aps_json->GetDictionary(i, &actual_json));
        ASSERT_TRUE(JsonFieldEquals("macAddress", *expected_json,
                                    *actual_json));
        ASSERT_TRUE(JsonFieldEquals("signalStrength", *expected_json,
                                    *actual_json));
        ASSERT_TRUE(JsonFieldEquals("channel", *expected_json, *actual_json));
        ASSERT_TRUE(JsonFieldEquals("signalToNoiseRatio", *expected_json,
                                    *actual_json));
      }
    } else {
      ASSERT_FALSE(request_json->HasKey("wifiAccessPoints"));
    }
    EXPECT_TRUE(request_url.is_valid());
  }

  GURL test_server_url_;
  base::MessageLoop main_message_loop_;
  scoped_refptr<FakeAccessTokenStore> access_token_store_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  scoped_refptr<MockWifiDataProvider> wifi_data_provider_;
};

TEST_F(GeolocationNetworkProviderTest, CreateDestroy) {
  // Test fixture members were SetUp correctly.
  EXPECT_EQ(&main_message_loop_, base::MessageLoop::current());
  std::unique_ptr<LocationProvider> provider(CreateProvider(true));
  EXPECT_TRUE(NULL != provider.get());
  provider.reset();
  SUCCEED();
}

TEST_F(GeolocationNetworkProviderTest, StartProvider) {
  std::unique_ptr<LocationProvider> provider(CreateProvider(true));
  EXPECT_TRUE(provider->StartProvider(false));
  net::TestURLFetcher* fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher != NULL);
  CheckRequestIsValid(*fetcher, 0, 0, 0, std::string());
}

TEST_F(GeolocationNetworkProviderTest, StartProviderDefaultUrl) {
  test_server_url_ = LocationArbitratorImpl::DefaultNetworkProviderURL();
  std::unique_ptr<LocationProvider> provider(CreateProvider(true));
  EXPECT_TRUE(provider->StartProvider(false));
  net::TestURLFetcher* fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher != NULL);
  CheckRequestIsValid(*fetcher, 0, 0, 0, std::string());
}

TEST_F(GeolocationNetworkProviderTest, StartProviderLongRequest) {
  std::unique_ptr<LocationProvider> provider(CreateProvider(true));
  EXPECT_TRUE(provider->StartProvider(false));
  const int kFirstScanAps = 20;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kFirstScanAps));
  main_message_loop_.RunUntilIdle();
  net::TestURLFetcher* fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher != NULL);
  // The request url should have been shortened to less than 2048 characters
  // in length by not including access points with the lowest signal strength
  // in the request.
  EXPECT_LT(fetcher->GetOriginalURL().spec().size(), size_t(2048));
  CheckRequestIsValid(*fetcher, 0, 16, 4, std::string());
}

TEST_F(GeolocationNetworkProviderTest, MultipleWifiScansComplete) {
  std::unique_ptr<LocationProvider> provider(CreateProvider(true));
  EXPECT_TRUE(provider->StartProvider(false));

  net::TestURLFetcher* fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher != NULL);
  EXPECT_TRUE(IsTestServerUrl(fetcher->GetOriginalURL()));

  // Complete the network request with bad position fix.
  const char* kNoFixNetworkResponse =
      "{"
      "  \"status\": \"ZERO_RESULTS\""
      "}";
  fetcher->set_url(test_server_url_);
  fetcher->set_status(net::URLRequestStatus());
  fetcher->set_response_code(200);  // OK
  fetcher->SetResponseString(kNoFixNetworkResponse);
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  Geoposition position;
  provider->GetPosition(&position);
  EXPECT_FALSE(position.Validate());

  // Now wifi data arrives -- SetData will notify listeners.
  const int kFirstScanAps = 6;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kFirstScanAps));
  main_message_loop_.RunUntilIdle();
  fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher != NULL);
  // The request should have the wifi data.
  CheckRequestIsValid(*fetcher, 0, kFirstScanAps, 0, std::string());

  // Send a reply with good position fix.
  const char* kReferenceNetworkResponse =
      "{"
      "  \"accessToken\": \"" REFERENCE_ACCESS_TOKEN "\","
      "  \"accuracy\": 1200.4,"
      "  \"location\": {"
      "    \"lat\": 51.0,"
      "    \"lng\": -0.1"
      "  }"
      "}";
  fetcher->set_url(test_server_url_);
  fetcher->set_status(net::URLRequestStatus());
  fetcher->set_response_code(200);  // OK
  fetcher->SetResponseString(kReferenceNetworkResponse);
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  provider->GetPosition(&position);
  EXPECT_EQ(51.0, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_EQ(1200.4, position.accuracy);
  EXPECT_FALSE(position.timestamp.is_null());
  EXPECT_TRUE(position.Validate());

  // Token should be in the store.
  EXPECT_EQ(base::UTF8ToUTF16(REFERENCE_ACCESS_TOKEN),
            access_token_store_->access_token_map_[test_server_url_]);

  // Wifi updated again, with one less AP. This is 'close enough' to the
  // previous scan, so no new request made.
  const int kSecondScanAps = kFirstScanAps - 1;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kSecondScanAps));
  main_message_loop_.RunUntilIdle();
  fetcher = get_url_fetcher_and_advance_id();
  EXPECT_FALSE(fetcher);

  provider->GetPosition(&position);
  EXPECT_EQ(51.0, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_TRUE(position.Validate());

  // Now a third scan with more than twice the original amount -> new request.
  const int kThirdScanAps = kFirstScanAps * 2 + 1;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kThirdScanAps));
  main_message_loop_.RunUntilIdle();
  fetcher = get_url_fetcher_and_advance_id();
  EXPECT_TRUE(fetcher);
  CheckRequestIsValid(*fetcher, 0, kThirdScanAps, 0, REFERENCE_ACCESS_TOKEN);
  // ...reply with a network error.

  fetcher->set_url(test_server_url_);
  fetcher->set_status(net::URLRequestStatus::FromError(net::ERR_FAILED));
  fetcher->set_response_code(200);  // should be ignored
  fetcher->SetResponseString(std::string());
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // Error means we now no longer have a fix.
  provider->GetPosition(&position);
  EXPECT_FALSE(position.Validate());

  // Wifi scan returns to original set: should be serviced from cache.
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kFirstScanAps));
  main_message_loop_.RunUntilIdle();
  EXPECT_FALSE(get_url_fetcher_and_advance_id());  // No new request created.

  provider->GetPosition(&position);
  EXPECT_EQ(51.0, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_TRUE(position.Validate());
}

TEST_F(GeolocationNetworkProviderTest, NoRequestOnStartupUntilWifiData) {
  MessageLoopQuitListener listener;
  wifi_data_provider_->set_got_data(false);
  std::unique_ptr<LocationProvider> provider(CreateProvider(true));
  EXPECT_TRUE(provider->StartProvider(false));

  provider->SetUpdateCallback(base::Bind(
      &MessageLoopQuitListener::OnLocationUpdate, base::Unretained(&listener)));

  main_message_loop_.RunUntilIdle();
  EXPECT_FALSE(get_url_fetcher_and_advance_id())
      << "Network request should not be created right away on startup when "
         "wifi data has not yet arrived";

  wifi_data_provider_->SetData(CreateReferenceWifiScanData(1));
  main_message_loop_.RunUntilIdle();
  EXPECT_TRUE(get_url_fetcher_and_advance_id());
}

TEST_F(GeolocationNetworkProviderTest, NewDataReplacesExistingNetworkRequest) {
  // Send initial request with empty data
  std::unique_ptr<LocationProvider> provider(CreateProvider(true));
  EXPECT_TRUE(provider->StartProvider(false));
  net::TestURLFetcher* fetcher = get_url_fetcher_and_advance_id();
  EXPECT_TRUE(fetcher);

  // Now wifi data arrives; new request should be sent.
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(4));
  main_message_loop_.RunUntilIdle();
  fetcher = get_url_fetcher_and_advance_id();
  EXPECT_TRUE(fetcher);
}

TEST_F(GeolocationNetworkProviderTest, NetworkRequestDeferredForPermission) {
  std::unique_ptr<LocationProvider> provider(CreateProvider(false));
  EXPECT_TRUE(provider->StartProvider(false));
  net::TestURLFetcher* fetcher = get_url_fetcher_and_advance_id();
  EXPECT_FALSE(fetcher);
  provider->OnPermissionGranted();

  fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher != NULL);

  EXPECT_TRUE(IsTestServerUrl(fetcher->GetOriginalURL()));
}

TEST_F(GeolocationNetworkProviderTest,
       NetworkRequestWithWifiDataDeferredForPermission) {
  access_token_store_->access_token_map_[test_server_url_] =
      base::UTF8ToUTF16(REFERENCE_ACCESS_TOKEN);
  std::unique_ptr<LocationProvider> provider(CreateProvider(false));
  EXPECT_TRUE(provider->StartProvider(false));
  net::TestURLFetcher* fetcher = get_url_fetcher_and_advance_id();
  EXPECT_FALSE(fetcher);

  static const int kScanCount = 4;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kScanCount));
  main_message_loop_.RunUntilIdle();

  fetcher = get_url_fetcher_and_advance_id();
  EXPECT_FALSE(fetcher);

  provider->OnPermissionGranted();

  fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher != NULL);

  CheckRequestIsValid(*fetcher, 0, kScanCount, 0, REFERENCE_ACCESS_TOKEN);
}

TEST_F(GeolocationNetworkProviderTest, NetworkPositionCache) {
  NetworkLocationProvider::PositionCache cache;

  const int kCacheSize = NetworkLocationProvider::PositionCache::kMaximumSize;
  for (int i = 1; i < kCacheSize * 2 + 1; ++i) {
    Geoposition pos = CreateReferencePosition(i);
    bool ret = cache.CachePosition(CreateReferenceWifiScanData(i), pos);
    EXPECT_TRUE(ret)  << i;
    const Geoposition* item =
        cache.FindPosition(CreateReferenceWifiScanData(i));
    ASSERT_TRUE(item) << i;
    EXPECT_EQ(pos.latitude, item->latitude)  << i;
    EXPECT_EQ(pos.longitude, item->longitude)  << i;
    if (i <= kCacheSize) {
      // Nothing should have spilled yet; check oldest item is still there.
      EXPECT_TRUE(cache.FindPosition(CreateReferenceWifiScanData(1)));
    } else {
      const int evicted = i - kCacheSize;
      EXPECT_FALSE(cache.FindPosition(CreateReferenceWifiScanData(evicted)));
      EXPECT_TRUE(cache.FindPosition(CreateReferenceWifiScanData(evicted + 1)));
    }
  }
}

}  // namespace content
