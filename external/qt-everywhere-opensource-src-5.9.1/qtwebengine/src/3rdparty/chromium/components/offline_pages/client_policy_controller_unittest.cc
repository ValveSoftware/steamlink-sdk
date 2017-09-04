// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/client_policy_controller.h"

#include <algorithm>

#include "base/bind.h"
#include "base/time/time.h"
#include "components/offline_pages/client_namespace_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using LifetimeType = offline_pages::LifetimePolicy::LifetimeType;

namespace offline_pages {

namespace {
const char kUndefinedNamespace[] = "undefined";

bool isTemporary(const OfflinePageClientPolicy& policy) {
  return policy.lifetime_policy.lifetime_type == LifetimeType::TEMPORARY;
}
}  // namespace

class ClientPolicyControllerTest : public testing::Test {
 public:
  ClientPolicyController* controller() { return controller_.get(); }

  // testing::Test
  void SetUp() override;
  void TearDown() override;

 protected:
  void ExpectDownloadSupport(std::string name_space, bool expectation);
  void ExpectRecentTab(std::string name_space, bool expectation);
  void ExpectOnlyOriginalTab(std::string name_space, bool expectation);

 private:
  std::unique_ptr<ClientPolicyController> controller_;
};

void ClientPolicyControllerTest::SetUp() {
  controller_.reset(new ClientPolicyController());
}

void ClientPolicyControllerTest::TearDown() {
  controller_.reset();
}

void ClientPolicyControllerTest::ExpectDownloadSupport(std::string name_space,
                                                       bool expectation) {
  std::vector<std::string> cache =
      controller()->GetNamespacesSupportedByDownload();
  auto result = std::find(cache.begin(), cache.end(), name_space);
  EXPECT_EQ(expectation, result != cache.end());
  EXPECT_EQ(expectation, controller()->IsSupportedByDownload(name_space));
}

void ClientPolicyControllerTest::ExpectRecentTab(std::string name_space,
                                                 bool expectation) {
  std::vector<std::string> cache =
      controller()->GetNamespacesShownAsRecentlyVisitedSite();
  auto result = std::find(cache.begin(), cache.end(), name_space);
  EXPECT_EQ(expectation, result != cache.end());
  EXPECT_EQ(expectation,
            controller()->IsShownAsRecentlyVisitedSite(name_space));
}

void ClientPolicyControllerTest::ExpectOnlyOriginalTab(std::string name_space,
                                                       bool expectation) {
  std::vector<std::string> cache =
      controller()->GetNamespacesRestrictedToOriginalTab();
  auto result = std::find(cache.begin(), cache.end(), name_space);
  EXPECT_EQ(expectation, result != cache.end());
  EXPECT_EQ(expectation, controller()->IsRestrictedToOriginalTab(name_space));
}

TEST_F(ClientPolicyControllerTest, FallbackTest) {
  OfflinePageClientPolicy policy = controller()->GetPolicy(kUndefinedNamespace);
  EXPECT_EQ(policy.name_space, kDefaultNamespace);
  EXPECT_TRUE(isTemporary(policy));
  EXPECT_TRUE(controller()->IsRemovedOnCacheReset(kUndefinedNamespace));
  ExpectDownloadSupport(kUndefinedNamespace, false);
  ExpectRecentTab(kUndefinedNamespace, false);
  ExpectOnlyOriginalTab(kUndefinedNamespace, false);
}

TEST_F(ClientPolicyControllerTest, CheckBookmarkDefined) {
  OfflinePageClientPolicy policy = controller()->GetPolicy(kBookmarkNamespace);
  EXPECT_EQ(policy.name_space, kBookmarkNamespace);
  EXPECT_TRUE(isTemporary(policy));
  EXPECT_TRUE(controller()->IsRemovedOnCacheReset(kBookmarkNamespace));
  ExpectDownloadSupport(kBookmarkNamespace, false);
  ExpectRecentTab(kBookmarkNamespace, false);
  ExpectOnlyOriginalTab(kBookmarkNamespace, false);
}

TEST_F(ClientPolicyControllerTest, CheckLastNDefined) {
  OfflinePageClientPolicy policy = controller()->GetPolicy(kLastNNamespace);
  EXPECT_EQ(policy.name_space, kLastNNamespace);
  EXPECT_TRUE(isTemporary(policy));
  EXPECT_TRUE(controller()->IsRemovedOnCacheReset(kLastNNamespace));
  ExpectDownloadSupport(kLastNNamespace, false);
  ExpectRecentTab(kLastNNamespace, true);
  ExpectOnlyOriginalTab(kLastNNamespace, true);
}

TEST_F(ClientPolicyControllerTest, CheckAsyncDefined) {
  OfflinePageClientPolicy policy = controller()->GetPolicy(kAsyncNamespace);
  EXPECT_EQ(policy.name_space, kAsyncNamespace);
  EXPECT_FALSE(isTemporary(policy));
  EXPECT_FALSE(controller()->IsRemovedOnCacheReset(kAsyncNamespace));
  ExpectDownloadSupport(kAsyncNamespace, true);
  ExpectRecentTab(kAsyncNamespace, false);
  ExpectOnlyOriginalTab(kAsyncNamespace, false);
}

TEST_F(ClientPolicyControllerTest, CheckCCTDefined) {
  OfflinePageClientPolicy policy = controller()->GetPolicy(kCCTNamespace);
  EXPECT_EQ(policy.name_space, kCCTNamespace);
  EXPECT_TRUE(isTemporary(policy));
  EXPECT_TRUE(controller()->IsRemovedOnCacheReset(kCCTNamespace));
  ExpectDownloadSupport(kCCTNamespace, false);
  ExpectRecentTab(kCCTNamespace, false);
  ExpectOnlyOriginalTab(kCCTNamespace, false);
}

TEST_F(ClientPolicyControllerTest, CheckDownloadDefined) {
  OfflinePageClientPolicy policy = controller()->GetPolicy(kDownloadNamespace);
  EXPECT_EQ(policy.name_space, kDownloadNamespace);
  EXPECT_FALSE(isTemporary(policy));
  EXPECT_FALSE(controller()->IsRemovedOnCacheReset(kDownloadNamespace));
  ExpectDownloadSupport(kDownloadNamespace, true);
  ExpectRecentTab(kDownloadNamespace, false);
  ExpectOnlyOriginalTab(kDownloadNamespace, false);
}

TEST_F(ClientPolicyControllerTest, CheckNTPSuggestionsDefined) {
  OfflinePageClientPolicy policy =
      controller()->GetPolicy(kNTPSuggestionsNamespace);
  EXPECT_EQ(policy.name_space, kNTPSuggestionsNamespace);
  EXPECT_FALSE(isTemporary(policy));
  EXPECT_TRUE(controller()->IsRemovedOnCacheReset(kNTPSuggestionsNamespace));
  ExpectDownloadSupport(kNTPSuggestionsNamespace, true);
  ExpectRecentTab(kNTPSuggestionsNamespace, false);
  ExpectOnlyOriginalTab(kNTPSuggestionsNamespace, false);
}

}  // namespace offline_pages
