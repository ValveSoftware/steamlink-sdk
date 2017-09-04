// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/client_policy_controller.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "components/offline_pages/client_namespace_constants.h"

using LifetimeType = offline_pages::LifetimePolicy::LifetimeType;

namespace offline_pages {

ClientPolicyController::ClientPolicyController() {
  policies_.clear();
  // Manually defining client policies for bookmark and last_n.
  policies_.insert(std::make_pair(
      kBookmarkNamespace,
      MakePolicy(kBookmarkNamespace, LifetimeType::TEMPORARY,
                 base::TimeDelta::FromDays(7), kUnlimitedPages, 1)));
  policies_.insert(std::make_pair(
      kLastNNamespace,
      OfflinePageClientPolicyBuilder(kLastNNamespace, LifetimeType::TEMPORARY,
                                     kUnlimitedPages, kUnlimitedPages)
          .SetExpirePeriod(base::TimeDelta::FromDays(2))
          .SetIsSupportedByRecentTabs(true)
          .SetIsOnlyShownInOriginalTab(true)
          .Build()));
  policies_.insert(std::make_pair(
      kAsyncNamespace,
      OfflinePageClientPolicyBuilder(kAsyncNamespace, LifetimeType::PERSISTENT,
                                     kUnlimitedPages, kUnlimitedPages)
          .SetIsSupportedByDownload(true)
          .SetIsRemovedOnCacheReset(false)
          .Build()));
  policies_.insert(std::make_pair(
      kCCTNamespace,
      MakePolicy(kCCTNamespace, LifetimeType::TEMPORARY,
                 base::TimeDelta::FromDays(2), kUnlimitedPages, 1)));
  policies_.insert(std::make_pair(
      kDownloadNamespace, OfflinePageClientPolicyBuilder(
                              kDownloadNamespace, LifetimeType::PERSISTENT,
                              kUnlimitedPages, kUnlimitedPages)
                              .SetIsRemovedOnCacheReset(false)
                              .SetIsSupportedByDownload(true)
                              .Build()));
  policies_.insert(std::make_pair(
      kNTPSuggestionsNamespace,
      OfflinePageClientPolicyBuilder(kNTPSuggestionsNamespace,
                                     LifetimeType::PERSISTENT, kUnlimitedPages,
                                     kUnlimitedPages)
          .SetIsSupportedByDownload(true)
          .Build()));

  // Fallback policy.
  policies_.insert(std::make_pair(
      kDefaultNamespace, MakePolicy(kDefaultNamespace, LifetimeType::TEMPORARY,
                                    base::TimeDelta::FromDays(1), 10, 1)));
}

ClientPolicyController::~ClientPolicyController() {}

// static
const OfflinePageClientPolicy ClientPolicyController::MakePolicy(
    const std::string& name_space,
    LifetimeType lifetime_type,
    const base::TimeDelta& expire_period,
    size_t page_limit,
    size_t pages_allowed_per_url) {
  return OfflinePageClientPolicyBuilder(name_space, lifetime_type, page_limit,
                                        pages_allowed_per_url)
      .SetExpirePeriod(expire_period)
      .Build();
}

const OfflinePageClientPolicy& ClientPolicyController::GetPolicy(
    const std::string& name_space) const {
  const auto& iter = policies_.find(name_space);
  if (iter != policies_.end())
    return iter->second;
  // Fallback when the namespace isn't defined.
  return policies_.at(kDefaultNamespace);
}

std::vector<std::string> ClientPolicyController::GetAllNamespaces() const {
  std::vector<std::string> result;
  for (const auto& policy_item : policies_)
    result.emplace_back(policy_item.first);

  return result;
}

bool ClientPolicyController::IsRemovedOnCacheReset(
    const std::string& name_space) const {
  return GetPolicy(name_space).feature_policy.is_removed_on_cache_reset;
}

bool ClientPolicyController::IsSupportedByDownload(
    const std::string& name_space) const {
  return GetPolicy(name_space).feature_policy.is_supported_by_download;
}

const std::vector<std::string>&
ClientPolicyController::GetNamespacesSupportedByDownload() const {
  if (download_namespace_cache_)
    return *download_namespace_cache_;

  download_namespace_cache_ = base::MakeUnique<std::vector<std::string>>();
  for (const auto& policy_item : policies_) {
    if (policy_item.second.feature_policy.is_supported_by_download)
      download_namespace_cache_->emplace_back(policy_item.first);
  }
  return *download_namespace_cache_;
}

bool ClientPolicyController::IsShownAsRecentlyVisitedSite(
    const std::string& name_space) const {
  return GetPolicy(name_space).feature_policy.is_supported_by_recent_tabs;
}

const std::vector<std::string>&
ClientPolicyController::GetNamespacesShownAsRecentlyVisitedSite() const {
  if (recent_tab_namespace_cache_)
    return *recent_tab_namespace_cache_;

  recent_tab_namespace_cache_ = base::MakeUnique<std::vector<std::string>>();
  for (const auto& policy_item : policies_) {
    if (policy_item.second.feature_policy.is_supported_by_recent_tabs)
      recent_tab_namespace_cache_->emplace_back(policy_item.first);
  }

  return *recent_tab_namespace_cache_;
}

bool ClientPolicyController::IsRestrictedToOriginalTab(
    const std::string& name_space) const {
  return GetPolicy(name_space).feature_policy.only_shown_in_original_tab;
}

const std::vector<std::string>&
ClientPolicyController::GetNamespacesRestrictedToOriginalTab() const {
  if (show_in_original_tab_cache_)
    return *show_in_original_tab_cache_;

  show_in_original_tab_cache_ = base::MakeUnique<std::vector<std::string>>();
  for (const auto& policy_item : policies_) {
    if (policy_item.second.feature_policy.only_shown_in_original_tab)
      show_in_original_tab_cache_->emplace_back(policy_item.first);
  }

  return *show_in_original_tab_cache_;
}

void ClientPolicyController::AddPolicyForTest(
    const std::string& name_space,
    const OfflinePageClientPolicyBuilder& builder) {
  policies_.insert(std::make_pair(name_space, builder.Build()));
}

}  // namespace offline_pages
