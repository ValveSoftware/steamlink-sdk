// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/refresh_token_annotation_request.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

class RefreshTokenAnnotationRequestTest : public testing::Test {
 protected:
  TestingPrefServiceSimple pref_service_;

  void SetUp() override {
    pref_service_.registry()->RegisterInt64Pref(
        prefs::kGoogleServicesRefreshTokenAnnotateScheduledTime,
        base::Time().ToInternalValue());
  }
};

// Verifies that scheduling of RefreshTokenAnnotationRequest happens correctly
TEST_F(RefreshTokenAnnotationRequestTest, ShouldSendNow) {
  // When there is no value in preferences we shouldn't send request.
  base::Time now = base::Time::Now();
  EXPECT_FALSE(RefreshTokenAnnotationRequest::ShouldSendNow(&pref_service_));
  base::Time scheduled = base::Time::FromInternalValue(pref_service_.GetInt64(
      prefs::kGoogleServicesRefreshTokenAnnotateScheduledTime));
  // Next request should be scheduled sometime between 0 and 20 days from now.
  EXPECT_LE(now, scheduled);
  EXPECT_LE(scheduled, base::Time::Now() + base::TimeDelta::FromDays(20));

  // There is value in preferences, but time hasn't come yet to send request.
  // We shouldn't send request and shouldn't update preference.
  base::Time prefs_time = base::Time::Now() + base::TimeDelta::FromDays(365);
  pref_service_.SetInt64(
      prefs::kGoogleServicesRefreshTokenAnnotateScheduledTime,
      prefs_time.ToInternalValue());
  EXPECT_FALSE(RefreshTokenAnnotationRequest::ShouldSendNow(&pref_service_));
  scheduled = base::Time::FromInternalValue(pref_service_.GetInt64(
      prefs::kGoogleServicesRefreshTokenAnnotateScheduledTime));
  EXPECT_EQ(prefs_time, scheduled);

  // It is time to send request. We should update next request scheduled time.
  prefs_time = base::Time::Now() - base::TimeDelta::FromDays(365);
  pref_service_.SetInt64(
      prefs::kGoogleServicesRefreshTokenAnnotateScheduledTime,
      prefs_time.ToInternalValue());
  EXPECT_TRUE(RefreshTokenAnnotationRequest::ShouldSendNow(&pref_service_));
  scheduled = base::Time::FromInternalValue(pref_service_.GetInt64(
      prefs::kGoogleServicesRefreshTokenAnnotateScheduledTime));
  EXPECT_LE(now, scheduled);
  EXPECT_LE(scheduled, base::Time::Now() + base::TimeDelta::FromDays(20));
}

TEST_F(RefreshTokenAnnotationRequestTest, CreateApiCallBody) {
  RefreshTokenAnnotationRequest request(
      scoped_refptr<net::URLRequestContextGetter>(), "39.0 (stable)",
      "device_id1", "client_id1", base::Closure());
  std::string body = request.CreateApiCallBody();
  std::string expected_body =
      "force=true"
      "&response_type=none"
      "&scope=https://www.googleapis.com/auth/userinfo.email"
      "&client_id=client_id1"
      "&device_id=device_id1"
      "&device_type=chrome"
      "&lib_ver=39.0+(stable)";
  EXPECT_EQ(expected_body, body);
}
