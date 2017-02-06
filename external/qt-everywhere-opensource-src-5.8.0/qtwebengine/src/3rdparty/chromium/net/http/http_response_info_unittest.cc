// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_response_info.h"

#include "base/pickle.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class HttpResponseInfoTest : public testing::Test {
 protected:
  void SetUp() override {
    response_info_.headers = new HttpResponseHeaders("");
  }

  void PickleAndRestore(const HttpResponseInfo& response_info,
                        HttpResponseInfo* restored_response_info) const {
    base::Pickle pickle;
    response_info.Persist(&pickle, false, false);
    bool truncated = false;
    restored_response_info->InitFromPickle(pickle, &truncated);
  }

  HttpResponseInfo response_info_;
};

TEST_F(HttpResponseInfoTest, UnusedSincePrefetchDefault) {
  EXPECT_FALSE(response_info_.unused_since_prefetch);
}

TEST_F(HttpResponseInfoTest, UnusedSincePrefetchCopy) {
  response_info_.unused_since_prefetch = true;
  HttpResponseInfo response_info_clone(response_info_);
  EXPECT_TRUE(response_info_clone.unused_since_prefetch);
}

TEST_F(HttpResponseInfoTest, UnusedSincePrefetchPersistFalse) {
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_FALSE(restored_response_info.unused_since_prefetch);
}

TEST_F(HttpResponseInfoTest, UnusedSincePrefetchPersistTrue) {
  response_info_.unused_since_prefetch = true;
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_TRUE(restored_response_info.unused_since_prefetch);
}

TEST_F(HttpResponseInfoTest, PKPBypassPersistTrue) {
  response_info_.ssl_info.pkp_bypassed = true;
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_TRUE(restored_response_info.ssl_info.pkp_bypassed);
}

TEST_F(HttpResponseInfoTest, PKPBypassPersistFalse) {
  response_info_.ssl_info.pkp_bypassed = false;
  HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_FALSE(restored_response_info.ssl_info.pkp_bypassed);
}

TEST_F(HttpResponseInfoTest, AsyncRevalidationRequiredDefault) {
  EXPECT_FALSE(response_info_.async_revalidation_required);
}

TEST_F(HttpResponseInfoTest, AsyncRevalidationRequiredCopy) {
  response_info_.async_revalidation_required = true;
  net::HttpResponseInfo response_info_clone(response_info_);
  EXPECT_TRUE(response_info_clone.async_revalidation_required);
}

TEST_F(HttpResponseInfoTest, AsyncRevalidationRequiredAssign) {
  response_info_.async_revalidation_required = true;
  net::HttpResponseInfo response_info_clone;
  response_info_clone = response_info_;
  EXPECT_TRUE(response_info_clone.async_revalidation_required);
}

TEST_F(HttpResponseInfoTest, AsyncRevalidationRequiredNotPersisted) {
  response_info_.async_revalidation_required = true;
  net::HttpResponseInfo restored_response_info;
  PickleAndRestore(response_info_, &restored_response_info);
  EXPECT_FALSE(restored_response_info.async_revalidation_required);
}

}  // namespace

}  // namespace net
