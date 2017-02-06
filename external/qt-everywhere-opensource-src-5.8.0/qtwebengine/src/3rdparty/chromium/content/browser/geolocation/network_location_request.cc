// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/network_location_request.h"

#include <stdint.h>

#include <limits>
#include <set>
#include <string>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/geolocation/location_arbitrator_impl.h"
#include "content/public/common/geoposition.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

namespace content {
namespace {

const char kAccessTokenString[] = "accessToken";
const char kLocationString[] = "location";
const char kLatitudeString[] = "lat";
const char kLongitudeString[] = "lng";
const char kAccuracyString[] = "accuracy";

enum NetworkLocationRequestEvent {
  // NOTE: Do not renumber these as that would confuse interpretation of
  // previously logged data. When making changes, also update the enum list
  // in tools/metrics/histograms/histograms.xml to keep it in sync.
  NETWORK_LOCATION_REQUEST_EVENT_REQUEST_START = 0,
  NETWORK_LOCATION_REQUEST_EVENT_REQUEST_CANCEL = 1,
  NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_SUCCESS = 2,
  NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_NOT_OK = 3,
  NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_EMPTY = 4,
  NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_MALFORMED = 5,
  NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_INVALID_FIX = 6,

  // NOTE: Add entries only immediately above this line.
  NETWORK_LOCATION_REQUEST_EVENT_COUNT = 7
};

void RecordUmaEvent(NetworkLocationRequestEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Geolocation.NetworkLocationRequest.Event",
      event, NETWORK_LOCATION_REQUEST_EVENT_COUNT);
}

void RecordUmaResponseCode(int code) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("Geolocation.NetworkLocationRequest.ResponseCode",
      code);
}

void RecordUmaAccessPoints(int count) {
  const int min = 0;
  const int max = 20;
  const int buckets = 21;
  UMA_HISTOGRAM_CUSTOM_COUNTS("Geolocation.NetworkLocationRequest.AccessPoints",
      count, min, max, buckets);
}

// Local functions
// Creates the request url to send to the server.
GURL FormRequestURL(const GURL& url);

void FormUploadData(const WifiData& wifi_data,
                    const base::Time& wifi_timestamp,
                    const base::string16& access_token,
                    std::string* upload_data);

// Attempts to extract a position from the response. Detects and indicates
// various failure cases.
void GetLocationFromResponse(bool http_post_result,
                             int status_code,
                             const std::string& response_body,
                             const base::Time& wifi_timestamp,
                             const GURL& server_url,
                             Geoposition* position,
                             base::string16* access_token);

// Parses the server response body. Returns true if parsing was successful.
// Sets |*position| to the parsed location if a valid fix was received,
// otherwise leaves it unchanged.
bool ParseServerResponse(const std::string& response_body,
                         const base::Time& wifi_timestamp,
                         Geoposition* position,
                         base::string16* access_token);
void AddWifiData(const WifiData& wifi_data,
                 int age_milliseconds,
                 base::DictionaryValue* request);
}  // namespace

int NetworkLocationRequest::url_fetcher_id_for_tests = 0;

NetworkLocationRequest::NetworkLocationRequest(
    net::URLRequestContextGetter* context,
    const GURL& url,
    LocationResponseCallback callback)
    : url_context_(context), location_response_callback_(callback), url_(url) {
}

NetworkLocationRequest::~NetworkLocationRequest() {
}

bool NetworkLocationRequest::MakeRequest(const base::string16& access_token,
                                         const WifiData& wifi_data,
                                         const base::Time& wifi_timestamp) {
  RecordUmaEvent(NETWORK_LOCATION_REQUEST_EVENT_REQUEST_START);
  RecordUmaAccessPoints(wifi_data.access_point_data.size());
  if (url_fetcher_ != NULL) {
    DVLOG(1) << "NetworkLocationRequest : Cancelling pending request";
    RecordUmaEvent(NETWORK_LOCATION_REQUEST_EVENT_REQUEST_CANCEL);
    url_fetcher_.reset();
  }
  wifi_data_ = wifi_data;
  wifi_timestamp_ = wifi_timestamp;

  GURL request_url = FormRequestURL(url_);
  url_fetcher_ = net::URLFetcher::Create(url_fetcher_id_for_tests, request_url,
                                         net::URLFetcher::POST, this);
  url_fetcher_->SetRequestContext(url_context_.get());
  std::string upload_data;
  FormUploadData(wifi_data, wifi_timestamp, access_token, &upload_data);
  url_fetcher_->SetUploadData("application/json", upload_data);
  url_fetcher_->SetLoadFlags(
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
      net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_COOKIES |
      net::LOAD_DO_NOT_SEND_AUTH_DATA);

  request_start_time_ = base::TimeTicks::Now();
  url_fetcher_->Start();
  return true;
}

void NetworkLocationRequest::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK_EQ(url_fetcher_.get(), source);

  net::URLRequestStatus status = source->GetStatus();
  int response_code = source->GetResponseCode();
  RecordUmaResponseCode(response_code);

  Geoposition position;
  base::string16 access_token;
  std::string data;
  source->GetResponseAsString(&data);
  GetLocationFromResponse(status.is_success(), response_code, data,
                          wifi_timestamp_, source->GetURL(), &position,
                          &access_token);
  const bool server_error =
      !status.is_success() || (response_code >= 500 && response_code < 600);
  url_fetcher_.reset();

  if (!server_error) {
    const base::TimeDelta request_time =
        base::TimeTicks::Now() - request_start_time_;

    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Net.Wifi.LbsLatency",
        request_time,
        base::TimeDelta::FromMilliseconds(1),
        base::TimeDelta::FromSeconds(10),
        100);
  }

  DVLOG(1) << "NetworkLocationRequest::OnURLFetchComplete() : run callback.";
  location_response_callback_.Run(
      position, server_error, access_token, wifi_data_);
}

// Local functions.
namespace {

struct AccessPointLess {
  bool operator()(const AccessPointData* ap1,
                  const AccessPointData* ap2) const {
    return ap2->radio_signal_strength < ap1->radio_signal_strength;
  }
};

GURL FormRequestURL(const GURL& url) {
  if (url == LocationArbitratorImpl::DefaultNetworkProviderURL()) {
    std::string api_key = google_apis::GetAPIKey();
    if (!api_key.empty()) {
      std::string query(url.query());
      if (!query.empty())
        query += "&";
      query += "key=" + net::EscapeQueryParamValue(api_key, true);
      GURL::Replacements replacements;
      replacements.SetQueryStr(query);
      return url.ReplaceComponents(replacements);
    }
  }
  return url;
}

void FormUploadData(const WifiData& wifi_data,
                    const base::Time& wifi_timestamp,
                    const base::string16& access_token,
                    std::string* upload_data) {
  int age = std::numeric_limits<int32_t>::min();  // Invalid so AddInteger()
                                                  // will ignore.
  if (!wifi_timestamp.is_null()) {
    // Convert absolute timestamps into a relative age.
    int64_t delta_ms = (base::Time::Now() - wifi_timestamp).InMilliseconds();
    if (delta_ms >= 0 && delta_ms < std::numeric_limits<int32_t>::max())
      age = static_cast<int>(delta_ms);
  }

  base::DictionaryValue request;
  AddWifiData(wifi_data, age, &request);
  if (!access_token.empty())
    request.SetString(kAccessTokenString, access_token);
  base::JSONWriter::Write(request, upload_data);
}

void AddString(const std::string& property_name, const std::string& value,
               base::DictionaryValue* dict) {
  DCHECK(dict);
  if (!value.empty())
    dict->SetString(property_name, value);
}

void AddInteger(const std::string& property_name, int value,
                base::DictionaryValue* dict) {
  DCHECK(dict);
  if (value != std::numeric_limits<int32_t>::min())
    dict->SetInteger(property_name, value);
}

void AddWifiData(const WifiData& wifi_data,
                 int age_milliseconds,
                 base::DictionaryValue* request) {
  DCHECK(request);

  if (wifi_data.access_point_data.empty())
    return;

  typedef std::multiset<const AccessPointData*, AccessPointLess> AccessPointSet;
  AccessPointSet access_points_by_signal_strength;

  for (const auto& ap_data : wifi_data.access_point_data)
    access_points_by_signal_strength.insert(&ap_data);

  base::ListValue* wifi_access_point_list = new base::ListValue();
  for (const auto& ap_data : access_points_by_signal_strength) {
    base::DictionaryValue* wifi_dict = new base::DictionaryValue();
    AddString("macAddress", base::UTF16ToUTF8(ap_data->mac_address), wifi_dict);
    AddInteger("signalStrength", ap_data->radio_signal_strength, wifi_dict);
    AddInteger("age", age_milliseconds, wifi_dict);
    AddInteger("channel", ap_data->channel, wifi_dict);
    AddInteger("signalToNoiseRatio", ap_data->signal_to_noise, wifi_dict);
    wifi_access_point_list->Append(wifi_dict);
  }
  request->Set("wifiAccessPoints", wifi_access_point_list);
}

void FormatPositionError(const GURL& server_url,
                         const std::string& message,
                         Geoposition* position) {
    position->error_code = Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
    position->error_message = "Network location provider at '";
    position->error_message += server_url.GetOrigin().spec();
    position->error_message += "' : ";
    position->error_message += message;
    position->error_message += ".";
    VLOG(1) << "NetworkLocationRequest::GetLocationFromResponse() : "
            << position->error_message;
}

void GetLocationFromResponse(bool http_post_result,
                             int status_code,
                             const std::string& response_body,
                             const base::Time& wifi_timestamp,
                             const GURL& server_url,
                             Geoposition* position,
                             base::string16* access_token) {
  DCHECK(position);
  DCHECK(access_token);

  // HttpPost can fail for a number of reasons. Most likely this is because
  // we're offline, or there was no response.
  if (!http_post_result) {
    FormatPositionError(server_url, "No response received", position);
    RecordUmaEvent(NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_EMPTY);
    return;
  }
  if (status_code != 200) {  // HTTP OK.
    std::string message = "Returned error code ";
    message += base::IntToString(status_code);
    FormatPositionError(server_url, message, position);
    RecordUmaEvent(NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_NOT_OK);
    return;
  }
  // We use the timestamp from the wifi data that was used to generate
  // this position fix.
  if (!ParseServerResponse(response_body, wifi_timestamp, position,
                           access_token)) {
    // We failed to parse the repsonse.
    FormatPositionError(server_url, "Response was malformed", position);
    RecordUmaEvent(NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_MALFORMED);
    return;
  }
  // The response was successfully parsed, but it may not be a valid
  // position fix.
  if (!position->Validate()) {
    FormatPositionError(server_url,
                        "Did not provide a good position fix", position);
    RecordUmaEvent(NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_INVALID_FIX);
    return;
  }
  RecordUmaEvent(NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_SUCCESS);
}

// Numeric values without a decimal point have type integer and IsDouble() will
// return false. This is convenience function for detecting integer or floating
// point numeric values. Note that isIntegral() includes boolean values, which
// is not what we want.
bool GetAsDouble(const base::DictionaryValue& object,
                 const std::string& property_name,
                 double* out) {
  DCHECK(out);
  const base::Value* value = NULL;
  if (!object.Get(property_name, &value))
    return false;
  int value_as_int;
  DCHECK(value);
  if (value->GetAsInteger(&value_as_int)) {
    *out = value_as_int;
    return true;
  }
  return value->GetAsDouble(out);
}

bool ParseServerResponse(const std::string& response_body,
                         const base::Time& wifi_timestamp,
                         Geoposition* position,
                         base::string16* access_token) {
  DCHECK(position);
  DCHECK(!position->Validate());
  DCHECK(position->error_code == Geoposition::ERROR_CODE_NONE);
  DCHECK(access_token);
  DCHECK(!wifi_timestamp.is_null());

  if (response_body.empty()) {
    LOG(WARNING) << "ParseServerResponse() : Response was empty.";
    return false;
  }
  DVLOG(1) << "ParseServerResponse() : Parsing response " << response_body;

  // Parse the response, ignoring comments.
  std::string error_msg;
  std::unique_ptr<base::Value> response_value =
      base::JSONReader::ReadAndReturnError(response_body, base::JSON_PARSE_RFC,
                                           NULL, &error_msg);
  if (response_value == NULL) {
    LOG(WARNING) << "ParseServerResponse() : JSONReader failed : "
                 << error_msg;
    return false;
  }

  if (!response_value->IsType(base::Value::TYPE_DICTIONARY)) {
    VLOG(1) << "ParseServerResponse() : Unexpected response type "
            << response_value->GetType();
    return false;
  }
  const base::DictionaryValue* response_object =
      static_cast<base::DictionaryValue*>(response_value.get());

  // Get the access token, if any.
  response_object->GetString(kAccessTokenString, access_token);

  // Get the location
  const base::Value* location_value = NULL;
  if (!response_object->Get(kLocationString, &location_value)) {
    VLOG(1) << "ParseServerResponse() : Missing location attribute.";
    // GLS returns a response with no location property to represent
    // no fix available; return true to indicate successful parse.
    return true;
  }
  DCHECK(location_value);

  if (!location_value->IsType(base::Value::TYPE_DICTIONARY)) {
    if (!location_value->IsType(base::Value::TYPE_NULL)) {
      VLOG(1) << "ParseServerResponse() : Unexpected location type "
              << location_value->GetType();
      // If the network provider was unable to provide a position fix, it should
      // return a HTTP 200, with "location" : null. Otherwise it's an error.
      return false;
    }
    return true;  // Successfully parsed response containing no fix.
  }
  const base::DictionaryValue* location_object =
      static_cast<const base::DictionaryValue*>(location_value);

  // latitude and longitude fields are always required.
  double latitude = 0;
  double longitude = 0;
  if (!GetAsDouble(*location_object, kLatitudeString, &latitude) ||
      !GetAsDouble(*location_object, kLongitudeString, &longitude)) {
    VLOG(1) << "ParseServerResponse() : location lacks lat and/or long.";
    return false;
  }
  // All error paths covered: now start actually modifying postion.
  position->latitude = latitude;
  position->longitude = longitude;
  position->timestamp = wifi_timestamp;

  // Other fields are optional.
  GetAsDouble(*response_object, kAccuracyString, &position->accuracy);

  return true;
}

}  // namespace

}  // namespace content
