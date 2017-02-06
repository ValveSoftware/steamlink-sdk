// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_STUB_OFFLINE_PAGE_MODEL_H_
#define COMPONENTS_OFFLINE_PAGES_STUB_OFFLINE_PAGE_MODEL_H_

#include <set>
#include <string>
#include <vector>

#include "components/offline_pages/offline_page_model.h"

namespace offline_pages {

// Stub implementation of OfflinePageModel interface for testing. Besides using
// as a stub for tests, it may also be subclassed to mock specific methods
// needed for a set of tests.
class StubOfflinePageModel : public OfflinePageModel {
 public:
  StubOfflinePageModel();
  ~StubOfflinePageModel() override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void SavePage(const GURL& url,
                const ClientId& client_id,
                std::unique_ptr<OfflinePageArchiver> archiver,
                const SavePageCallback& callback) override;
  void MarkPageAccessed(int64_t offline_id) override;
  void ClearAll(const base::Closure& callback) override;
  void DeletePagesByOfflineId(const std::vector<int64_t>& offline_ids,
                              const DeletePageCallback& callback) override;
  void DeletePagesByURLPredicate(const UrlPredicate& predicate,
                                 const DeletePageCallback& callback) override;
  void HasPages(const std::string& name_space,
                const HasPagesCallback& callback) override;
  void CheckPagesExistOffline(
      const std::set<GURL>& urls,
      const CheckPagesExistOfflineCallback& callback) override;
  void GetAllPages(const MultipleOfflinePageItemCallback& callback) override;
  void GetOfflineIdsForClientId(
      const ClientId& client_id,
      const MultipleOfflineIdCallback& callback) override;
  const std::vector<int64_t> MaybeGetOfflineIdsForClientId(
      const ClientId& client_id) const override;
  void GetPageByOfflineId(
      int64_t offline_id,
      const SingleOfflinePageItemCallback& callback) override;
  const OfflinePageItem* MaybeGetPageByOfflineId(
      int64_t offline_id) const override;
  void GetPageByOfflineURL(
      const GURL& offline_url,
      const SingleOfflinePageItemCallback& callback) override;
  const OfflinePageItem* MaybeGetPageByOfflineURL(
      const GURL& offline_url) const override;
  void GetPagesByOnlineURL(
      const GURL& online_url,
      const MultipleOfflinePageItemCallback& callback) override;
  void GetBestPageForOnlineURL(
      const GURL& online_url,
      const SingleOfflinePageItemCallback callback) override;
  const OfflinePageItem* MaybeGetBestPageForOnlineURL(
      const GURL& online_url) const override;
  void CheckMetadataConsistency() override;
  void ExpirePages(const std::vector<int64_t>& offline_ids,
                   const base::Time& expiration_time,
                   const base::Callback<void(bool)>& callback) override;
  ClientPolicyController* GetPolicyController() override;
  bool is_loaded() const override;
  OfflineEventLogger* GetLogger() override;

 private:
  std::vector<int64_t> offline_ids_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_STUB_OFFLINE_PAGE_MODEL_H_
