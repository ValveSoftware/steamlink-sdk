// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/google_service_auth_error.h"

#include <memory>
#include <string>

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using base::ExpectDictStringValue;

class GoogleServiceAuthErrorTest : public testing::Test {};

void TestSimpleState(GoogleServiceAuthError::State state) {
  GoogleServiceAuthError error(state);
  std::unique_ptr<base::DictionaryValue> value(error.ToValue());
  EXPECT_EQ(1u, value->size());
  std::string state_str;
  EXPECT_TRUE(value->GetString("state", &state_str));
  EXPECT_FALSE(state_str.empty());
  EXPECT_NE("CONNECTION_FAILED", state_str);
  EXPECT_NE("CAPTCHA_REQUIRED", state_str);
}

TEST_F(GoogleServiceAuthErrorTest, SimpleToValue) {
  for (int i = GoogleServiceAuthError::NONE;
       i <= GoogleServiceAuthError::USER_NOT_SIGNED_UP; ++i) {
    TestSimpleState(static_cast<GoogleServiceAuthError::State>(i));
  }
}

TEST_F(GoogleServiceAuthErrorTest, None) {
  GoogleServiceAuthError error(GoogleServiceAuthError::AuthErrorNone());
  std::unique_ptr<base::DictionaryValue> value(error.ToValue());
  EXPECT_EQ(1u, value->size());
  ExpectDictStringValue("NONE", *value, "state");
}

TEST_F(GoogleServiceAuthErrorTest, ConnectionFailed) {
  GoogleServiceAuthError error(
      GoogleServiceAuthError::FromConnectionError(net::OK));
  std::unique_ptr<base::DictionaryValue> value(error.ToValue());
  EXPECT_EQ(2u, value->size());
  ExpectDictStringValue("CONNECTION_FAILED", *value, "state");
  ExpectDictStringValue("net::OK", *value, "networkError");
}

}  // namespace
