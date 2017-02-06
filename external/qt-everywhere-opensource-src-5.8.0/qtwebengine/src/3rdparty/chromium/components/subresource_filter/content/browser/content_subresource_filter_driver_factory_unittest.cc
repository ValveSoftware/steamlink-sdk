// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "components/safe_browsing_db/util.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace subresource_filter {

class ContentSubresourceFilterDriverFactoryTest
    : public content::RenderViewHostTestHarness {
 public:
  ContentSubresourceFilterDriverFactoryTest() {}
  ~ContentSubresourceFilterDriverFactoryTest() override {}

  // content::RenderViewHostTestHarness:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    ContentSubresourceFilterDriverFactory::CreateForWebContents(web_contents());
  }

  ContentSubresourceFilterDriverFactory* factory() {
    return ContentSubresourceFilterDriverFactory::FromWebContents(
        web_contents());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSubresourceFilterDriverFactoryTest);
};

class ContentSubresourceFilterDriverFactoryThreatTypeTest
    : public ContentSubresourceFilterDriverFactoryTest,
      public ::testing::WithParamInterface<safe_browsing::ThreatPatternType> {
 public:
  ContentSubresourceFilterDriverFactoryThreatTypeTest() {}
  ~ContentSubresourceFilterDriverFactoryThreatTypeTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSubresourceFilterDriverFactoryThreatTypeTest);
};

TEST_F(ContentSubresourceFilterDriverFactoryTest, SocEngHitEmptyRedirects) {
  factory()->OnMainResourceMatchedSafeBrowsingBlacklist(
      GURL("https://example.com/soceng?q=engsoc"), std::vector<GURL>(),
      safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS);
  EXPECT_EQ(1U, factory()->activation_set().size());
  EXPECT_TRUE(factory()->ShouldActivateForURL(GURL("https://example.com")));
  EXPECT_TRUE(factory()->ShouldActivateForURL(GURL("http://example.com")));
  EXPECT_TRUE(
      factory()->ShouldActivateForURL(GURL("http://example.com/42?q=42!")));
  EXPECT_TRUE(
      factory()->ShouldActivateForURL(GURL("https://example.com/42?q=42!")));
  EXPECT_TRUE(
      factory()->ShouldActivateForURL(GURL("http://example.com/awesomepath")));
}

TEST_F(ContentSubresourceFilterDriverFactoryTest, SocEngHitWithRedirects) {
  std::vector<GURL> redirects;
  redirects.push_back(GURL("https://example1.com"));
  redirects.push_back(GURL("https://example2.com"));
  redirects.push_back(GURL("https://example3.com"));
  factory()->OnMainResourceMatchedSafeBrowsingBlacklist(
      GURL("https://example.com/engsoc/q=soceng"), redirects,
      safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS);
  EXPECT_EQ(4U, factory()->activation_set().size());
  EXPECT_TRUE(factory()->ShouldActivateForURL(GURL("https://example.com")));
  for (const auto& redirect : redirects) {
    EXPECT_TRUE(factory()->ShouldActivateForURL(redirect));
    EXPECT_TRUE(factory()->ShouldActivateForURL(redirect.GetWithEmptyPath()));
    EXPECT_TRUE(
        factory()->ShouldActivateForURL(GURL("http://" + redirect.host())));
    EXPECT_TRUE(factory()->ShouldActivateForURL(
        GURL("http://" + redirect.host() + "/path?q=q")));
  }
}

TEST_P(ContentSubresourceFilterDriverFactoryThreatTypeTest, NonSocEngHit) {
  std::vector<GURL> redirects;
  redirects.push_back(GURL("https://example1.com"));
  redirects.push_back(GURL("https://example2.com"));
  redirects.push_back(GURL("https://example3.com"));

  const GURL test_url("https://example.com/nonsoceng?q=engsocnon");
  factory()->OnMainResourceMatchedSafeBrowsingBlacklist(
      GURL(test_url), std::vector<GURL>(), GetParam());
  EXPECT_EQ(0U, factory()->activation_set().size());
  EXPECT_FALSE(factory()->ShouldActivateForURL(GURL(test_url)));
  EXPECT_FALSE(
      factory()->ShouldActivateForURL(GURL(test_url.GetWithEmptyPath())));
  EXPECT_FALSE(factory()->ShouldActivateForURL(
      GURL("http://" + test_url.host() + "/path?q=q")));
  for (const auto& redirect : redirects) {
    EXPECT_FALSE(factory()->ShouldActivateForURL(redirect));
    EXPECT_FALSE(factory()->ShouldActivateForURL(redirect.GetWithEmptyPath()));
    EXPECT_FALSE(
        factory()->ShouldActivateForURL(GURL("http://" + redirect.host())));
    EXPECT_FALSE(factory()->ShouldActivateForURL(
        GURL("http://" + redirect.host() + "/path?q=q")));
  }
};

INSTANTIATE_TEST_CASE_P(
    NoSonEngHit,
    ContentSubresourceFilterDriverFactoryThreatTypeTest,
    testing::Values(
        safe_browsing::ThreatPatternType::NONE,
        safe_browsing::ThreatPatternType::MALWARE_LANDING,
        safe_browsing::ThreatPatternType::MALWARE_DISTRIBUTION,
        safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_LANDING));

}  // namespace subresource_filter
