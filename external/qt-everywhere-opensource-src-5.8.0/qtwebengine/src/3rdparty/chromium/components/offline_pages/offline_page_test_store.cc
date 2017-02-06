// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_test_store.h"

#include "base/bind.h"
#include "base/location.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

OfflinePageTestStore::OfflinePageTestStore(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : task_runner_(task_runner), scenario_(TestScenario::SUCCESSFUL) {}

OfflinePageTestStore::OfflinePageTestStore(
    const OfflinePageTestStore& other_store)
    : task_runner_(other_store.task_runner_),
      scenario_(other_store.scenario_),
      offline_pages_(other_store.offline_pages_) {}

OfflinePageTestStore::~OfflinePageTestStore() {}

void OfflinePageTestStore::Load(const LoadCallback& callback) {
  OfflinePageMetadataStore::LoadStatus load_status;
  if (scenario_ == TestScenario::LOAD_FAILED) {
    load_status = OfflinePageMetadataStore::STORE_LOAD_FAILED;
    offline_pages_.clear();
  } else {
    load_status = OfflinePageMetadataStore::LOAD_SUCCEEDED;
  }
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(callback, load_status, GetAllPages()));
}

void OfflinePageTestStore::AddOrUpdateOfflinePage(
    const OfflinePageItem& offline_page,
    const UpdateCallback& callback) {
  last_saved_page_ = offline_page;
  bool result = scenario_ != TestScenario::WRITE_FAILED;
  if (result)
    offline_pages_[offline_page.offline_id] = offline_page;
  if (!callback.is_null())
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, result));
}

void OfflinePageTestStore::RemoveOfflinePages(
    const std::vector<int64_t>& offline_ids,
    const UpdateCallback& callback) {
  ASSERT_FALSE(offline_ids.empty());
  bool result = false;
  if (scenario_ != TestScenario::REMOVE_FAILED) {
    for (const auto& offline_id : offline_ids) {
      auto iter = offline_pages_.find(offline_id);
      if (iter != offline_pages_.end()) {
        offline_pages_.erase(iter);
        result = true;
      }
    }
  }

  task_runner_->PostTask(FROM_HERE, base::Bind(callback, result));
}

void OfflinePageTestStore::Reset(const ResetCallback& callback) {
  offline_pages_.clear();
  task_runner_->PostTask(FROM_HERE, base::Bind(callback, true));
}

void OfflinePageTestStore::UpdateLastAccessTime(
    int64_t offline_id,
    const base::Time& last_access_time) {
  auto iter = offline_pages_.find(offline_id);
  if (iter == offline_pages_.end())
    return;
  iter->second.last_access_time = last_access_time;
}

std::vector<OfflinePageItem> OfflinePageTestStore::GetAllPages() const {
  std::vector<OfflinePageItem> offline_pages;
  for (const auto& id_page_pair : offline_pages_)
    offline_pages.push_back(id_page_pair.second);
  return offline_pages;
}

void OfflinePageTestStore::ClearAllPages() {
  offline_pages_.clear();
}

}  // namespace offline_pages
