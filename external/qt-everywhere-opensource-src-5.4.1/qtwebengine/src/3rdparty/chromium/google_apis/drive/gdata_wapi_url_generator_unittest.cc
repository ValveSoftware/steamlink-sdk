// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/gdata_wapi_url_generator.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace google_apis {

class GDataWapiUrlGeneratorTest : public testing::Test {
 public:
  GDataWapiUrlGeneratorTest()
      : url_generator_(
          GURL(GDataWapiUrlGenerator::kBaseUrlForProduction)) {
  }

 protected:
  GDataWapiUrlGenerator url_generator_;
};

TEST_F(GDataWapiUrlGeneratorTest, AddStandardUrlParams) {
  EXPECT_EQ("http://www.example.com/?v=3&alt=json&showroot=true",
            GDataWapiUrlGenerator::AddStandardUrlParams(
                GURL("http://www.example.com")).spec());
}

TEST_F(GDataWapiUrlGeneratorTest, GenerateEditUrl) {
  EXPECT_EQ(
      "https://docs.google.com/feeds/default/private/full/XXX?v=3&alt=json"
      "&showroot=true",
      url_generator_.GenerateEditUrl("XXX").spec());
}

TEST_F(GDataWapiUrlGeneratorTest, GenerateEditUrlWithoutParams) {
  EXPECT_EQ(
      "https://docs.google.com/feeds/default/private/full/XXX",
      url_generator_.GenerateEditUrlWithoutParams("XXX").spec());
}

TEST_F(GDataWapiUrlGeneratorTest, GenerateEditUrlWithEmbedOrigin) {
  url::AddStandardScheme("chrome-extension");

  EXPECT_EQ(
      "https://docs.google.com/feeds/default/private/full/XXX?v=3&alt=json"
      "&showroot=true&embedOrigin=chrome-extension%3A%2F%2Ftest",
      url_generator_.GenerateEditUrlWithEmbedOrigin(
          "XXX",
          GURL("chrome-extension://test")).spec());
  EXPECT_EQ(
      "https://docs.google.com/feeds/default/private/full/XXX?v=3&alt=json"
      "&showroot=true",
      url_generator_.GenerateEditUrlWithEmbedOrigin(
          "XXX",
          GURL()).spec());
}

}  // namespace google_apis
