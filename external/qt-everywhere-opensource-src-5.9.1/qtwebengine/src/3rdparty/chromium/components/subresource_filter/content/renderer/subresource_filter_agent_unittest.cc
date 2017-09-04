// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/renderer/subresource_filter_agent.h"

#include <memory>
#include <utility>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/test/histogram_tester.h"
#include "components/subresource_filter/content/common/subresource_filter_messages.h"
#include "components/subresource_filter/content/renderer/ruleset_dealer.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebDocumentSubresourceFilter.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "url/gurl.h"

namespace subresource_filter {

namespace {

// The SubresourceFilterAgent with its dependencies on Blink mocked out.
//
// This approach is somewhat rudimentary, but appears to be the best compromise
// considering the alternatives:
//  -- Passing in a TestRenderFrame would itself require bringing up a
//     significant number of supporting classes.
//  -- Using a RenderViewTest would not allow having any non-filtered resource
//     loads due to not having a child thread and ResourceDispatcher.
// The real implementations of the mocked-out methods are exercised in:
//   chrome/browser/subresource_filter/subresource_filter_browsertest.cc.
class SubresourceFilterAgentUnderTest : public SubresourceFilterAgent {
 public:
  explicit SubresourceFilterAgentUnderTest(RulesetDealer* ruleset_dealer)
      : SubresourceFilterAgent(nullptr /* RenderFrame */, ruleset_dealer) {}
  ~SubresourceFilterAgentUnderTest() = default;

  MOCK_METHOD0(GetAncestorDocumentURLs, std::vector<GURL>());
  MOCK_METHOD0(OnSetSubresourceFilterForCommittedLoadCalled, void());
  MOCK_METHOD0(SignalFirstSubresourceDisallowedForCommittedLoad, void());
  void SetSubresourceFilterForCommittedLoad(
      std::unique_ptr<blink::WebDocumentSubresourceFilter> filter) override {
    last_injected_filter_ = std::move(filter);
    OnSetSubresourceFilterForCommittedLoadCalled();
  }

  blink::WebDocumentSubresourceFilter* filter() {
    return last_injected_filter_.get();
  }

  std::unique_ptr<blink::WebDocumentSubresourceFilter> TakeFilter() {
    return std::move(last_injected_filter_);
  }

 private:
  std::unique_ptr<blink::WebDocumentSubresourceFilter> last_injected_filter_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterAgentUnderTest);
};

const char kTestFirstURL[] = "http://example.com/alpha";
const char kTestSecondURL[] = "http://example.com/beta";
const char kTestFirstURLPathSuffix[] = "alpha";
const char kTestSecondURLPathSuffix[] = "beta";
const char kTestBothURLsPathSuffix[] = "a";

}  // namespace

class SubresourceFilterAgentTest : public ::testing::Test {
 public:
  SubresourceFilterAgentTest() {}

 protected:
  void SetUp() override { ResetAgent(); }

  void ResetAgent() {
    agent_.reset(new ::testing::StrictMock<SubresourceFilterAgentUnderTest>(
        &ruleset_dealer_));
    ON_CALL(*agent(), GetAncestorDocumentURLs())
        .WillByDefault(::testing::Return(std::vector<GURL>(
            {GURL("http://inner.com/"), GURL("http://outer.com/")})));
  }

  void SetTestRulesetToDisallowURLsWithPathSuffix(base::StringPiece suffix) {
    testing::TestRulesetPair test_ruleset_pair;
    ASSERT_NO_FATAL_FAILURE(
        test_ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
            suffix, &test_ruleset_pair));
    ruleset_dealer_.SetRulesetFile(
        testing::TestRuleset::Open(test_ruleset_pair.indexed));
  }

  void StartLoadWithoutSettingActivationState() {
    agent_as_rfo()->DidStartProvisionalLoad();
    agent_as_rfo()->DidCommitProvisionalLoad(
        true /* is_new_navigation */, false /* is_same_page_navigation */);
  }

  void StartLoadAndSetActivationState(ActivationState activation_state) {
    agent_as_rfo()->DidStartProvisionalLoad();
    EXPECT_TRUE(agent_as_rfo()->OnMessageReceived(
        SubresourceFilterMsg_ActivateForProvisionalLoad(0, activation_state,
                                                        GURL())));
    agent_as_rfo()->DidCommitProvisionalLoad(
        true /* is_new_navigation */, false /* is_same_page_navigation */);
  }

  void FinishLoad() { agent_as_rfo()->DidFinishLoad(); }

  void ExpectSubresourceFilterGetsInjected() {
    EXPECT_CALL(*agent(), GetAncestorDocumentURLs());
    EXPECT_CALL(*agent(), OnSetSubresourceFilterForCommittedLoadCalled());
  }

  void ExpectSignalAboutFirstSubresourceDisallowed() {
    EXPECT_CALL(*agent(), SignalFirstSubresourceDisallowedForCommittedLoad());
  }

  void ExpectNoSubresourceFilterGetsInjected() {
    EXPECT_CALL(*agent(), OnSetSubresourceFilterForCommittedLoadCalled())
        .Times(0);
  }

  void ExpectNoSignalAboutFirstSubresourceDisallowed() {
    EXPECT_CALL(*agent(), SignalFirstSubresourceDisallowedForCommittedLoad())
        .Times(0);
  }

  void ExpectLoadAllowed(base::StringPiece url_spec, bool allowed) {
    blink::WebURL url = GURL(url_spec);
    blink::WebURLRequest::RequestContext request_context =
        blink::WebURLRequest::RequestContextImage;
    EXPECT_EQ(allowed, agent()->filter()->allowLoad(url, request_context));
  }

  SubresourceFilterAgentUnderTest* agent() { return agent_.get(); }
  content::RenderFrameObserver* agent_as_rfo() {
    return static_cast<content::RenderFrameObserver*>(agent_.get());
  }

 private:
  testing::TestRulesetCreator test_ruleset_creator_;
  RulesetDealer ruleset_dealer_;

  std::unique_ptr<SubresourceFilterAgentUnderTest> agent_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterAgentTest);
};

TEST_F(SubresourceFilterAgentTest, DisabledByDefault_NoFilterIsInjected) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestBothURLsPathSuffix));
  ExpectNoSubresourceFilterGetsInjected();
  StartLoadWithoutSettingActivationState();
  FinishLoad();
}

TEST_F(SubresourceFilterAgentTest, Disabled_NoFilterIsInjected) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestBothURLsPathSuffix));
  ExpectNoSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(ActivationState::DISABLED);
  FinishLoad();
}

TEST_F(SubresourceFilterAgentTest,
       EnabledButRulesetUnavailable_NoFilterIsInjected) {
  ExpectNoSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(ActivationState::ENABLED);
  FinishLoad();
}

TEST_F(SubresourceFilterAgentTest, Enabled_FilteringIsInEffectForOneLoad) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));

  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(ActivationState::ENABLED);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  ExpectSignalAboutFirstSubresourceDisallowed();
  ExpectLoadAllowed(kTestFirstURL, false);
  ExpectLoadAllowed(kTestSecondURL, true);
  FinishLoad();

  ExpectNoSubresourceFilterGetsInjected();
  StartLoadWithoutSettingActivationState();
  FinishLoad();
}

TEST_F(SubresourceFilterAgentTest, Enabled_NewRulesetIsPickedUpAtNextLoad) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));
  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(ActivationState::ENABLED);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  // Set the new ruleset just after the deadline for being used for the current
  // load, to exercises doing filtering based on obseleted rulesets.
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestSecondURLPathSuffix));

  ExpectSignalAboutFirstSubresourceDisallowed();
  ExpectLoadAllowed(kTestFirstURL, false);
  ExpectLoadAllowed(kTestSecondURL, true);
  FinishLoad();

  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(ActivationState::ENABLED);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  ExpectSignalAboutFirstSubresourceDisallowed();
  ExpectLoadAllowed(kTestFirstURL, true);
  ExpectLoadAllowed(kTestSecondURL, false);
  FinishLoad();
}

// If a provisional load is aborted, the RenderFrameObservers might not receive
// any further notifications about that load. It is thus possible that there
// will be two RenderFrameObserver::DidStartProvisionalLoad in a row. Make sure
// that the activation decision does not outlive the first provisional load.
TEST_F(SubresourceFilterAgentTest,
       Enabled_FilteringNoLongerEffectAfterProvisionalLoadIsCancelled) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestBothURLsPathSuffix));
  ExpectNoSubresourceFilterGetsInjected();
  agent_as_rfo()->DidStartProvisionalLoad();
  EXPECT_TRUE(agent_as_rfo()->OnMessageReceived(
      SubresourceFilterMsg_ActivateForProvisionalLoad(
          0, ActivationState::ENABLED, GURL())));
  agent_as_rfo()->DidStartProvisionalLoad();
  agent_as_rfo()->DidCommitProvisionalLoad(true /* is_new_navigation */,
                                           false /* is_same_page_navigation */);
  FinishLoad();
}

TEST_F(SubresourceFilterAgentTest, DryRun_ResourcesAreEvaluatedButNotFiltered) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));
  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(ActivationState::DRYRUN);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  ExpectLoadAllowed(kTestFirstURL, true);
  ExpectLoadAllowed(kTestSecondURL, true);
  FinishLoad();
}

TEST_F(SubresourceFilterAgentTest,
       SignalFirstSubresourceDisallowed_OncePerDocumentLoad) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));
  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(ActivationState::ENABLED);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  ExpectSignalAboutFirstSubresourceDisallowed();
  ExpectLoadAllowed(kTestFirstURL, false);
  ExpectNoSignalAboutFirstSubresourceDisallowed();
  ExpectLoadAllowed(kTestFirstURL, false);
  ExpectLoadAllowed(kTestSecondURL, true);
  FinishLoad();

  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(ActivationState::ENABLED);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  ExpectLoadAllowed(kTestSecondURL, true);
  ExpectSignalAboutFirstSubresourceDisallowed();
  ExpectLoadAllowed(kTestFirstURL, false);
  FinishLoad();
}

TEST_F(SubresourceFilterAgentTest,
       SignalFirstSubresourceDisallowed_ComesAfterAgentDestroyed) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));
  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(ActivationState::ENABLED);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  auto filter = agent()->TakeFilter();
  ResetAgent();
  EXPECT_FALSE(filter->allowLoad(GURL(kTestFirstURL),
                                 blink::WebURLRequest::RequestContextImage));
}

TEST_F(SubresourceFilterAgentTest, Disabled_HistogramSamples) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestBothURLsPathSuffix));
  StartLoadWithoutSettingActivationState();
  FinishLoad();

  histogram_tester.ExpectUniqueSample(
      "SubresourceFilter.DocumentLoad.ActivationState",
      static_cast<int>(ActivationState::DISABLED), 1);
  histogram_tester.ExpectTotalCount(
      "SubresourceFilter.DocumentLoad.RulesetIsAvailable", 0);
  histogram_tester.ExpectTotalCount(
      "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Total", 0);
  histogram_tester.ExpectTotalCount(
      "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Evaluated", 0);
  histogram_tester.ExpectTotalCount(
      "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Disallowed", 0);
}

TEST_F(SubresourceFilterAgentTest,
       EnabledButRulesetUnavailable_HistogramSamples) {
  base::HistogramTester histogram_tester;
  StartLoadAndSetActivationState(ActivationState::ENABLED);
  FinishLoad();

  histogram_tester.ExpectUniqueSample(
      "SubresourceFilter.DocumentLoad.ActivationState",
      static_cast<int>(ActivationState::ENABLED), 1);
  histogram_tester.ExpectUniqueSample(
      "SubresourceFilter.DocumentLoad.RulesetIsAvailable", 0, 1);
  histogram_tester.ExpectTotalCount(
      "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Total", 0);
  histogram_tester.ExpectTotalCount(
      "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Evaluated", 0);
  histogram_tester.ExpectTotalCount(
      "SubresourceFilter.DocumentLoad.NumSubresourceLoads.MatchedRules", 0);
  histogram_tester.ExpectTotalCount(
      "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Disallowed", 0);
}

TEST_F(SubresourceFilterAgentTest, Enabled_HistogramSamples) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));
  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(ActivationState::ENABLED);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  ExpectSignalAboutFirstSubresourceDisallowed();
  ExpectLoadAllowed(kTestFirstURL, false);
  ExpectLoadAllowed(kTestFirstURL, false);
  ExpectLoadAllowed(kTestSecondURL, true);
  FinishLoad();

  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(ActivationState::ENABLED);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  ExpectSignalAboutFirstSubresourceDisallowed();
  ExpectLoadAllowed(kTestFirstURL, false);
  ExpectLoadAllowed(kTestSecondURL, true);
  FinishLoad();

  histogram_tester.ExpectUniqueSample(
      "SubresourceFilter.DocumentLoad.ActivationState",
      static_cast<int>(ActivationState::ENABLED), 2);
  histogram_tester.ExpectUniqueSample(
      "SubresourceFilter.DocumentLoad.RulesetIsAvailable", 1, 2);
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Total"),
              ::testing::ElementsAre(base::Bucket(2, 1), base::Bucket(3, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Evaluated"),
      ::testing::ElementsAre(base::Bucket(2, 1), base::Bucket(3, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "SubresourceFilter.DocumentLoad.NumSubresourceLoads.MatchedRules"),
      ::testing::ElementsAre(base::Bucket(1, 1), base::Bucket(2, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Disallowed"),
      ::testing::ElementsAre(base::Bucket(1, 1), base::Bucket(2, 1)));
}

TEST_F(SubresourceFilterAgentTest, DryRun_HistogramSamples) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));
  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(ActivationState::DRYRUN);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  // In dry-run mode, loads to the first URL should be recorded as
  // `MatchedRules`, but still be allowed to proceed and not recorded as
  // `Disallowed`.
  ExpectLoadAllowed(kTestFirstURL, true);
  ExpectLoadAllowed(kTestFirstURL, true);
  ExpectLoadAllowed(kTestSecondURL, true);
  FinishLoad();

  histogram_tester.ExpectUniqueSample(
      "SubresourceFilter.DocumentLoad.ActivationState",
      static_cast<int>(ActivationState::DRYRUN), 1);
  histogram_tester.ExpectUniqueSample(
      "SubresourceFilter.DocumentLoad.RulesetIsAvailable", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Total", 3, 1);
  histogram_tester.ExpectUniqueSample(
      "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Evaluated", 3, 1);
  histogram_tester.ExpectUniqueSample(
      "SubresourceFilter.DocumentLoad.NumSubresourceLoads.MatchedRules", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Disallowed", 0, 1);
}

}  // namespace subresource_filter
