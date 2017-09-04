// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "components/safe_browsing_db/util.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"
#include "components/subresource_filter/core/browser/subresource_filter_client.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kExampleUrlWithParams[] = "https://example.com/soceng?q=engsoc";
const char kTestUrl[] = "https://test.com";
const char kExampleUrl[] = "https://example.com";
const char kExampleLoginUrl[] = "https://example.com/login";

struct ActivationListTestData {
  bool should_add;
  const char* const activation_list;
  safe_browsing::SBThreatType threat_type;
  safe_browsing::ThreatPatternType threat_type_metadata;
};

const ActivationListTestData kActivationListTestData[] = {
    {false, "", safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {false, subresource_filter::kActivationListSocialEngineeringAdsInterstitial,
     safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
     safe_browsing::ThreatPatternType::NONE},
    {false, subresource_filter::kActivationListSocialEngineeringAdsInterstitial,
     safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
     safe_browsing::ThreatPatternType::MALWARE_LANDING},
    {false, subresource_filter::kActivationListSocialEngineeringAdsInterstitial,
     safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
     safe_browsing::ThreatPatternType::MALWARE_DISTRIBUTION},
    {true, subresource_filter::kActivationListSocialEngineeringAdsInterstitial,
     safe_browsing::SB_THREAT_TYPE_URL_MALWARE,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {false, subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_API_ABUSE,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {false, subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_BLACKLISTED_RESOURCE,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {false, subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {false, subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_BINARY_MALWARE_URL,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {false, subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_URL_UNWANTED,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {false, subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_URL_MALWARE,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {false, subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {false, subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_SAFE,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {true, subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
     safe_browsing::ThreatPatternType::NONE},
};

}  // namespace

namespace subresource_filter {

class MockSubresourceFilterDriver : public ContentSubresourceFilterDriver {
 public:
  explicit MockSubresourceFilterDriver(
      content::RenderFrameHost* render_frame_host)
      : ContentSubresourceFilterDriver(render_frame_host) {}

  ~MockSubresourceFilterDriver() override = default;

  MOCK_METHOD2(ActivateForProvisionalLoad, void(ActivationState, const GURL&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSubresourceFilterDriver);
};

class MockSubresourceFilterClient : public SubresourceFilterClient {
 public:
  MockSubresourceFilterClient() {}

  ~MockSubresourceFilterClient() override = default;

  MOCK_METHOD1(ToggleNotificationVisibility, void(bool));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSubresourceFilterClient);
};

class ContentSubresourceFilterDriverFactoryTest
    : public content::RenderViewHostTestHarness {
 public:
  ContentSubresourceFilterDriverFactoryTest() {}
  ~ContentSubresourceFilterDriverFactoryTest() override {}

  // content::RenderViewHostImplTestHarness:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    client_ = new MockSubresourceFilterClient();
    ContentSubresourceFilterDriverFactory::CreateForWebContents(
        web_contents(), base::WrapUnique(client()));
    driver_ = new MockSubresourceFilterDriver(main_rfh());
    SetDriverForFrameHostForTesting(main_rfh(), driver());
    // Add a subframe.
    content::RenderFrameHostTester* rfh_tester =
        content::RenderFrameHostTester::For(main_rfh());
    rfh_tester->InitializeRenderFrameIfNeeded();
    subframe_rfh_ = rfh_tester->AppendChild("Child");
    subframe_driver_ = new MockSubresourceFilterDriver(subframe_rfh());
    SetDriverForFrameHostForTesting(subframe_rfh(), subframe_driver());
  }

  void SetDriverForFrameHostForTesting(
      content::RenderFrameHost* render_frame_host,
      ContentSubresourceFilterDriver* driver) {
    factory()->SetDriverForFrameHostForTesting(render_frame_host,
                                               base::WrapUnique(driver));
  }

  ContentSubresourceFilterDriverFactory* factory() {
    return ContentSubresourceFilterDriverFactory::FromWebContents(
        web_contents());
  }

  MockSubresourceFilterClient* client() { return client_; }
  MockSubresourceFilterDriver* driver() { return driver_; }

  MockSubresourceFilterDriver* subframe_driver() { return subframe_driver_; }
  content::RenderFrameHost* subframe_rfh() { return subframe_rfh_; }

  void BlacklistURLWithRedirectsNavigateAndCommit(
      const GURL& bad_url,
      const std::vector<GURL>& redirects,
      const GURL& url,
      bool should_activate) {
    EXPECT_CALL(*client(), ToggleNotificationVisibility(false)).Times(1);
    content::WebContentsTester::For(web_contents())->StartNavigation(url);
    ::testing::Mock::VerifyAndClearExpectations(client());
    factory()->OnMainResourceMatchedSafeBrowsingBlacklist(
        bad_url, redirects, safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
        safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS);

    ActivateAndExpectForFrameHostForUrl(driver(), main_rfh(), url,
                                        should_activate);
    content::RenderFrameHostTester::For(main_rfh())
        ->SimulateNavigationCommit(url);
  }

  void ActivateAndExpectForFrameHostForUrl(MockSubresourceFilterDriver* driver,
                                           content::RenderFrameHost* rfh,
                                           const GURL& url,
                                           bool should_activate) {
    EXPECT_CALL(*driver, ActivateForProvisionalLoad(::testing::_, ::testing::_))
        .Times(should_activate);
    factory()->ReadyToCommitNavigationInternal(rfh, url);
    ::testing::Mock::VerifyAndClearExpectations(driver);
  }

  void NavigateAndCommitSubframe(const GURL& url, bool should_activate) {
    EXPECT_CALL(*subframe_driver(),
                ActivateForProvisionalLoad(::testing::_, ::testing::_))
        .Times(should_activate);
    EXPECT_CALL(*client(), ToggleNotificationVisibility(::testing::_)).Times(0);

    factory()->ReadyToCommitNavigationInternal(subframe_rfh(), url);
    ::testing::Mock::VerifyAndClearExpectations(subframe_driver());
    ::testing::Mock::VerifyAndClearExpectations(client());
  }

  void BlacklistURLWithRedirectsNavigateMainFrameAndSubrame(
      const GURL& bad_url,
      const std::vector<GURL>& redirects,
      const GURL& url,
      bool should_activate) {
    BlacklistURLWithRedirectsNavigateAndCommit(bad_url, redirects, url,
                                               should_activate);

    NavigateAndCommitSubframe(GURL(kExampleLoginUrl), should_activate);
  }

  void SpecialCaseNavSeq(const GURL& bad_url,
                         const std::vector<GURL>& redirects,
                         bool should_activate) {
    // This test-case examinse the nevigation woth following sequence of event:
    //   DidStartProvisional(main, "example.com")
    //   ReadyToCommitMainFrameNavigation(“example.com”)
    //   DidCommitProvisional(main, "example.com")
    //   DidStartProvisional(sub, "example.com/login")
    //   DidCommitProvisional(sub, "example.com/login")
    //   DidCommitProvisional(main, "example.com#ref")

    BlacklistURLWithRedirectsNavigateMainFrameAndSubrame(
        bad_url, redirects, GURL(kExampleUrl), should_activate);
    EXPECT_CALL(*driver(),
                ActivateForProvisionalLoad(::testing::_, ::testing::_))
        .Times(0);
    EXPECT_CALL(*client(), ToggleNotificationVisibility(::testing::_)).Times(1);
    content::RenderFrameHostTester::For(main_rfh())
        ->SimulateNavigationCommit(GURL(kExampleUrl));
    ::testing::Mock::VerifyAndClearExpectations(driver());
    ::testing::Mock::VerifyAndClearExpectations(client());
  }

 private:
  // Owned by the factory.
  MockSubresourceFilterClient* client_;
  MockSubresourceFilterDriver* driver_;

  content::RenderFrameHost* subframe_rfh_;
  MockSubresourceFilterDriver* subframe_driver_;

  DISALLOW_COPY_AND_ASSIGN(ContentSubresourceFilterDriverFactoryTest);
};

class ContentSubresourceFilterDriverFactoryThreatTypeTest
    : public ContentSubresourceFilterDriverFactoryTest,
      public ::testing::WithParamInterface<ActivationListTestData> {
 public:
  ContentSubresourceFilterDriverFactoryThreatTypeTest() {}
  ~ContentSubresourceFilterDriverFactoryThreatTypeTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSubresourceFilterDriverFactoryThreatTypeTest);
};

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       ActivateForFrameHostNotNeeded) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_DISABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeNoSites, kActivationListSocialEngineeringAdsInterstitial);
  ActivateAndExpectForFrameHostForUrl(driver(), main_rfh(), GURL(kTestUrl),
                                      false /* should_activate */);
  const GURL url(kExampleUrlWithParams);
  BlacklistURLWithRedirectsNavigateAndCommit(url, std::vector<GURL>(), url,
                                             false /* should_activate */);
  BlacklistURLWithRedirectsNavigateAndCommit(url, std::vector<GURL>(),
                                             GURL("https://not-example.com"),
                                             false /* should_activate */);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest, ActivateForFrameHostNeeded) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial);

  const GURL url(kExampleUrlWithParams);
  BlacklistURLWithRedirectsNavigateAndCommit(url, std::vector<GURL>(), url,
                                             true /* should_activate */);
  BlacklistURLWithRedirectsNavigateAndCommit(
      url, std::vector<GURL>(), GURL(kExampleUrl), false /* should_activate */);
}

TEST_P(ContentSubresourceFilterDriverFactoryThreatTypeTest, NonSocEngHit) {
  const ActivationListTestData& test_data = GetParam();
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeNoSites, test_data.activation_list);

  std::vector<GURL> redirects;
  redirects.push_back(GURL("https://example1.com"));
  redirects.push_back(GURL("https://example2.com"));
  redirects.push_back(GURL("https://example3.com"));

  const GURL test_url("https://example.com/nonsoceng?q=engsocnon");

  BlacklistURLWithRedirectsNavigateAndCommit(GURL::EmptyGURL(),
                                             std::vector<GURL>(), test_url,
                                             false /* should_activate */);
  EXPECT_CALL(*client(), ToggleNotificationVisibility(false)).Times(1);
  content::WebContentsTester::For(web_contents())->StartNavigation(test_url);
  ::testing::Mock::VerifyAndClearExpectations(client());
  factory()->OnMainResourceMatchedSafeBrowsingBlacklist(
      test_url, redirects, test_data.threat_type,
      test_data.threat_type_metadata);
  ActivateAndExpectForFrameHostForUrl(driver(), main_rfh(), test_url, false);
  content::RenderFrameHostTester::For(main_rfh())
      ->SimulateNavigationCommit(test_url);
};

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       ActivationPromptNotShownForNonMainFrames) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial);
  BlacklistURLWithRedirectsNavigateAndCommit(
      GURL::EmptyGURL(), std::vector<GURL>(), GURL(kExampleUrl),
      false /* should_prompt */);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       DidStartProvisionalLoadScopeAllSitesStateDryRun) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateDryRun,
      kActivationScopeAllSites,
      kActivationListSocialEngineeringAdsInterstitial);
  const GURL url(kExampleUrlWithParams);
  BlacklistURLWithRedirectsNavigateMainFrameAndSubrame(
      url, std::vector<GURL>(), url, true /* should_activate */);
  factory()->AddHostOfURLToWhitelistSet(url);
  BlacklistURLWithRedirectsNavigateMainFrameAndSubrame(
      url, std::vector<GURL>(), GURL(kExampleUrlWithParams),
      false /* should_activate */);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       DidStartProvisionalLoadScopeAllSitesStateEnabled) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeAllSites,
      kActivationListSocialEngineeringAdsInterstitial);
  const GURL url(kExampleUrlWithParams);
  BlacklistURLWithRedirectsNavigateMainFrameAndSubrame(
      url, std::vector<GURL>(), url, true /* should_activate */);
  factory()->AddHostOfURLToWhitelistSet(url);
  BlacklistURLWithRedirectsNavigateMainFrameAndSubrame(
      url, std::vector<GURL>(), GURL(kExampleUrlWithParams),
      false /* should_activate */);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       DidStartProvisionalLoadScopeActivationListSitesStateEnabled) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial);
  const GURL url(kExampleUrlWithParams);
  BlacklistURLWithRedirectsNavigateMainFrameAndSubrame(
      url, std::vector<GURL>(), url, true /* should_activate */);
  factory()->AddHostOfURLToWhitelistSet(url);
  BlacklistURLWithRedirectsNavigateMainFrameAndSubrame(
      url, std::vector<GURL>(), GURL(kExampleUrlWithParams),
      false /* should_activate */);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       DidStartProvisionalLoadScopeActivationListSitesStateDryRun) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateDryRun,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial);
  const GURL url(kExampleUrlWithParams);
  BlacklistURLWithRedirectsNavigateMainFrameAndSubrame(
      url, std::vector<GURL>(), url, true /* should_activate */);
  factory()->AddHostOfURLToWhitelistSet(url);
  BlacklistURLWithRedirectsNavigateMainFrameAndSubrame(
      url, std::vector<GURL>(), url, false /* should_activate */);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       DidStartProvisionalLoadScopeNoSites) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateDryRun,
      kActivationScopeNoSites, kActivationListSocialEngineeringAdsInterstitial);
  BlacklistURLWithRedirectsNavigateMainFrameAndSubrame(
      GURL::EmptyGURL(), std::vector<GURL>(), GURL(kExampleUrlWithParams),
      false /* should_activate */);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       DidStartProvisionalLoadScopeActivationList) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateDisabled,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial);
  BlacklistURLWithRedirectsNavigateMainFrameAndSubrame(
      GURL::EmptyGURL(), std::vector<GURL>(), GURL(kExampleUrlWithParams),
      false /* should_activate */);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       SpecialCaseNavigationAllSitesDryRun) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateDryRun,
      kActivationScopeAllSites);
  SpecialCaseNavSeq(GURL::EmptyGURL(), std::vector<GURL>(),
                    true /* should_activate */);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       SpecialCaseNavigationAllSitesEnabled) {
  // Check that when the experiment is enabled for all site, the activation
  // signal is always sent.
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeAllSites);
  SpecialCaseNavSeq(GURL::EmptyGURL(), std::vector<GURL>(),
                    true /* should_activate */);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       SpecialCaseNavigationActivationListEnabled) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial);
  SpecialCaseNavSeq(GURL(kExampleUrl), std::vector<GURL>(),
                    true /* should_activate */);
}

INSTANTIATE_TEST_CASE_P(NoSonEngHit,
                        ContentSubresourceFilterDriverFactoryThreatTypeTest,
                        ::testing::ValuesIn(kActivationListTestData));

}  // namespace subresource_filter
