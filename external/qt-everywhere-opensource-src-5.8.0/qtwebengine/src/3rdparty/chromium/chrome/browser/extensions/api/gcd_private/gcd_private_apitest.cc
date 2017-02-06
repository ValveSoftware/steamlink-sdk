// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/command_line.h"
#include "chrome/browser/extensions/api/gcd_private/gcd_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/local_discovery/test_service_discovery_client.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/common/switches.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/test_url_fetcher_factory.h"

namespace api = extensions::api;

namespace {

const char kPrivetInfoResponse[] =
    "{\"version\":\"3.0\","
    "\"endpoints\":{\"httpsPort\": 443},"
    "\"authentication\":{"
    "  \"mode\":[\"anonymous\",\"pairing\",\"cloud\"],"
    "  \"pairing\":[\"embeddedCode\"],"
    "  \"crypto\":[\"p224_spake2\"]"
    "}}";

const uint8_t kAnnouncePacket[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x80, 0x00,  // Standard query response, no error
    0x00, 0x00,  // No questions (for simplicity)
    0x00, 0x05,  // 5 RR (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs
    0x07, '_',  'p',  'r',  'i',  'v',  'e',  't',  0x04, '_',
    't',  'c',  'p',  0x05, 'l',  'o',  'c',  'a',  'l',  0x00,
    0x00, 0x0c,              // TYPE is PTR.
    0x00, 0x01,              // CLASS is IN.
    0x00, 0x00,              // TTL (4 bytes) is 32768 second.
    0x10, 0x00, 0x00, 0x0c,  // RDLENGTH is 12 bytes.
    0x09, 'm',  'y',  'S',  'e',  'r',  'v',  'i',  'c',  'e',
    0xc0, 0x0c, 0x09, 'm',  'y',  'S',  'e',  'r',  'v',  'i',
    'c',  'e',  0xc0, 0x0c, 0x00, 0x10,  // TYPE is TXT.
    0x00, 0x01,                          // CLASS is IN.
    0x00, 0x00,                          // TTL (4 bytes) is 32768 seconds.
    0x01, 0x00, 0x00, 0x41,              // RDLENGTH is 69 bytes.
    0x03, 'i',  'd',  '=',  0x10, 't',  'y',  '=',  'S',  'a',
    'm',  'p',  'l',  'e',  ' ',  'd',  'e',  'v',  'i',  'c',
    'e',  0x1e, 'n',  'o',  't',  'e',  '=',  'S',  'a',  'm',
    'p',  'l',  'e',  ' ',  'd',  'e',  'v',  'i',  'c',  'e',
    ' ',  'd',  'e',  's',  'c',  'r',  'i',  'p',  't',  'i',
    'o',  'n',  0x0c, 't',  'y',  'p',  'e',  '=',  'p',  'r',
    'i',  'n',  't',  'e',  'r',  0x09, 'm',  'y',  'S',  'e',
    'r',  'v',  'i',  'c',  'e',  0xc0, 0x0c, 0x00, 0x21,  // Type is SRV
    0x00, 0x01,                                            // CLASS is IN
    0x00, 0x00,                          // TTL (4 bytes) is 32768 second.
    0x10, 0x00, 0x00, 0x17,              // RDLENGTH is 23
    0x00, 0x00, 0x00, 0x00, 0x22, 0xb8,  // port 8888
    0x09, 'm',  'y',  'S',  'e',  'r',  'v',  'i',  'c',  'e',
    0x05, 'l',  'o',  'c',  'a',  'l',  0x00, 0x09, 'm',  'y',
    'S',  'e',  'r',  'v',  'i',  'c',  'e',  0x05, 'l',  'o',
    'c',  'a',  'l',  0x00, 0x00, 0x01,  // Type is A
    0x00, 0x01,                          // CLASS is IN
    0x00, 0x00,                          // TTL (4 bytes) is 32768 second.
    0x10, 0x00, 0x00, 0x04,              // RDLENGTH is 4
    0x01, 0x02, 0x03, 0x04,              // 1.2.3.4
    0x09, 'm',  'y',  'S',  'e',  'r',  'v',  'i',  'c',  'e',
    0x05, 'l',  'o',  'c',  'a',  'l',  0x00, 0x00, 0x1C,  // Type is AAAA
    0x00, 0x01,                                            // CLASS is IN
    0x00, 0x00,              // TTL (4 bytes) is 32768 second.
    0x10, 0x00, 0x00, 0x10,  // RDLENGTH is 16
    0x01, 0x02, 0x03, 0x04,  // 1.2.3.4
    0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02,
    0x03, 0x04,
};

class GcdPrivateAPITest : public ExtensionApiTest {
 public:
  GcdPrivateAPITest()
      : url_fetcher_factory_(
            &url_fetcher_impl_factory_,
            base::Bind(&GcdPrivateAPITest::CreateFakeURLFetcher,
                       base::Unretained(this))) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        "ddchlicdkolnonkihahngkmmmjnjlkkf");
  }

  std::unique_ptr<net::FakeURLFetcher> CreateFakeURLFetcher(
      const GURL& url,
      net::URLFetcherDelegate* fetcher_delegate,
      const std::string& response_data,
      net::HttpStatusCode response_code,
      net::URLRequestStatus::Status status) {
    std::unique_ptr<net::FakeURLFetcher> fetcher(new net::FakeURLFetcher(
        url, fetcher_delegate, response_data, response_code, status));
    scoped_refptr<net::HttpResponseHeaders> headers =
        new net::HttpResponseHeaders("");
    headers->AddHeader("Content-Type: application/json");
    fetcher->set_response_headers(headers);
    return fetcher;
  }

 protected:
  net::URLFetcherImplFactory url_fetcher_impl_factory_;
  net::FakeURLFetcherFactory url_fetcher_factory_;
};

class GcdPrivateWithMdnsAPITest : public GcdPrivateAPITest {
 public:
  void SetUpOnMainThread() override {
    test_service_discovery_client_ =
        new local_discovery::TestServiceDiscoveryClient();
    test_service_discovery_client_->Start();
  }

  void TearDownOnMainThread() override {
    test_service_discovery_client_ = nullptr;
    ExtensionApiTest::TearDownOnMainThread();
  }

 protected:
  void SimulateReceiveWithDelay(const uint8_t* packet, int size) {
    if (ExtensionSubtestsAreSkipped())
      return;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(
            &local_discovery::TestServiceDiscoveryClient::SimulateReceive,
            test_service_discovery_client_, packet, size),
        base::TimeDelta::FromSeconds(1));
  }

  scoped_refptr<local_discovery::TestServiceDiscoveryClient>
      test_service_discovery_client_;
};

IN_PROC_BROWSER_TEST_F(GcdPrivateWithMdnsAPITest, DeviceInfo) {
  test_service_discovery_client_->SimulateReceive(kAnnouncePacket,
                                                  sizeof(kAnnouncePacket));
  url_fetcher_factory_.SetFakeResponse(GURL("http://1.2.3.4:8888/privet/info"),
                                       kPrivetInfoResponse,
                                       net::HTTP_OK,
                                       net::URLRequestStatus::SUCCESS);
  EXPECT_TRUE(RunExtensionSubtest("gcd_private/api", "device_info.html"));
}

IN_PROC_BROWSER_TEST_F(GcdPrivateWithMdnsAPITest, Session) {
  test_service_discovery_client_->SimulateReceive(kAnnouncePacket,
                                                  sizeof(kAnnouncePacket));
  url_fetcher_factory_.SetFakeResponse(GURL("http://1.2.3.4:8888/privet/info"),
                                       kPrivetInfoResponse,
                                       net::HTTP_OK,
                                       net::URLRequestStatus::SUCCESS);
  EXPECT_TRUE(RunExtensionSubtest("gcd_private/api", "session.html"));
}

}  // namespace
