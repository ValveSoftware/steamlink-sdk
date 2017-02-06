// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_tamper_detection.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/macros.h"
#include "base/md5.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/histogram_tester.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers_test_utils.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "net/android/network_library.h"
#endif

namespace {

// Calcuates MD5 hash value for a string and then base64 encode it. Testcases
// contain expected fingerprint in plain text, which needs to be encoded before
// comparison.
std::string GetEncoded(const std::string& input) {
  base::MD5Digest digest;
  base::MD5Sum(input.c_str(), input.size(), &digest);
  std::string base64encoded;
  base::Base64Encode(
      std::string(reinterpret_cast<char*>(digest.a), arraysize(digest.a)),
      &base64encoded);
  return base64encoded;
}

// Replaces all contents within "[]" by corresponding base64 encoded MD5 value.
// It can handle nested case like: [[abc]def]. This helper function transforms
// fingerprint in plain text to actual encoded fingerprint.
void ReplaceWithEncodedString(std::string* input) {
  size_t start, end, temp;
  while (true) {
    start = input->find("[");
    if (start == std::string::npos) break;
    while (true) {
      temp = input->find("[", start + 1);
      end = input->find("]", start + 1);
      if (end != std::string::npos && end < temp)
        break;

      start = temp;
    }
    std::string need_to_encode = input->substr(start + 1, end - start - 1);
    *input = input->substr(0, start) + GetEncoded(need_to_encode) +
             input->substr(end + 1);
  }
}

// Returns a vector contains all the values from a comma-separated string.
// Some testcases contain string representation of a vector, this helper
// function generates a vector from a input string.
std::vector<std::string> StringsToVector(const std::string& values) {
  std::vector<std::string> ret;
  if (values.empty())
    return ret;
  size_t now = 0;
  size_t next;
  while ((next = values.find(",", now)) != std::string::npos) {
    ret.push_back(values.substr(now, next - now));
    now = next + 1;
  }
  return ret;
}

void InitEnv() {
#if defined(OS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  static bool inited = false;
  if (!inited) {
    net::android::RegisterNetworkLibrary(env);
    inited = true;
  }
#endif
}

}  // namespace

namespace data_reduction_proxy {

using DataReductionProxyTamperDetectionTest = testing::Test;

// Tests function ValidateChromeProxyHeader.
TEST_F(DataReductionProxyTamperDetectionTest, ChromeProxy) {
  // |received_fingerprint| is not the actual fingerprint from Data Reduction
  // Proxy, instead, the base64 encoded field is in plain text (within "[]")
  // and needs to be encoded first.
  struct {
    std::string label;
    std::string raw_header;
    std::string received_fingerprint;
    bool expected_tampered_with;
  } test[] = {
    {
      "Checks sorting.",
      "HTTP/1.1 200 OK\n"
      "Chrome-Proxy: c,b,a,3,2,1,fcp=f\n",
      "[1,2,3,a,b,c,]",
      false,
    },
    {
      "Checks Chrome-Proxy's fingerprint removing.",
      "HTTP/1.1 200 OK\n"
      "Chrome-Proxy: a,b,c,d,e,3,2,1,fcp=f\n",
      "[1,2,3,a,b,c,d,e,]",
      false,
    },
    {
      "Checks no Chrome-Proxy header case (should not happen).",
      "HTTP/1.1 200 OK\n",
      "[]",
      false,
    },
    {
      "Checks empty Chrome-Proxy header case (should not happen).",
      "HTTP/1.1 200 OK\n"
      "Chrome-Proxy:    \n",
      "[,]",
      false,
    },
    {
      "Checks Chrome-Proxy header with its fingerprint only case.",
      "HTTP/1.1 200 OK\n"
      "Chrome-Proxy: fcp=f\n",
      "[]",
      false,
    },
    {
      "Checks empty Chrome-Proxy header case, with extra ',' and ' '",
      "HTTP/1.1 200 OK\n"
      "Chrome-Proxy: fcp=f  ,  \n",
      "[]",
      false,
    },
    {
      "Changed no value to empty value.",
      "HTTP/1.1 200 OK\n"
      "Chrome-Proxy: fcp=f\n",
      "[,]",
      true,
    },
    {
      "Changed header values.",
      "HTTP/1.1 200 OK\n"
      "Chrome-Proxy: a,b=2,c,d=1,fcp=f\n",
      "[a,b=3,c,d=1,]",
      true,
    },
    {
      "Changed order of header values.",
      "HTTP/1.1 200 OK\n"
      "Chrome-Proxy: c,b,a,fcp=1\n",
      "[c,b,a,]",
      true,
    },
    {
      "Checks Chrome-Proxy header with extra ' '.",
      "HTTP/1.1 200 OK\n"
      "Chrome-Proxy: a  , b   , c,   d,  fcp=f\n",
      "[a,b,c,d,]",
      false
    },
    {
      "Check Chrome-Proxy header with multiple lines and ' '.",
      "HTTP/1.1 200 OK\n"
      "Chrome-Proxy:     a    ,   c   , d, fcp=f    \n"
      "Chrome-Proxy:    b \n",
      "[a,b,c,d,]",
      false
    },
    {
      "Checks Chrome-Proxy header with multiple lines, at different positions",
      "HTTP/1.1 200 OK\n"
      "Chrome-Proxy:     a   \n"
      "Chrome-Proxy:    c \n"
      "Content-Type: 1\n"
      "Cache-Control: 2\n"
      "ETag: 3\n"
      "Chrome-Proxy:    b  \n"
      "Connection: 4\n"
      "Expires: 5\n"
      "Chrome-Proxy:    fcp=f \n"
      "Via: \n"
      "Content-Length: 12345\n",
      "[a,b,c,]",
      false
    },
    {
      "Checks Chrome-Proxy header with multiple same values.",
      "HTTP/1.1 200 OK\n"
      "Chrome-Proxy:     a   \n"
      "Chrome-Proxy:    b\n"
      "Chrome-Proxy:    c\n"
      "Chrome-Proxy:    d,   fcp=f    \n"
      "Chrome-Proxy:     a   \n",
      "[a,a,b,c,d,]",
      false
    },
    {
      "Changed Chrome-Proxy header with multiple lines..",
      "HTTP/1.1 200 OK\n"
      "Chrome-Proxy: a\n"
      "Chrome-Proxy: a\n"
      "Chrome-Proxy: b\n"
      "Chrome-Proxy: c,fcp=f\n",
      "[a,b,c,]",
      true,
    },
    {
      "Checks case whose received fingerprint is empty.",
      "HTTP/1.1 200 OK\n"
      "Chrome-Proxy: a,b,c,fcp=1\n",
      "[]",
      true,
    },
    {
      "Checks case whose received fingerprint cannot be base64 decoded.",
      "HTTP/1.1 200 OK\n"
      "Chrome-Proxy: a,b,c,fcp=1\n",
      "not_base64_encoded",
      true,
    },
  };

  for (size_t i = 0; i < arraysize(test); ++i) {
    ReplaceWithEncodedString(&test[i].received_fingerprint);

    std::string raw_headers(test[i].raw_header);
    HeadersToRaw(&raw_headers);
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders(raw_headers));

    DataReductionProxyTamperDetection tamper_detection(headers.get(), true, 0);

    bool tampered = tamper_detection.ValidateChromeProxyHeader(
        test[i].received_fingerprint);

    EXPECT_EQ(test[i].expected_tampered_with, tampered) << test[i].label;
  }
}

// Tests function ValidateViaHeader.
TEST_F(DataReductionProxyTamperDetectionTest, Via) {
  struct {
    std::string label;
    std::string raw_header;
    std::string received_fingerprint;
    bool expected_tampered_with;
    bool expected_has_chrome_proxy_via_header;
  } test[] = {
    {
      "Checks the case that Chrome-Compression-Proxy occurs at the last.",
      "HTTP/1.1 200 OK\n"
      "Via: a, b, c, 1.1 Chrome-Compression-Proxy\n",
      "",
      false,
      true,
    },
    {
      "Checks when there is intermediary.",
      "HTTP/1.1 200 OK\n"
      "Via: a, b,  c,   1.1 Chrome-Compression-Proxy,    xyz\n",
      "",
      true,
      true,
    },
    {
      "Checks the case of empty Via header.",
      "HTTP/1.1 200 OK\n"
      "Via:  \n",
      "",
      true,
      false,
    },
    {
      "Checks the case that only the Data Reduction Proxy's Via header occurs.",
      "HTTP/1.1 200 OK\n"
      "Via:  1.1 Chrome-Compression-Proxy    \n",
      "",
      false,
      true,
    },
    {
      "Checks the case that there are ' ', i.e., empty value after the Data"
      " Reduction Proxy's Via header.",
      "HTTP/1.1 200 OK\n"
      "Via:  1.1 Chrome-Compression-Proxy  ,  , \n",
      "",
      false,
      true,
    },
    {
      "Checks the case when there is no Via header",
      "HTTP/1.1 200 OK\n",
      "",
      true,
      false,
    },
  };

  for (size_t i = 0; i < arraysize(test); ++i) {
    std::string raw_headers(test[i].raw_header);
    HeadersToRaw(&raw_headers);
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders(raw_headers));

    DataReductionProxyTamperDetection tamper_detection(headers.get(), true, 0);

    bool has_chrome_proxy_via_header;
    bool tampered = tamper_detection.ValidateViaHeader(
        test[i].received_fingerprint, &has_chrome_proxy_via_header);

    EXPECT_EQ(test[i].expected_tampered_with, tampered) << test[i].label;
    EXPECT_EQ(test[i].expected_has_chrome_proxy_via_header,
              has_chrome_proxy_via_header) << test[i].label;
  }
}

// Tests function ValidateOtherHeaders.
TEST_F(DataReductionProxyTamperDetectionTest, OtherHeaders) {
  // For following testcases, |received_fingerprint| is not the actual
  // fingerprint from Data Reduction Proxy, instead, the base64 encoded field
  // is in plain text (within "[]") and needs to be encoded first. For example,
  // "[12345;]|content-length" needs to be encoded to
  // "Base64Encoded(MD5(12345;))|content-length" before calling the checking
  // function.
  struct {
    std::string label;
    std::string raw_header;
    std::string received_fingerprint;
    bool expected_tampered_with;
  } test[] = {
    {
      "Checks the case that only one header is requested.",
      "HTTP/1.1 200 OK\n"
      "Content-Length: 12345\n",
      "[12345,;]|content-length",
      false
    },
    {
      "Checks the case that there is only one requested header and it does not"
      "exist.",
      "HTTP/1.1 200 OK\n",
      "[;]|non_exist_header",
      false
    },
    {
      "Checks the case of multiple headers are requested.",
      "HTTP/1.1 200 OK\n"
      "Content-Type: 1\n"
      "Cache-Control: 2\n"
      "ETag: 3\n"
      "Connection: 4\n"
      "Expires: 5\n",
      "[1,;2,;3,;4,;5,;]|content-type|cache-control|etag|connection|expires",
      false
    },
    {
      "Checks the case that one header has multiple values.",
      "HTTP/1.1 200 OK\n"
      "Content-Type: aaa1,    bbb1, ccc1\n"
      "Cache-Control: aaa2\n",
      "[aaa1,bbb1,ccc1,;aaa2,;]|content-type|cache-control",
      false
    },
    {
      "Checks the case that one header has multiple lines.",
      "HTTP/1.1 200 OK\n"
      "Content-Type: aaa1,   ccc1\n"
      "Content-Type: xxx1,    bbb1, ccc1\n"
      "Cache-Control: aaa2\n",
      "[aaa1,bbb1,ccc1,ccc1,xxx1,;aaa2,;]|content-type|cache-control",
      false
    },
    {
      "Checks the case that more than one headers have multiple values.",
      "HTTP/1.1 200 OK\n"
      "Content-Type: aaa1,   ccc1\n"
      "Cache-Control: ccc2    , bbb2\n"
      "Content-Type:  bbb1, ccc1\n"
      "Cache-Control: aaa2   \n",
      "[aaa1,bbb1,ccc1,ccc1,;aaa2,bbb2,ccc2,;]|content-type|cache-control",
      false
    },
    {
      "Checks the case that one of the requested headers is missing (Expires).",
      "HTTP/1.1 200 OK\n"
      "Content-Type: aaa1,   ccc1\n",
      "[aaa1,ccc1,;;]|content-type|expires",
      false
    },
    {
      "Checks the case that some of the requested headers have empty value.",
      "HTTP/1.1 200 OK\n"
      "Content-Type:   \n"
      "Cache-Control: \n",
      "[,;,;]|content-type|cache-control",
      false
    },
    {
      "Checks the case that all the requested headers are missing.",
      "HTTP/1.1 200 OK\n",
      "[;;]|content-type|expires",
      false
    },
    {
      "Checks the case that some headers are missing, some of them are empty.",
      "HTTP/1.1 200 OK\n"
      "Cache-Control: \n",
      "[;,;]|content-type|cache-control",
      false
    },
    {
      "Checks the case there is no requested header (header list is empty).",
      "HTTP/1.1 200 OK\n"
      "Chrome-Proxy: aut=aauutthh,bbbypas=0,aaxxx=xxx,bbbloc=1\n"
      "Content-Type: 1\n"
      "Cache-Control: 2\n",
      "[]",
      false
    },
    {
      "Checks tampered requested header values.",
      "HTTP/1.1 200 OK\n"
      "Content-Type: aaa1,   ccc1\n"
      "Cache-Control: ccc2    , bbb2\n",
      "[aaa1,bbb1,;bbb2,ccc2,;]|content-type|cache-control",
      true
    },
  };

  for (size_t i = 0; i < arraysize(test); ++i) {
    ReplaceWithEncodedString(&test[i].received_fingerprint);

    std::string raw_headers(test[i].raw_header);
    HeadersToRaw(&raw_headers);
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders(raw_headers));

    DataReductionProxyTamperDetection tamper_detection(headers.get(), true, 0);

    bool tampered = tamper_detection.ValidateOtherHeaders(
        test[i].received_fingerprint);

    EXPECT_EQ(test[i].expected_tampered_with, tampered) << test[i].label;
  }
}

// Tests function ValidateContentLength.
TEST_F(DataReductionProxyTamperDetectionTest, ContentLength) {
  struct {
    std::string label;
    std::string received_fingerprint;
    int64_t actual_content_length;
    int64_t expected_original_content_length;
    bool expected_tampered_with;
  } test[] = {
    {
      "Checks the case fingerprint matches actual content length.",
      "12345",
      12345,
      12345,
      false,
    },
    {
      "Checks case that response got modified.",
      "12345",
      12346,
      12345,
      true,
    },
  };

  for (size_t i = 0; i < arraysize(test); ++i) {
    std::string raw_headers("HTTP/1.1 200 OK\n");
    HeadersToRaw(&raw_headers);
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders(raw_headers));

    DataReductionProxyTamperDetection tamper_detection(headers.get(), true, 0);

    int64_t original_content_length;
    bool tampered = tamper_detection.ValidateContentLength(
        test[i].received_fingerprint,
        test[i].actual_content_length,
        &original_content_length);

    EXPECT_EQ(test[i].expected_tampered_with, tampered) << test[i].label;
    EXPECT_EQ(test[i].expected_original_content_length,
              original_content_length) << test[i].label;
  }
}

// Tests ValuesToSortedString function.
TEST_F(DataReductionProxyTamperDetectionTest, ValuesToSortedString) {
  struct {
    std::string label;
    std::string input_values;
    std::string expected_output_string;
  } test[] = {
    {
      "Checks the correctness of sorting.",
      "3,2,1,",
      "1,2,3,",
    },
    {
      "Checks the case that there is an empty input vector.",
      "",
      "",
    },
    {
      "Checks the case that there is an empty string in the input vector.",
      ",",
      ",",
    },
  };

  for (size_t i = 0; i < arraysize(test); ++i) {
    std::vector<std::string> input_values =
        StringsToVector(test[i].input_values);
    std::string output_string =
        DataReductionProxyTamperDetection::ValuesToSortedString(&input_values);
    EXPECT_EQ(output_string, test[i].expected_output_string) << test[i].label;
  }
}

// Tests GetHeaderValues function.
TEST_F(DataReductionProxyTamperDetectionTest, GetHeaderValues) {
  struct {
    std::string label;
    std::string raw_header;
    std::string header_name;
    std::string expected_output_values;
  } test[] = {
    {
      "Checks the correctness of getting single line header.",
      "HTTP/1.1 200 OK\n"
      "test: 1, 2, 3\n",
      "test",
      "1,2,3,",
    },
    {
      "Checks the correctness of getting multiple lines header.",
      "HTTP/1.1 200 OK\n"
      "test: 1, 2, 3\n"
      "test: 4, 5, 6\n"
      "test: 7, 8, 9\n",
      "test",
      "1,2,3,4,5,6,7,8,9,",
    },
    {
      "Checks the correctness of getting missing header.",
      "HTTP/1.1 200 OK\n",
      "test",
      "",
    },
    {
      "Checks the correctness of getting empty header.",
      "HTTP/1.1 200 OK\n"
      "test:   \n",
      "test",
      ",",
    },
  };

  for (size_t i = 0; i < arraysize(test); ++i) {
    std::string raw_headers(test[i].raw_header);
    HeadersToRaw(&raw_headers);
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders(raw_headers));

    std::vector<std::string> expected_output_values =
        StringsToVector(test[i].expected_output_values);

    std::vector<std::string> output_values =
        DataReductionProxyTamperDetection::GetHeaderValues(headers.get(),
                                                           test[i].header_name);
    EXPECT_EQ(expected_output_values, output_values) << test[i].label;
  }
}

// Tests UMA histogram count.
TEST_F(DataReductionProxyTamperDetectionTest, HistogramCount) {
  struct {
    std::string raw_header;
    std::string histogram_name_suffix;
    int original_content_length;
    std::string image_histogram_name_suffix;
  } tests[] = {
      // Checks the correctness of histogram for Javascript
      {"HTTP/1.1 200 OK\n"
       "Content-Type: text/javascript\n",
       "_JS", -1, ""},
      // Checks the correctness of histogram for CSS
      {"HTTP/1.1 200 OK\n"
       "Content-Type: text/css\n",
       "_CSS", -1, ""},
      // Checks the correctness of histogram for image
      {"HTTP/1.1 200 OK\n"
       "Content-Type: image/test\n",
       "_Image", 1, "_Image_0_10KB"},
      // Checks the correctness of histogram for GIF
      {"HTTP/1.1 200 OK\n"
       "Content-Type: image/gif\n",
       "_Image_GIF", 20 * 1024, "_Image_10_100KB"},
      // Checks the correctness of histogram for JPG
      {"HTTP/1.1 200 OK\n"
       "Content-Type: image/jpeg\n",
       "_Image_JPG", 200 * 1024, "_Image_100_500KB"},
      // Checks the correctness of histogram for PNG
      {"HTTP/1.1 200 OK\n"
       "Content-Type: image/png\n",
       "_Image_PNG", 600 * 1024, "_Image_500KB"},
      // Checks the correctness of histogram for WebP
      {"HTTP/1.1 200 OK\n"
       "Content-Type: image/webp\n",
       "_Image_WEBP", -1, ""},
      // Checks the correctness of histogram for Video
      {"HTTP/1.1 200 OK\n"
       "Content-Type: video/webm\n",
       "_Video", -1, ""},
  };

  const int carrier_id = 100;

  for (auto& test : tests) {
    std::string raw_headers(test.raw_header);
    HeadersToRaw(&raw_headers);
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders(raw_headers));

    // Test HTTPS and HTTP separately.
    int https_values[] = {true, false};
    for (auto https : https_values) {
      base::HistogramTester histogram_tester;

      DataReductionProxyTamperDetection tamper_detection(headers.get(), https,
                                                         carrier_id);
      tamper_detection.ReportUMAForTamperDetectionCount(
          test.original_content_length);
      histogram_tester.ExpectTotalCount(
          base::StringPrintf("DataReductionProxy.HeaderTamperDetectionHTTP%s"
                             "%s_Total",
                             (https ? "S" : ""),
                             test.histogram_name_suffix.c_str()),
          1);
      histogram_tester.ExpectUniqueSample(
          base::StringPrintf("DataReductionProxy.HeaderTamperDetectionHTTP%s"
                             "%s",
                             (https ? "S" : ""),
                             test.histogram_name_suffix.c_str()),
          carrier_id, 1);
      histogram_tester.ExpectTotalCount(
          base::StringPrintf("DataReductionProxy.HeaderTamperDetectionHTTP%s"
                             "_Total",
                             (https ? "S" : "")),
          1);
      histogram_tester.ExpectUniqueSample(
          base::StringPrintf("DataReductionProxy.HeaderTamperDetectionHTTP%s",
                             (https ? "S" : "")),
          carrier_id, 1);

      if (test.original_content_length != -1) {
        histogram_tester.ExpectTotalCount(
            base::StringPrintf("DataReductionProxy.HeaderTamperDetectionHTTP%s"
                               "%s_Total",
                               (https ? "S" : ""),
                               test.image_histogram_name_suffix.c_str()),
            1);
        histogram_tester.ExpectUniqueSample(
            base::StringPrintf("DataReductionProxy.HeaderTamperDetectionHTTP%s"
                               "%s",
                               (https ? "S" : ""),
                               test.image_histogram_name_suffix.c_str()),
            carrier_id, 1);
      }
    }
  }
}

// Tests function ReportUMAForContentLength, with the focus on compression
// ratio.
TEST_F(DataReductionProxyTamperDetectionTest, CompressionRatio) {
  struct {
    std::string raw_header;
    std::string histogram_name_suffix;
    int original_content_length;
    int content_length;
    int compression_ratio;
  } tests[] = {
      // Checks the correctness of histogram for Video
      {"HTTP/1.1 200 OK\n"
       "Content-Type: video/webm\n",
       "_Video", 1000, 800, 80},
      // Checks the correctness of histogram for JPEG
      {"HTTP/1.1 200 OK\n"
       "Content-Type: image/jpg\n",
       "_Image_JPG", 1000, 1, 0},
      // Checks the correctness of histogram for PNG
      {"HTTP/1.1 200 OK\n"
       "Content-Type: image/png\n",
       "_Image_PNG", 1000, 0, 0},
      // Checks the correctness of histogram for WebP
      {"HTTP/1.1 200 OK\n"
       "Content-Type: image/webp\n",
       "_Image_WEBP", 1000, 5000, 500},
  };

  const int carrier_id = 100;

  for (auto& test : tests) {
    std::string raw_headers(test.raw_header);
    HeadersToRaw(&raw_headers);
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders(raw_headers));

    // Test HTTPS and HTTP separately.
    int https_values[] = {true, false};
    for (auto https : https_values) {
      base::HistogramTester histogram_tester;

      DataReductionProxyTamperDetection tamper_detection(headers.get(), https,
                                                         carrier_id);
      tamper_detection.ReportUMAForContentLength(test.content_length,
                                                 test.original_content_length);
      histogram_tester.ExpectTotalCount(
          base::StringPrintf("DataReductionProxy.HeaderTamperedHTTP%s"
                             "_ContentLength%s_Total",
                             (https ? "S" : ""),
                             test.histogram_name_suffix.c_str()),
          1);
      histogram_tester.ExpectUniqueSample(
          base::StringPrintf("DataReductionProxy.HeaderTamperedHTTP%s"
                             "_ContentLength%s",
                             (https ? "S" : ""),
                             test.histogram_name_suffix.c_str()),
          carrier_id, 1);
      histogram_tester.ExpectUniqueSample(
          base::StringPrintf("DataReductionProxy.HeaderTamperedHTTP%s"
                             "_CompressionRatio%s",
                             (https ? "S" : ""),
                             test.histogram_name_suffix.c_str()),
          test.compression_ratio, 1);
    }
  }
}

// Tests main function DetectAndReport.
TEST_F(DataReductionProxyTamperDetectionTest, DetectAndReport) {
  struct {
    std::string label;
    std::string raw_header;
    int64_t content_length;
    bool expected_tampered_with;
  } test[] = {
    {
      "Check no fingerprint added case.",
      "HTTP/1.1 200 OK\n"
      "Via: a1, b2, 1.1 Chrome-Compression-Proxy\n"
      "Content-Length: 12345\n"
      "Chrome-Proxy: bypass=0\n",
      12345,
      false,
    },
    {
      "Check the case Chrome-Proxy fingerprint doesn't match.",
      "HTTP/1.1 200 OK\n"
      "Via: a1, b2, 1.1 Chrome-Compression-Proxy\n"
      "Content-Length: 12345\n"
      "header1: header_1\n"
      "header2: header_2\n"
      "header3: header_3\n"
      "Chrome-Proxy: fcl=12345, "
      "foh=[header_1,;header_2,;header_3,;]|header1|header2|header3,fvia=0,"
      "fcp=abc\n",
      12345,
      true,
    },
    {
      "Check the case response matches the fingerprint completely.",
      "HTTP/1.1 200 OK\n"
      "Via: a1, b2, 1.1 Chrome-Compression-Proxy\n"
      "Content-Length: 12345\n"
      "header1: header_1\n"
      "header2: header_2\n"
      "header3: header_3\n"
      "Chrome-Proxy: fcl=12345, "
      "foh=[header_1,;header_2,;header_3,;]|header1|header2|header3,"
      "fvia=0, fcp=[fcl=12345,foh=[header_1,;header_2,;header_3,;]"
      "|header1|header2|header3,fvia=0,]\n",
      12345,
      false,
    },
    {
      "Check the case that actual content length doesn't match.",
      "HTTP/1.1 200 OK\n"
      "Via: a1, b2, 1.1 Chrome-Compression-Proxy\n"
      "Content-Length: 0\n"
      "header1: header_1\n"
      "header2: header_2\n"
      "header3: header_3\n"
      "Chrome-Proxy: fcl=12345, "
      "foh=[header_1,;header_2,;header_3,;]|header1|header2|header3, fvia=0, "
      "fcp=[fcl=12345,foh=[header_1,;header_2,;header_3,;]|"
      "header1|header2|header3,fvia=0,]\n",
      123,
      true,
    },
  };

  InitEnv();

  for (size_t i = 0; i < arraysize(test); ++i) {
    std::string raw_headers(test[i].raw_header);
    ReplaceWithEncodedString(&raw_headers);
    HeadersToRaw(&raw_headers);
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders(raw_headers));

    EXPECT_EQ(
        test[i].expected_tampered_with,
        DataReductionProxyTamperDetection::DetectAndReport(
        headers.get(), true, test[i].content_length)) << test[i].label;
  }
}

}  // namespace data_reduction_proxy
