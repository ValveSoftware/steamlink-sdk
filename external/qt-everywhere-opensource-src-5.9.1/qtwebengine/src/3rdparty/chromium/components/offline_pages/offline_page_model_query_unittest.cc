// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "components/offline_pages/client_namespace_constants.h"
#include "components/offline_pages/client_policy_controller.h"
#include "components/offline_pages/offline_page_client_policy.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_model_query.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

using Requirement = OfflinePageModelQueryBuilder::Requirement;

namespace {

const ClientId kClientId1 = {kDefaultNamespace, "id1"};
const GURL kUrl1 = GURL("https://ktestitem1.com");
const OfflinePageItem kTestItem1(kUrl1, 1, kClientId1, base::FilePath(), 1);

const ClientId kClientId2 = {kDefaultNamespace, "id2"};
const GURL kUrl2 = GURL("https://ktestitem2.com");
const OfflinePageItem kTestItem2(kUrl2, 2, kClientId2, base::FilePath(), 2);

const char kTestNamespace[] = "test_namespace";
}  // namespace

class OfflinePageModelQueryTest : public testing::Test {
 public:
  OfflinePageModelQueryTest();
  ~OfflinePageModelQueryTest() override;

 protected:
  ClientPolicyController policy_;
  OfflinePageModelQueryBuilder builder_;

  const OfflinePageItem expired_page() {
    OfflinePageItem expiredTestItem(GURL("https://ktestitem1.com"), 3,
                                    kClientId1, base::FilePath(), 3);
    expiredTestItem.expiration_time = base::Time::Now();

    return expiredTestItem;
  }

  const OfflinePageItem download_page() {
    return OfflinePageItem(GURL("https://download.com"), 4,
                           {kDownloadNamespace, "id1"}, base::FilePath(), 4);
  }

  const OfflinePageItem original_tab_page() {
    return OfflinePageItem(GURL("https://download.com"), 5,
                           {kLastNNamespace, "id1"}, base::FilePath(), 5);
  }

  const OfflinePageItem test_namespace_page() {
    return OfflinePageItem(GURL("https://download.com"), 6,
                           {kTestNamespace, "id1"}, base::FilePath(), 6);
  }

  const OfflinePageItem recent_page() {
    return OfflinePageItem(GURL("https://download.com"), 7,
                           {kLastNNamespace, "id1"}, base::FilePath(), 7);
  }
};

OfflinePageModelQueryTest::OfflinePageModelQueryTest() {
  policy_.AddPolicyForTest(
      kTestNamespace,
      OfflinePageClientPolicyBuilder(kTestNamespace,
                                     LifetimePolicy::LifetimeType::TEMPORARY,
                                     kUnlimitedPages, kUnlimitedPages)
          .SetIsOnlyShownInOriginalTab(true));
}

OfflinePageModelQueryTest::~OfflinePageModelQueryTest() {}

TEST_F(OfflinePageModelQueryTest, DefaultValues) {
  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  EXPECT_NE(nullptr, query.get());
  EXPECT_FALSE(query->GetAllowExpired());
  EXPECT_EQ(Requirement::UNSET, query->GetRestrictedToOfflineIds().first);
  EXPECT_FALSE(query->GetRestrictedToNamespaces().first);

  EXPECT_TRUE(query->Matches(kTestItem1));
  EXPECT_TRUE(query->Matches(kTestItem2));
  EXPECT_FALSE(query->Matches(expired_page()));
}

TEST_F(OfflinePageModelQueryTest, OfflinePageIdsSet_Exclude) {
  std::vector<int64_t> ids = {1, 4, 9, 16};
  builder_.SetOfflinePageIds(Requirement::EXCLUDE_MATCHING, ids);

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);
  std::pair<Requirement, std::set<int64_t>> offline_id_restriction =
      query->GetRestrictedToOfflineIds();
  EXPECT_EQ(Requirement::EXCLUDE_MATCHING, offline_id_restriction.first);

  ASSERT_EQ(ids.size(), offline_id_restriction.second.size());
  for (auto id : ids) {
    EXPECT_EQ(1U, offline_id_restriction.second.count(id))
        << "Did not find " << id << "in query restrictions.";
  }

  EXPECT_FALSE(query->Matches(kTestItem1));
  EXPECT_TRUE(query->Matches(kTestItem2));
}

TEST_F(OfflinePageModelQueryTest, OfflinePageIdsSet) {
  std::vector<int64_t> ids = {1, 4, 9, 16};
  builder_.SetOfflinePageIds(Requirement::INCLUDE_MATCHING, ids);

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);
  std::pair<Requirement, std::set<int64_t>> offline_id_restriction =
      query->GetRestrictedToOfflineIds();
  EXPECT_EQ(Requirement::INCLUDE_MATCHING, offline_id_restriction.first);

  ASSERT_EQ(ids.size(), offline_id_restriction.second.size());
  for (auto id : ids) {
    EXPECT_EQ(1U, offline_id_restriction.second.count(id))
        << "Did not find " << id << "in query restrictions.";
  }

  EXPECT_TRUE(query->Matches(kTestItem1));
  EXPECT_FALSE(query->Matches(kTestItem2));
}

TEST_F(OfflinePageModelQueryTest, OfflinePageIdsReplace) {
  std::vector<int64_t> ids = {1, 4, 9, 16};
  std::vector<int64_t> ids2 = {1, 2, 3, 4};

  builder_.SetOfflinePageIds(Requirement::INCLUDE_MATCHING, ids);
  builder_.SetOfflinePageIds(Requirement::INCLUDE_MATCHING, ids2);

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);
  std::pair<Requirement, std::set<int64_t>> offline_id_restriction =
      query->GetRestrictedToOfflineIds();
  EXPECT_EQ(Requirement::INCLUDE_MATCHING, offline_id_restriction.first);

  ASSERT_EQ(ids2.size(), offline_id_restriction.second.size());
  for (auto id : ids2) {
    EXPECT_EQ(1U, offline_id_restriction.second.count(id))
        << "Did not find " << id << "in query restrictions.";
  }

  EXPECT_TRUE(query->Matches(kTestItem1));
  EXPECT_TRUE(query->Matches(kTestItem2));
}

TEST_F(OfflinePageModelQueryTest, ClientIdsSet) {
  std::vector<ClientId> ids = {kClientId2, {"invalid", "client id"}};
  builder_.SetClientIds(Requirement::INCLUDE_MATCHING, ids);

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToClientIds();
  const Requirement& requirement = restriction.first;
  const std::set<ClientId>& ids_out = restriction.second;

  EXPECT_EQ(Requirement::INCLUDE_MATCHING, requirement);

  ASSERT_EQ(ids.size(), ids_out.size());
  for (auto id : ids) {
    EXPECT_EQ(1U, ids_out.count(id)) << "Did not find " << id.name_space << "."
                                     << id.id << "in query restrictions.";
  }

  EXPECT_TRUE(query->Matches(kTestItem2));
  EXPECT_FALSE(query->Matches(kTestItem1));
}

TEST_F(OfflinePageModelQueryTest, ClientIdsSet_Exclude) {
  std::vector<ClientId> ids = {kClientId2, {"invalid", "client id"}};
  builder_.SetClientIds(Requirement::EXCLUDE_MATCHING, ids);

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToClientIds();
  const Requirement& requirement = restriction.first;
  const std::set<ClientId>& ids_out = restriction.second;

  EXPECT_EQ(Requirement::EXCLUDE_MATCHING, requirement);

  ASSERT_EQ(ids.size(), ids_out.size());
  for (auto id : ids) {
    EXPECT_EQ(1U, ids_out.count(id)) << "Did not find " << id.name_space << "."
                                     << id.id << "in query restrictions.";
  }

  EXPECT_TRUE(query->Matches(kTestItem1));
  EXPECT_FALSE(query->Matches(kTestItem2));
}

TEST_F(OfflinePageModelQueryTest, ClientIdsReplace) {
  std::vector<ClientId> ids = {kClientId2, {"invalid", "client id"}};
  std::vector<ClientId> ids2 = {kClientId1, {"invalid", "client id"}};

  builder_.SetClientIds(Requirement::INCLUDE_MATCHING, ids);
  builder_.SetClientIds(Requirement::INCLUDE_MATCHING, ids2);

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToClientIds();
  const Requirement& requirement = restriction.first;
  const std::set<ClientId>& ids_out = restriction.second;

  EXPECT_EQ(Requirement::INCLUDE_MATCHING, requirement);

  ASSERT_EQ(ids2.size(), ids_out.size());
  for (auto id : ids2) {
    EXPECT_EQ(1U, ids_out.count(id)) << "Did not find " << id.name_space << "."
                                     << id.id << "in query restrictions.";
  }

  EXPECT_TRUE(query->Matches(kTestItem1));
  EXPECT_FALSE(query->Matches(kTestItem2));
}

TEST_F(OfflinePageModelQueryTest, UrlsSet) {
  std::vector<GURL> urls = {kUrl1, GURL("https://abc.def")};
  builder_.SetUrls(Requirement::INCLUDE_MATCHING, urls);

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToUrls();
  const Requirement& requirement = restriction.first;
  const std::set<GURL>& urls_out = restriction.second;

  EXPECT_EQ(Requirement::INCLUDE_MATCHING, requirement);

  ASSERT_EQ(urls.size(), urls_out.size());
  for (auto url : urls) {
    EXPECT_EQ(1U, urls_out.count(url)) << "Did not find " << url
                                       << "in query restrictions.";
  }

  EXPECT_TRUE(query->Matches(kTestItem1));
  EXPECT_FALSE(query->Matches(kTestItem2));
}

TEST_F(OfflinePageModelQueryTest, UrlsSet_Exclude) {
  std::vector<GURL> urls = {kUrl1, GURL("https://abc.def")};
  builder_.SetUrls(Requirement::EXCLUDE_MATCHING, urls);

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToUrls();
  const Requirement& requirement = restriction.first;
  const std::set<GURL>& urls_out = restriction.second;

  EXPECT_EQ(Requirement::EXCLUDE_MATCHING, requirement);

  ASSERT_EQ(urls.size(), urls_out.size());
  for (auto url : urls) {
    EXPECT_EQ(1U, urls_out.count(url)) << "Did not find " << url
                                       << "in query restrictions.";
  }

  EXPECT_FALSE(query->Matches(kTestItem1));
  EXPECT_TRUE(query->Matches(kTestItem2));
}

TEST_F(OfflinePageModelQueryTest, UrlsReplace) {
  std::vector<GURL> urls = {kUrl1, GURL("https://abc.def")};
  std::vector<GURL> urls2 = {kUrl2, GURL("https://abc.def")};

  builder_.SetUrls(Requirement::INCLUDE_MATCHING, urls);
  builder_.SetUrls(Requirement::INCLUDE_MATCHING, urls2);

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToUrls();
  const Requirement& requirement = restriction.first;
  const std::set<GURL>& urls_out = restriction.second;

  EXPECT_EQ(Requirement::INCLUDE_MATCHING, requirement);

  ASSERT_EQ(urls2.size(), urls_out.size());
  for (auto url : urls2) {
    EXPECT_EQ(1U, urls_out.count(url)) << "Did not find " << url
                                       << "in query restrictions.";
  }

  EXPECT_FALSE(query->Matches(kTestItem1));
  EXPECT_TRUE(query->Matches(kTestItem2));
}

TEST_F(OfflinePageModelQueryTest, AllowExpired) {
  std::unique_ptr<OfflinePageModelQuery> query =
      builder_.AllowExpiredPages(true).Build(&policy_);

  EXPECT_NE(nullptr, query.get());
  EXPECT_TRUE(query->GetAllowExpired());

  EXPECT_TRUE(query->Matches(kTestItem1));
  EXPECT_TRUE(query->Matches(kTestItem2));
  EXPECT_TRUE(query->Matches(expired_page()));
}

TEST_F(OfflinePageModelQueryTest, RequireSupportedByDownload_Only) {
  builder_.RequireSupportedByDownload(Requirement::INCLUDE_MATCHING);
  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToNamespaces();
  std::set<std::string> namespaces_allowed = restriction.second;
  bool restricted_to_namespaces = restriction.first;
  EXPECT_TRUE(restricted_to_namespaces);

  for (const std::string& name_space : namespaces_allowed) {
    EXPECT_TRUE(policy_.IsSupportedByDownload(name_space)) << "Namespace: "
                                                           << name_space;
  }
  EXPECT_FALSE(query->Matches(kTestItem1));
  EXPECT_TRUE(query->Matches(download_page()));
}

TEST_F(OfflinePageModelQueryTest, RequireSupportedByDownload_Except) {
  builder_.RequireSupportedByDownload(Requirement::EXCLUDE_MATCHING);
  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToNamespaces();
  std::set<std::string> namespaces_allowed = restriction.second;
  bool restricted_to_namespaces = restriction.first;
  EXPECT_TRUE(restricted_to_namespaces);

  for (const std::string& name_space : namespaces_allowed) {
    EXPECT_FALSE(policy_.IsSupportedByDownload(name_space)) << "Namespace: "
                                                            << name_space;
  }

  EXPECT_TRUE(query->Matches(kTestItem1));
  EXPECT_FALSE(query->Matches(download_page()));
}

TEST_F(OfflinePageModelQueryTest, RequireShownAsRecentlyVisitedSite_Only) {
  builder_.RequireShownAsRecentlyVisitedSite(Requirement::INCLUDE_MATCHING);
  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToNamespaces();
  std::set<std::string> namespaces_allowed = restriction.second;
  bool restricted_to_namespaces = restriction.first;
  EXPECT_TRUE(restricted_to_namespaces);

  for (const std::string& name_space : namespaces_allowed) {
    EXPECT_TRUE(policy_.IsShownAsRecentlyVisitedSite(name_space))
        << "Namespace: " << name_space;
  }
  EXPECT_FALSE(query->Matches(kTestItem1));
  EXPECT_TRUE(query->Matches(recent_page()));
}

TEST_F(OfflinePageModelQueryTest, RequireShownAsRecentlyVisitedSite_Except) {
  builder_.RequireShownAsRecentlyVisitedSite(Requirement::EXCLUDE_MATCHING);
  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToNamespaces();
  std::set<std::string> namespaces_allowed = restriction.second;
  bool restricted_to_namespaces = restriction.first;
  EXPECT_TRUE(restricted_to_namespaces);

  for (const std::string& name_space : namespaces_allowed) {
    EXPECT_FALSE(policy_.IsShownAsRecentlyVisitedSite(name_space))
        << "Namespace: " << name_space;
  }
  EXPECT_TRUE(query->Matches(kTestItem1));
  EXPECT_FALSE(query->Matches(recent_page()));
}

TEST_F(OfflinePageModelQueryTest, RequireRestrictedToOriginalTab_Only) {
  builder_.RequireRestrictedToOriginalTab(Requirement::INCLUDE_MATCHING);
  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToNamespaces();
  std::set<std::string> namespaces_allowed = restriction.second;
  bool restricted_to_namespaces = restriction.first;
  EXPECT_TRUE(restricted_to_namespaces);

  for (const std::string& name_space : namespaces_allowed) {
    EXPECT_TRUE(policy_.IsRestrictedToOriginalTab(name_space)) << "Namespace: "
                                                               << name_space;
  }
  EXPECT_FALSE(query->Matches(kTestItem1));
  EXPECT_TRUE(query->Matches(original_tab_page()));
}

TEST_F(OfflinePageModelQueryTest, RequireRestrictedToOriginalTab_Except) {
  builder_.RequireRestrictedToOriginalTab(Requirement::EXCLUDE_MATCHING);
  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToNamespaces();
  std::set<std::string> namespaces_allowed = restriction.second;
  bool restricted_to_namespaces = restriction.first;
  EXPECT_TRUE(restricted_to_namespaces);

  for (const std::string& name_space : namespaces_allowed) {
    EXPECT_FALSE(policy_.IsRestrictedToOriginalTab(name_space)) << "Namespace: "
                                                                << name_space;
  }
  EXPECT_TRUE(query->Matches(kTestItem1));
  EXPECT_FALSE(query->Matches(original_tab_page()));
}

TEST_F(OfflinePageModelQueryTest, IntersectNamespaces) {
  // This should exclude last N, but include |kTestNamespace|.
  builder_.RequireRestrictedToOriginalTab(Requirement::INCLUDE_MATCHING)
      .RequireShownAsRecentlyVisitedSite(Requirement::EXCLUDE_MATCHING);
  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToNamespaces();
  std::set<std::string> namespaces_allowed = restriction.second;
  bool restricted_to_namespaces = restriction.first;
  EXPECT_TRUE(restricted_to_namespaces);

  EXPECT_TRUE(namespaces_allowed.count(kTestNamespace) == 1);
  EXPECT_FALSE(query->Matches(recent_page()));
}

}  // namespace offline_pages
