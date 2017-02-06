// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/stub_offline_page_model.h"

namespace offline_pages {

StubOfflinePageModel::StubOfflinePageModel() {}
StubOfflinePageModel::~StubOfflinePageModel() {}

void StubOfflinePageModel::AddObserver(Observer* observer) {}
void StubOfflinePageModel::RemoveObserver(Observer* observer) {}
void StubOfflinePageModel::SavePage(
    const GURL& url,
    const ClientId& client_id,
    std::unique_ptr<OfflinePageArchiver> archiver,
    const SavePageCallback& callback) {}
void StubOfflinePageModel::MarkPageAccessed(int64_t offline_id) {}
void StubOfflinePageModel::ClearAll(const base::Closure& callback) {}
void StubOfflinePageModel::DeletePagesByOfflineId(
    const std::vector<int64_t>& offline_ids,
    const DeletePageCallback& callback) {}
void StubOfflinePageModel::DeletePagesByURLPredicate(
    const UrlPredicate& predicate,
    const DeletePageCallback& callback) {}
void StubOfflinePageModel::HasPages(const std::string& name_space,
                                    const HasPagesCallback& callback) {}
void StubOfflinePageModel::CheckPagesExistOffline(
    const std::set<GURL>& urls,
    const CheckPagesExistOfflineCallback& callback) {}
void StubOfflinePageModel::GetAllPages(
    const MultipleOfflinePageItemCallback& callback) {}
void StubOfflinePageModel::GetOfflineIdsForClientId(
    const ClientId& client_id,
    const MultipleOfflineIdCallback& callback) {}
const std::vector<int64_t> StubOfflinePageModel::MaybeGetOfflineIdsForClientId(
    const ClientId& client_id) const {
  std::vector<int64_t> offline_ids;
  return offline_ids;
}
void StubOfflinePageModel::GetPageByOfflineId(
    int64_t offline_id,
    const SingleOfflinePageItemCallback& callback) {}
const OfflinePageItem* StubOfflinePageModel::MaybeGetPageByOfflineId(
    int64_t offline_id) const {
  return nullptr;
}
void StubOfflinePageModel::GetPageByOfflineURL(
    const GURL& offline_url,
    const SingleOfflinePageItemCallback& callback) {}
const OfflinePageItem* StubOfflinePageModel::MaybeGetPageByOfflineURL(
    const GURL& offline_url) const {
  return nullptr;
}
void StubOfflinePageModel::GetPagesByOnlineURL(
    const GURL& online_url,
    const MultipleOfflinePageItemCallback& callback) {}
void StubOfflinePageModel::GetBestPageForOnlineURL(
    const GURL& online_url,
    const SingleOfflinePageItemCallback callback) {}
const OfflinePageItem* StubOfflinePageModel::MaybeGetBestPageForOnlineURL(
    const GURL& online_url) const {
  return nullptr;
}
void StubOfflinePageModel::CheckMetadataConsistency() {}
void StubOfflinePageModel::ExpirePages(
    const std::vector<int64_t>& offline_ids,
    const base::Time& expiration_time,
    const base::Callback<void(bool)>& callback) {}
ClientPolicyController* StubOfflinePageModel::GetPolicyController() {
  return nullptr;
}
bool StubOfflinePageModel::is_loaded() const {
  return true;
}
OfflineEventLogger* StubOfflinePageModel::GetLogger() {
  return nullptr;
}
}  // namespace offline_pages
