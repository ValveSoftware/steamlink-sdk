// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/foreign_fetch_request_handler.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_trial_policy.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_response_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// This is a sample public key for testing the API. The corresponding private
// key (use this to generate new samples for this test file) is:
//
//  0x83, 0x67, 0xf4, 0xcd, 0x2a, 0x1f, 0x0e, 0x04, 0x0d, 0x43, 0x13,
//  0x4c, 0x67, 0xc4, 0xf4, 0x28, 0xc9, 0x90, 0x15, 0x02, 0xe2, 0xba,
//  0xfd, 0xbb, 0xfa, 0xbc, 0x92, 0x76, 0x8a, 0x2c, 0x4b, 0xc7, 0x75,
//  0x10, 0xac, 0xf9, 0x3a, 0x1c, 0xb8, 0xa9, 0x28, 0x70, 0xd2, 0x9a,
//  0xd0, 0x0b, 0x59, 0xe1, 0xac, 0x2b, 0xb7, 0xd5, 0xca, 0x1f, 0x64,
//  0x90, 0x08, 0x8e, 0xa8, 0xe0, 0x56, 0x3a, 0x04, 0xd0
const uint8_t kTestPublicKey[] = {
    0x75, 0x10, 0xac, 0xf9, 0x3a, 0x1c, 0xb8, 0xa9, 0x28, 0x70, 0xd2,
    0x9a, 0xd0, 0x0b, 0x59, 0xe1, 0xac, 0x2b, 0xb7, 0xd5, 0xca, 0x1f,
    0x64, 0x90, 0x08, 0x8e, 0xa8, 0xe0, 0x56, 0x3a, 0x04, 0xd0,
};

}  // namespace

class ForeignFetchRequestHandlerTest : public testing::Test {
 public:
  ForeignFetchRequestHandlerTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {
    SetContentClient(&test_content_client_);
  }
  ~ForeignFetchRequestHandlerTest() override {}

  void SetUp() override {
    const GURL kScope("https://valid.example.com/scope/");
    const GURL kResource1("https://valid.example.com/scope/sw.js");
    const int64_t kRegistrationId = 0;
    const int64_t kVersionId = 0;
    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));
    registration_ = new ServiceWorkerRegistration(kScope, kRegistrationId,
                                                  context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(registration_.get(), kResource1,
                                        kVersionId, context()->AsWeakPtr());
  }

  void TearDown() override {
    version_ = nullptr;
    registration_ = nullptr;
    helper_.reset();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  ServiceWorkerContextCore* context() const { return helper_->context(); }

  bool CheckOriginTrialToken(const ServiceWorkerVersion* const version) const {
    return ForeignFetchRequestHandler::CheckOriginTrialToken(version);
  }

  ServiceWorkerVersion* version() const { return version_.get(); }

  static std::unique_ptr<net::HttpResponseInfo> CreateTestHttpResponseInfo() {
    std::unique_ptr<net::HttpResponseInfo> http_info(
        base::MakeUnique<net::HttpResponseInfo>());
    http_info->ssl_info.cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    DCHECK(http_info->ssl_info.is_valid());
    http_info->ssl_info.security_bits = 0x100;
    // SSL3 TLS_DHE_RSA_WITH_AES_256_CBC_SHA
    http_info->ssl_info.connection_status = 0x300039;
    http_info->headers = make_scoped_refptr(new net::HttpResponseHeaders(""));
    return http_info;
  }

 private:
  class TestContentClient : public ContentClient {
   public:
    // ContentRendererClient methods
    OriginTrialPolicy* GetOriginTrialPolicy() override {
      return &origin_trial_policy_;
    }

   private:
    class TestOriginTrialPolicy : public OriginTrialPolicy {
     public:
      base::StringPiece GetPublicKey() const override {
        return base::StringPiece(reinterpret_cast<const char*>(kTestPublicKey),
                                 arraysize(kTestPublicKey));
      }
      bool IsFeatureDisabled(base::StringPiece feature) const override {
        return false;
      }
    };

    TestOriginTrialPolicy origin_trial_policy_;
  };

  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  TestContentClient test_content_client_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  TestBrowserThreadBundle browser_thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(ForeignFetchRequestHandlerTest);
};

TEST_F(ForeignFetchRequestHandlerTest, CheckOriginTrialToken_NoToken) {
  EXPECT_TRUE(CheckOriginTrialToken(version()));
  std::unique_ptr<net::HttpResponseInfo> http_info(
      CreateTestHttpResponseInfo());
  version()->SetMainScriptHttpResponseInfo(*http_info);
  EXPECT_FALSE(CheckOriginTrialToken(version()));
}

TEST_F(ForeignFetchRequestHandlerTest, CheckOriginTrialToken_ValidToken) {
  EXPECT_TRUE(CheckOriginTrialToken(version()));
  std::unique_ptr<net::HttpResponseInfo> http_info(
      CreateTestHttpResponseInfo());
  const std::string kOriginTrial("Origin-Trial: ");
  // Token for ForeignFetch which expires 2033-05-18.
  // generate_token.py valid.example.com ForeignFetch
  // --expire-timestamp=2000000000
  // TODO(horo): Generate this sample token during the build.
  const std::string kFeatureToken(
      "AsDmvl17hoVfq9G6OT0VEhX28Nnl0VnbGZJoG6XFzosIamNfxNJ28m40PRA3PtFv3BfOlRe1"
      "5bLrEZhtICdDnwwAAABceyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5leGFtcGxlLmNvbTo0"
      "NDMiLCAiZmVhdHVyZSI6ICJGb3JlaWduRmV0Y2giLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0"
      "=");
  http_info->headers->AddHeader(kOriginTrial + kFeatureToken);
  version()->SetMainScriptHttpResponseInfo(*http_info);
  EXPECT_TRUE(CheckOriginTrialToken(version()));
}

TEST_F(ForeignFetchRequestHandlerTest, CheckOriginTrialToken_InvalidToken) {
  EXPECT_TRUE(CheckOriginTrialToken(version()));
  std::unique_ptr<net::HttpResponseInfo> http_info(
      CreateTestHttpResponseInfo());
  const std::string kOriginTrial("Origin-Trial: ");
  // Token for FooBar which expires 2033-05-18.
  // generate_token.py valid.example.com FooBar
  // --expire-timestamp=2000000000
  // TODO(horo): Generate this sample token during the build.
  const std::string kFeatureToken(
      "AqwtRpuoLdc6MKSFH8TbzoLFvLouL8VXTv6OJIqQvRtJBynRMbAbFwjUlcwMuj4pXUBbquBj"
      "zj/w/d1H/ZsOcQIAAABWeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5leGFtcGxlLmNvbTo0"
      "NDMiLCAiZmVhdHVyZSI6ICJGb29CYXIiLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0=");
  http_info->headers->AddHeader(kOriginTrial + kFeatureToken);
  version()->SetMainScriptHttpResponseInfo(*http_info);
  EXPECT_FALSE(CheckOriginTrialToken(version()));
}

TEST_F(ForeignFetchRequestHandlerTest, CheckOriginTrialToken_ExpiredToken) {
  EXPECT_TRUE(CheckOriginTrialToken(version()));
  std::unique_ptr<net::HttpResponseInfo> http_info(
      CreateTestHttpResponseInfo());
  const std::string kOriginTrial("Origin-Trial: ");
  // Token for ForeignFetch which expired 2001-09-09.
  // generate_token.py valid.example.com ForeignFetch
  // --expire-timestamp=1000000000
  const std::string kFeatureToken(
      "AgBgj4Zhwzn85LJw7rzh4ZFRFqp49+9Es2SrCwZdDcoqtqQEjbvui4SKLn6GqMpr4DynGfJh"
      "tIy9dpOuK8PVTwkAAABceyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5leGFtcGxlLmNvbTo0"
      "NDMiLCAiZmVhdHVyZSI6ICJGb3JlaWduRmV0Y2giLCAiZXhwaXJ5IjogMTAwMDAwMDAwMH0"
      "=");
  http_info->headers->AddHeader(kOriginTrial + kFeatureToken);
  version()->SetMainScriptHttpResponseInfo(*http_info);
  EXPECT_FALSE(CheckOriginTrialToken(version()));
}

}  // namespace content
