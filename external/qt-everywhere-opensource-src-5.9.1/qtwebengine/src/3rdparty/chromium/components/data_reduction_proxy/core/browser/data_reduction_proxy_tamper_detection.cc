// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_tamper_detection.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/base64.h"
#include "base/macros.h"
#include "base/md5.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "net/base/mime_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

#if defined(OS_ANDROID)
#include "net/android/network_library.h"
#endif

// Macro for UMA reporting of tamper detection. HTTP response first reports to
// histogram events |http_histogram| by |carrier_id|; then reports the total
// counts to |http_histogram|_Total. HTTPS response reports to histograms
// |https_histogram| and |https_histogram|_Total similarly.
#define REPORT_TAMPER_DETECTION_UMA( \
    scheme_is_https, https_histogram, http_histogram, carrier_id) \
  do { \
    if (scheme_is_https) { \
      UMA_HISTOGRAM_SPARSE_SLOWLY(https_histogram, carrier_id); \
      UMA_HISTOGRAM_COUNTS(https_histogram "_Total", 1); \
    } else { \
      UMA_HISTOGRAM_SPARSE_SLOWLY(http_histogram, carrier_id); \
      UMA_HISTOGRAM_COUNTS(http_histogram "_Total", 1); \
    } \
  } while (0)

// Macro for UMA reporting of compression ratio. Reports |compression_ratio|
// to either "HTTPS" histogram or "HTTP" histogram, depending on the response
// type.
#define REPORT_TAMPER_DETECTION_UMA_COMPRESSION_RATIO( \
    scheme_is_https, histogram, compression_ratio) \
  do { \
    if (scheme_is_https) { \
      UMA_HISTOGRAM_SPARSE_SLOWLY( \
          "DataReductionProxy.HeaderTamperedHTTPS_CompressionRatio" \
          histogram, compression_ratio); \
    } else { \
      UMA_HISTOGRAM_SPARSE_SLOWLY( \
          "DataReductionProxy.HeaderTamperedHTTP_CompressionRatio" \
          histogram, compression_ratio); \
    } \
  } while (0)

// Macro for UMA reporting of tamper detection as well as compression ratio.
#define REPORT_TAMPER_DETECTION_UMA_AND_COMPRESSION_RATIO( \
    scheme_is_https, https_histogram, http_histogram, carrier_id, \
    histogram_compression_ratio, compression_ratio) \
  do { \
    if (scheme_is_https) { \
      UMA_HISTOGRAM_SPARSE_SLOWLY(https_histogram, carrier_id); \
      UMA_HISTOGRAM_COUNTS(https_histogram "_Total", 1); \
      UMA_HISTOGRAM_SPARSE_SLOWLY( \
          "DataReductionProxy.HeaderTamperedHTTPS_CompressionRatio" \
          histogram_compression_ratio, compression_ratio); \
    } else { \
      UMA_HISTOGRAM_SPARSE_SLOWLY(http_histogram, carrier_id); \
      UMA_HISTOGRAM_COUNTS(http_histogram "_Total", 1); \
      UMA_HISTOGRAM_SPARSE_SLOWLY( \
          "DataReductionProxy.HeaderTamperedHTTP_CompressionRatio" \
          histogram_compression_ratio, compression_ratio); \
    } \
  } while (0)

namespace data_reduction_proxy {

// static
bool DataReductionProxyTamperDetection::DetectAndReport(
    const net::HttpResponseHeaders* headers,
    bool scheme_is_https,
    int64_t content_length) {
  if (headers == nullptr) {
    return false;
  }

  // Abort tamper detection, if the fingerprint of the Chrome-Proxy header is
  // absent.
  std::string chrome_proxy_fingerprint;
  if (!GetDataReductionProxyActionFingerprintChromeProxy(
      headers, &chrome_proxy_fingerprint))
    return false;

  // Get carrier ID.
  unsigned carrier_id = 0;
#if defined(OS_ANDROID)
  base::StringToUint(net::android::GetTelephonyNetworkOperator(), &carrier_id);
#endif

  DataReductionProxyTamperDetection tamper_detection(
      headers, scheme_is_https, carrier_id);

  // Checks if the Chrome-Proxy header has been tampered with.
  if (tamper_detection.ValidateChromeProxyHeader(chrome_proxy_fingerprint)) {
    tamper_detection.ReportUMAForChromeProxyHeaderValidation();
    return true;
  }

  // Chrome-Proxy header has not been tampered with, and thus other
  // fingerprints are valid.
  bool tampered = false;
  int64_t original_content_length = -1;
  std::string fingerprint;

  if (GetDataReductionProxyActionFingerprintVia(headers, &fingerprint)) {
    bool has_chrome_proxy_via_header;
    if (tamper_detection.ValidateViaHeader(
        fingerprint, &has_chrome_proxy_via_header)) {
      tamper_detection.ReportUMAForViaHeaderValidation(
          has_chrome_proxy_via_header);
      tampered = true;
    }
  }

  if (GetDataReductionProxyActionFingerprintOtherHeaders(
      headers, &fingerprint)) {
    if (tamper_detection.ValidateOtherHeaders(fingerprint)) {
      tamper_detection.ReportUMAForOtherHeadersValidation();
      tampered = true;
    }
  }

  if (GetDataReductionProxyActionFingerprintContentLength(
      headers, &fingerprint)) {
    if (tamper_detection.ValidateContentLength(fingerprint,
                                               content_length,
                                               &original_content_length)) {
      tamper_detection.ReportUMAForContentLength(content_length,
                                                 original_content_length);
      tampered = true;
    }
  }

  if (!tampered) {
    REPORT_TAMPER_DETECTION_UMA(
        scheme_is_https,
        "DataReductionProxy.HeaderTamperDetectionPassHTTPS",
        "DataReductionProxy.HeaderTamperDetectionPassHTTP",
        carrier_id);
  }

  // Reports the number of responses that other fingerprints will be checked,
  // separated by MIME type.
  tamper_detection.ReportUMAForTamperDetectionCount(original_content_length);

  return tampered;
}

// Constructor initializes the map of fingerprint names to codes.
DataReductionProxyTamperDetection::DataReductionProxyTamperDetection(
    const net::HttpResponseHeaders* headers,
    const bool is_secure,
    const unsigned carrier_id)
    : response_headers_(headers),
      scheme_is_https_(is_secure),
      carrier_id_(carrier_id) {
  DCHECK(headers);
}

DataReductionProxyTamperDetection::~DataReductionProxyTamperDetection() {};

void DataReductionProxyTamperDetection::ReportUMAForTamperDetectionCount(
    int64_t original_content_length) const {
  REPORT_TAMPER_DETECTION_UMA(
      scheme_is_https_, "DataReductionProxy.HeaderTamperDetectionHTTPS",
      "DataReductionProxy.HeaderTamperDetectionHTTP", carrier_id_);

  std::string mime_type;
  response_headers_->GetMimeType(&mime_type);

  if (net::MatchesMimeType("text/javascript", mime_type) ||
      net::MatchesMimeType("application/x-javascript", mime_type) ||
      net::MatchesMimeType("application/javascript", mime_type)) {
    REPORT_TAMPER_DETECTION_UMA(
        scheme_is_https_, "DataReductionProxy.HeaderTamperDetectionHTTPS_JS",
        "DataReductionProxy.HeaderTamperDetectionHTTP_JS", carrier_id_);
  } else if (net::MatchesMimeType("text/css", mime_type)) {
    REPORT_TAMPER_DETECTION_UMA(
        scheme_is_https_, "DataReductionProxy.HeaderTamperDetectionHTTPS_CSS",
        "DataReductionProxy.HeaderTamperDetectionHTTP_CSS", carrier_id_);
  } else if (net::MatchesMimeType("image/*", mime_type)) {
    REPORT_TAMPER_DETECTION_UMA(
        scheme_is_https_, "DataReductionProxy.HeaderTamperDetectionHTTPS_Image",
        "DataReductionProxy.HeaderTamperDetectionHTTP_Image", carrier_id_);

    if (net::MatchesMimeType("image/gif", mime_type)) {
      REPORT_TAMPER_DETECTION_UMA(
          scheme_is_https_,
          "DataReductionProxy.HeaderTamperDetectionHTTPS_Image_GIF",
          "DataReductionProxy.HeaderTamperDetectionHTTP_Image_GIF",
          carrier_id_);
    } else if (net::MatchesMimeType("image/jpeg", mime_type) ||
               net::MatchesMimeType("image/jpg", mime_type)) {
      REPORT_TAMPER_DETECTION_UMA(
          scheme_is_https_,
          "DataReductionProxy.HeaderTamperDetectionHTTPS_Image_JPG",
          "DataReductionProxy.HeaderTamperDetectionHTTP_Image_JPG",
          carrier_id_);
    } else if (net::MatchesMimeType("image/png", mime_type)) {
      REPORT_TAMPER_DETECTION_UMA(
          scheme_is_https_,
          "DataReductionProxy.HeaderTamperDetectionHTTPS_Image_PNG",
          "DataReductionProxy.HeaderTamperDetectionHTTP_Image_PNG",
          carrier_id_);
    } else if (net::MatchesMimeType("image/webp", mime_type)) {
      REPORT_TAMPER_DETECTION_UMA(
          scheme_is_https_,
          "DataReductionProxy.HeaderTamperDetectionHTTPS_Image_WEBP",
          "DataReductionProxy.HeaderTamperDetectionHTTP_Image_WEBP",
          carrier_id_);
    }

    if (original_content_length == -1)
      return;

    if (original_content_length < 10 * 1024) {  // 0-10KB
      REPORT_TAMPER_DETECTION_UMA(
          scheme_is_https_,
          "DataReductionProxy.HeaderTamperDetectionHTTPS_Image_0_10KB",
          "DataReductionProxy.HeaderTamperDetectionHTTP_Image_0_10KB",
          carrier_id_);
    } else if (original_content_length < 100 * 1024) {  // 10-100KB
      REPORT_TAMPER_DETECTION_UMA(
          scheme_is_https_,
          "DataReductionProxy.HeaderTamperDetectionHTTPS_Image_10_100KB",
          "DataReductionProxy.HeaderTamperDetectionHTTP_Image_10_100KB",
          carrier_id_);
    } else if (original_content_length < 500 * 1024) {  // 100-500KB
      REPORT_TAMPER_DETECTION_UMA(
          scheme_is_https_,
          "DataReductionProxy.HeaderTamperDetectionHTTPS_Image_100_500KB",
          "DataReductionProxy.HeaderTamperDetectionHTTP_Image_100_500KB",
          carrier_id_);
    } else {  // >=500KB
      REPORT_TAMPER_DETECTION_UMA(
          scheme_is_https_,
          "DataReductionProxy.HeaderTamperDetectionHTTPS_Image_500KB",
          "DataReductionProxy.HeaderTamperDetectionHTTP_Image_500KB",
          carrier_id_);
    }
  } else if (net::MatchesMimeType("video/*", mime_type)) {
    REPORT_TAMPER_DETECTION_UMA(
        scheme_is_https_, "DataReductionProxy.HeaderTamperDetectionHTTPS_Video",
        "DataReductionProxy.HeaderTamperDetectionHTTP_Video", carrier_id_);
  }
}

// |fingerprint| is Base64 encoded. Decodes it first. Then calculates the
// fingerprint of received Chrome-Proxy header, and compares the two to see
// whether they are equal or not.
bool DataReductionProxyTamperDetection::ValidateChromeProxyHeader(
    base::StringPiece fingerprint) const {
  std::string received_fingerprint;
  if (!base::Base64Decode(fingerprint, &received_fingerprint))
    return true;

  // Gets the Chrome-Proxy header values with its fingerprint removed.
  std::vector<std::string> chrome_proxy_header_values;
  GetDataReductionProxyHeaderWithFingerprintRemoved(
      response_headers_, &chrome_proxy_header_values);

  // Calculates the MD5 hash value of Chrome-Proxy.
  std::string actual_fingerprint;
  GetMD5(ValuesToSortedString(&chrome_proxy_header_values),
         &actual_fingerprint);

  return received_fingerprint != actual_fingerprint;
}

void DataReductionProxyTamperDetection::
    ReportUMAForChromeProxyHeaderValidation() const {
  REPORT_TAMPER_DETECTION_UMA(
      scheme_is_https_,
      "DataReductionProxy.HeaderTamperedHTTPS_ChromeProxy",
      "DataReductionProxy.HeaderTamperedHTTP_ChromeProxy",
      carrier_id_);
}

// Checks whether there are other proxies/middleboxes' named after the Data
// Reduction Proxy's name in Via header. |has_chrome_proxy_via_header| marks
// that whether the Data Reduction Proxy's Via header occurs or not.
bool DataReductionProxyTamperDetection::ValidateViaHeader(
    base::StringPiece fingerprint,
    bool* has_chrome_proxy_via_header) const {
  bool has_intermediary;
  *has_chrome_proxy_via_header = HasDataReductionProxyViaHeader(
      response_headers_,
      &has_intermediary);

  if (*has_chrome_proxy_via_header)
    return !has_intermediary;
  return true;
}

void DataReductionProxyTamperDetection::ReportUMAForViaHeaderValidation(
    bool has_chrome_proxy) const {
  // The Via header of the Data Reduction Proxy is missing.
  if (!has_chrome_proxy) {
    REPORT_TAMPER_DETECTION_UMA(
        scheme_is_https_,
        "DataReductionProxy.HeaderTamperedHTTPS_Via_Missing",
        "DataReductionProxy.HeaderTamperedHTTP_Via_Missing",
        carrier_id_);
    return;
  }

  REPORT_TAMPER_DETECTION_UMA(
      scheme_is_https_,
      "DataReductionProxy.HeaderTamperedHTTPS_Via",
      "DataReductionProxy.HeaderTamperedHTTP_Via",
      carrier_id_);
}

// The Data Reduction Proxy constructs a canonical representation of values of
// a list of headers. The fingerprint is constructed as follows:
// 1) for each header, gets the string representation of its values (same as
//    ValuesToSortedString);
// 2) concatenates all header's string representations using ";" as a delimiter;
// 3) calculates the MD5 hash value of above concatenated string;
// 4) appends the header names to the fingerprint, with a delimiter "|".
// The constructed fingerprint looks like:
//     [hashed_fingerprint]|header_name1|header_namer2:...
//
// To check whether such a fingerprint matches the response that the Chromium
// client receives, the client firstly extracts the header names. For
// each header, gets its string representation (by ValuesToSortedString),
// concatenates them and calculates the MD5 hash value. Compares the hash
// value to the fingerprint received from the Data Reduction Proxy.
bool DataReductionProxyTamperDetection::ValidateOtherHeaders(
    const std::string& fingerprint) const {
  DCHECK(!fingerprint.empty());

  // According to RFC 2616, "|" is not a valid character in a header name; and
  // it is not a valid base64 encoding character, so there is no ambituity in
  //using it as a delimiter.
  net::HttpUtil::ValuesIterator it(
      fingerprint.begin(), fingerprint.end(), '|');

  // The first value is the base64 encoded fingerprint.
  std::string received_fingerprint;
  if (!it.GetNext() ||
      !base::Base64Decode(base::StringPiece(it.value_begin(), it.value_end()),
                          &received_fingerprint)) {
    NOTREACHED();
    return true;
  }

  std::string header_values;
  // The following values are the header names included in the fingerprint
  // calculation.
  while (it.GetNext()) {
    // Gets values of one header.
    std::vector<std::string> response_header_values = GetHeaderValues(
        response_headers_, base::StringPiece(it.value_begin(), it.value_end()));
    // Sorts the values and concatenate them, with delimiter ";". ";" can occur
    // in a header value and thus two different sets of header values could map
    // to the same string representation. This should be very rare.
    // TODO(xingx): find an unambiguous representation.
    header_values += ValuesToSortedString(&response_header_values)  + ";";
  }

  // Calculates the MD5 hash of the concatenated string.
  std::string actual_fingerprint;
  GetMD5(header_values, &actual_fingerprint);

  return received_fingerprint != actual_fingerprint;
}

void DataReductionProxyTamperDetection::
    ReportUMAForOtherHeadersValidation() const {
  REPORT_TAMPER_DETECTION_UMA(
      scheme_is_https_,
      "DataReductionProxy.HeaderTamperedHTTPS_OtherHeaders",
      "DataReductionProxy.HeaderTamperedHTTP_OtherHeaders",
      carrier_id_);
}

bool DataReductionProxyTamperDetection::ValidateContentLength(
    base::StringPiece fingerprint,
    int64_t content_length,
    int64_t* original_content_length) const {
  DCHECK(original_content_length);
  // Abort, if Content-Length value from the Data Reduction Proxy does not
  // exist or it cannot be converted to an integer.
  if (!base::StringToInt64(fingerprint, original_content_length))
    return false;

  return *original_content_length != content_length;
}

void DataReductionProxyTamperDetection::ReportUMAForContentLength(
    int64_t content_length,
    int64_t original_content_length) const {
  // Gets MIME type of the response and reports to UMA histograms separately.
  // Divides MIME types into 4 groups: JavaScript, CSS, Images, and others.
  REPORT_TAMPER_DETECTION_UMA(
      scheme_is_https_,
      "DataReductionProxy.HeaderTamperedHTTPS_ContentLength",
      "DataReductionProxy.HeaderTamperedHTTP_ContentLength",
      carrier_id_);

  // Gets MIME type.
  std::string mime_type;
  response_headers_->GetMimeType(&mime_type);

  // Gets the compression ratio as a percentage. The compression ratio is
  // defined as the received content length of the Chromium client, over the
  // sent content length of the Data Reduction Proxy.
  int compression_ratio = 0;
  if (original_content_length != 0) {
    compression_ratio = content_length * 100 / original_content_length;
  }

  if (net::MatchesMimeType("text/javascript", mime_type) ||
      net::MatchesMimeType("application/x-javascript", mime_type) ||
      net::MatchesMimeType("application/javascript", mime_type)) {
    REPORT_TAMPER_DETECTION_UMA(
        scheme_is_https_,
        "DataReductionProxy.HeaderTamperedHTTPS_ContentLength_JS",
        "DataReductionProxy.HeaderTamperedHTTP_ContentLength_JS",
        carrier_id_);
  } else if (net::MatchesMimeType("text/css", mime_type)) {
    REPORT_TAMPER_DETECTION_UMA(
        scheme_is_https_,
        "DataReductionProxy.HeaderTamperedHTTPS_ContentLength_CSS",
        "DataReductionProxy.HeaderTamperedHTTP_ContentLength_CSS",
        carrier_id_);
  } else if (net::MatchesMimeType("image/*", mime_type)) {
    REPORT_TAMPER_DETECTION_UMA(
        scheme_is_https_,
        "DataReductionProxy.HeaderTamperedHTTPS_ContentLength_Image",
        "DataReductionProxy.HeaderTamperedHTTP_ContentLength_Image",
        carrier_id_);

    if (net::MatchesMimeType("image/gif", mime_type)) {
      REPORT_TAMPER_DETECTION_UMA_AND_COMPRESSION_RATIO(
          scheme_is_https_,
          "DataReductionProxy.HeaderTamperedHTTPS_ContentLength_Image_GIF",
          "DataReductionProxy.HeaderTamperedHTTP_ContentLength_Image_GIF",
          carrier_id_,
          "_Image_GIF",
          compression_ratio);
    } else if (net::MatchesMimeType("image/jpeg", mime_type) ||
        net::MatchesMimeType("image/jpg", mime_type)) {
      REPORT_TAMPER_DETECTION_UMA_AND_COMPRESSION_RATIO(
          scheme_is_https_,
          "DataReductionProxy.HeaderTamperedHTTPS_ContentLength_Image_JPG",
          "DataReductionProxy.HeaderTamperedHTTP_ContentLength_Image_JPG",
          carrier_id_,
          "_Image_JPG",
          compression_ratio);
    } else if (net::MatchesMimeType("image/png", mime_type)) {
      REPORT_TAMPER_DETECTION_UMA_AND_COMPRESSION_RATIO(
          scheme_is_https_,
          "DataReductionProxy.HeaderTamperedHTTPS_ContentLength_Image_PNG",
          "DataReductionProxy.HeaderTamperedHTTP_ContentLength_Image_PNG",
          carrier_id_,
          "_Image_PNG",
          compression_ratio);
    } else if (net::MatchesMimeType("image/webp", mime_type)) {
      REPORT_TAMPER_DETECTION_UMA_AND_COMPRESSION_RATIO(
          scheme_is_https_,
          "DataReductionProxy.HeaderTamperedHTTPS_ContentLength_Image_WEBP",
          "DataReductionProxy.HeaderTamperedHTTP_ContentLength_Image_WEBP",
          carrier_id_,
          "_Image_WEBP",
          compression_ratio);
    }

    REPORT_TAMPER_DETECTION_UMA_COMPRESSION_RATIO(
        scheme_is_https_, "_Image", compression_ratio);

    if (original_content_length < 10*1024) { // 0-10KB
      REPORT_TAMPER_DETECTION_UMA_COMPRESSION_RATIO(
          scheme_is_https_, "_Image_0_10KB", compression_ratio);
    } else if (original_content_length < 100*1024) { // 10-100KB
      REPORT_TAMPER_DETECTION_UMA_COMPRESSION_RATIO(
          scheme_is_https_, "_Image_10_100KB", compression_ratio);
    } else if (original_content_length < 500*1024) { // 100-500KB
      REPORT_TAMPER_DETECTION_UMA_COMPRESSION_RATIO(
          scheme_is_https_, "_Image_100_500KB", compression_ratio);
    } else { // >=500KB
      REPORT_TAMPER_DETECTION_UMA_COMPRESSION_RATIO(
          scheme_is_https_, "_Image_500KB", compression_ratio);
    }
  } else if (net::MatchesMimeType("video/*", mime_type)) {
    REPORT_TAMPER_DETECTION_UMA_AND_COMPRESSION_RATIO(
        scheme_is_https_,
        "DataReductionProxy.HeaderTamperedHTTPS_ContentLength_Video",
        "DataReductionProxy.HeaderTamperedHTTP_ContentLength_Video",
        carrier_id_, "_Video", compression_ratio);
  } else {
    REPORT_TAMPER_DETECTION_UMA(
        scheme_is_https_,
        "DataReductionProxy.HeaderTamperedHTTPS_ContentLength_Other",
        "DataReductionProxy.HeaderTamperedHTTP_ContentLength_Other",
        carrier_id_);
  }
}

// We construct a canonical representation of the header so that reordered
// header values will produce the same fingerprint. The fingerprint is
// constructed as follows:
// 1) sort the values;
// 2) concatenate sorted values with a "," delimiter.
std::string DataReductionProxyTamperDetection::ValuesToSortedString(
    std::vector<std::string>* values) {
  DCHECK(values);
  if (!values) return "";

  std::sort(values->begin(), values->end());

  size_t expected_size = 0;
  for (const auto& value : *values)
    expected_size += value.size() + 1;

  std::string joined;
  joined.reserve(expected_size);
  // Join all the header values together, including a trailing ','.
  for (const auto& value : *values) {
    joined.append(value);
    joined.append(1, ',');
  }
  return joined;
}

void DataReductionProxyTamperDetection::GetMD5(base::StringPiece input,
                                               std::string* output) {
  base::MD5Digest digest;
  base::MD5Sum(input.data(), input.size(), &digest);
  *output = std::string(reinterpret_cast<char*>(digest.a), arraysize(digest.a));
}

std::vector<std::string> DataReductionProxyTamperDetection::GetHeaderValues(
    const net::HttpResponseHeaders* headers,
    base::StringPiece header_name) {
  std::vector<std::string> values;
  std::string value;
  size_t iter = 0;
  while (headers->EnumerateHeader(&iter, header_name, &value)) {
    values.push_back(std::move(value));
  }
  return values;
}

}  // namespace data_reduction_proxy
