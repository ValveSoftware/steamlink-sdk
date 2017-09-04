// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_STUB_OFFLINE_PAGE_MODEL_H_
#define COMPONENTS_OFFLINE_PAGES_STUB_OFFLINE_PAGE_MODEL_H_

#include <set>
#include <string>
#include <vector>

#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_pages/client_policy_controller.h"
#include "components/offline_pages/offline_page_model.h"

namespace offline_pages {

// Stub implementation of OfflinePageModel interface for testing. Besides using
// as a stub for tests, it may also be subclassed to mock specific methods
// needed for a set of tests.
class StubOfflinePageModel : public OfflinePageModel, public KeyedService {
 public:
  StubOfflinePageModel();
  ~StubOfflinePageModel() override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void SavePage(const SavePageParams& save_page_params,
                std::unique_ptr<OfflinePageArchiver> archiver,
                const SavePageCallback& callback) override;
  void MarkPageAccessed(int64_t offline_id) override;
  void DeletePagesByOfflineId(const std::vector<int64_t>& offline_ids,
                              const DeletePageCallback& callback) override;
  void DeletePagesByClientIds(const std::vector<ClientId>& client_ids,
                              const DeletePageCallback& callback) override;
  void GetPagesMatchingQuery(
      std::unique_ptr<OfflinePageModelQuery> query,
      const MultipleOfflinePageItemCallback& callback) override;
  void GetPagesByClientIds(
      const std::vector<ClientId>& client_ids,
      const MultipleOfflinePageItemCallback& callback) override;
  void DeleteCachedPagesByURLPredicate(
      const UrlPredicate& predicate,
      const DeletePageCallback& callback) override;
  void CheckPagesExistOffline(
      const std::set<GURL>& urls,
      const CheckPagesExistOfflineCallback& callback) override;
  void GetAllPages(const MultipleOfflinePageItemCallback& callback) override;
  void GetAllPagesWithExpired(
      const MultipleOfflinePageItemCallback& callback) override;
  void GetOfflineIdsForClientId(
      const ClientId& client_id,
      const MultipleOfflineIdCallback& callback) override;
  void GetPageByOfflineId(
      int64_t offline_id,
      const SingleOfflinePageItemCallback& callback) override;
  void GetPagesByURL(
      const GURL& url,
      URLSearchMode url_search_mode,
      const MultipleOfflinePageItemCallback& callback) override;
  void ExpirePages(const std::vector<int64_t>& offline_ids,
                   const base::Time& expiration_time,
                   const base::Callback<void(bool)>& callback) override;
  ClientPolicyController* GetPolicyController() override;
  bool is_loaded() const override;
  OfflineEventLogger* GetLogger() override;

 private:
  ClientPolicyController policy_controller_;
  std::vector<int64_t> offline_ids_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_STUB_OFFLINE_PAGE_MODEL_H_
