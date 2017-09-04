// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/downloads/download_ui_adapter.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/client_namespace_constants.h"
#include "components/offline_pages/client_policy_controller.h"
#include "components/offline_pages/downloads/download_ui_item.h"
#include "components/offline_pages/offline_page_model.h"

namespace offline_pages {

namespace {
const char kDownloadUIAdapterKey[] = "download-ui-adapter";
}

DownloadUIAdapter::ItemInfo::ItemInfo(const OfflinePageItem& page)
    : ui_item(base::MakeUnique<DownloadUIItem>(page)),
      offline_id(page.offline_id) {}

DownloadUIAdapter::ItemInfo::~ItemInfo() {}

DownloadUIAdapter::DownloadUIAdapter(OfflinePageModel* model)
    : model_(model),
      state_(State::NOT_LOADED),
      observers_count_(0),
      weak_ptr_factory_(this) {
}

DownloadUIAdapter::~DownloadUIAdapter() { }

// static
DownloadUIAdapter* DownloadUIAdapter::FromOfflinePageModel(
    OfflinePageModel* offline_page_model) {
  DownloadUIAdapter* adapter = static_cast<DownloadUIAdapter*>(
      offline_page_model->GetUserData(kDownloadUIAdapterKey));
  if (!adapter) {
    adapter = new DownloadUIAdapter(offline_page_model);
    offline_page_model->SetUserData(kDownloadUIAdapterKey, adapter);
  }
  return adapter;
}

void DownloadUIAdapter::AddObserver(Observer* observer) {
  DCHECK(observer);
  if (observers_.HasObserver(observer))
    return;
  if (observers_count_ == 0)
    LoadCache();
  observers_.AddObserver(observer);
  ++observers_count_;
  // If the items are already loaded, post the notification right away.
  // Don't just invoke it from here to avoid reentrancy in the client.
  if (state_ == State::LOADED) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&DownloadUIAdapter::NotifyItemsLoaded,
                              weak_ptr_factory_.GetWeakPtr(),
                              base::Unretained(observer)));
  }
}

void DownloadUIAdapter::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  if (!observers_.HasObserver(observer))
    return;
  observers_.RemoveObserver(observer);
  --observers_count_;
  // Once the last observer is gone, clear cached data.
  if (observers_count_ == 0)
    ClearCache();
}

void DownloadUIAdapter::OfflinePageModelLoaded(OfflinePageModel* model) {
  // This signal is not used here.
}

void DownloadUIAdapter::OfflinePageModelChanged(OfflinePageModel* model) {
  DCHECK(model == model_);
  model_->GetAllPages(
      base::Bind(&DownloadUIAdapter::OnOfflinePagesChanged,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DownloadUIAdapter::OfflinePageDeleted(
  int64_t offline_id, const ClientId& client_id) {
  if (!IsVisibleInUI(client_id))
    return;
  std::string guid = client_id.id;
  DownloadUIItems::const_iterator it = items_.find(guid);
  if (it == items_.end())
    return;
  items_.erase(it);
  for (Observer& observer : observers_)
    observer.ItemDeleted(guid);
}

std::vector<const DownloadUIItem*> DownloadUIAdapter::GetAllItems() const {
  std::vector<const DownloadUIItem*> result;
  for (const auto& item : items_)
    result.push_back(item.second->ui_item.get());
  return result;
}

const DownloadUIItem*
    DownloadUIAdapter::GetItem(const std::string& guid) const {
  DownloadUIItems::const_iterator it = items_.find(guid);
  if (it == items_.end())
    return nullptr;
  return it->second->ui_item.get();
}

void DownloadUIAdapter::DeleteItem(const std::string& guid) {
  // TODO(dimich): Also remove pending request from RequestQueue.
  DownloadUIItems::const_iterator it = items_.find(guid);
  if (it == items_.end())
    return;

  std::vector<int64_t> page_ids;
  page_ids.push_back(it->second->offline_id);
  // TODO(dimich): This should be ExpirePages(...Now()..) when Expire is
  // firing Observer method. The resulting Observer notification will update
  // local cache.
  model_->DeletePagesByOfflineId(
      page_ids, base::Bind(&DownloadUIAdapter::OnDeletePagesDone,
                           weak_ptr_factory_.GetWeakPtr()));
}

int64_t DownloadUIAdapter::GetOfflineIdByGuid(
    const std::string& guid) const {
  // TODO(dimich): when requests are also in the cache, filter them out.
  // Requests do not yet have offline ID.
  DownloadUIItems::const_iterator it = items_.find(guid);
  if (it != items_.end())
    return it->second->offline_id;
  return 0;
}

// Note that several LoadCache calls may be issued before the async GetAllPages
// comes back.
void DownloadUIAdapter::LoadCache() {
  // TODO(dimich): Add fetching from RequestQueue as well.
  state_ = State::LOADING;
  model_->GetAllPages(
      base::Bind(&DownloadUIAdapter::OnOfflinePagesLoaded,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DownloadUIAdapter::ClearCache() {
  // Once loaded, this class starts to observe the model. Only remove observer
  // if it was added.
  if (state_ == State::LOADED)
    model_->RemoveObserver(this);
  items_.clear();
  state_ = State::NOT_LOADED;
}

void DownloadUIAdapter::OnOfflinePagesLoaded(
    const MultipleOfflinePageItemResult& pages) {
  // If multiple observers register quickly, the cache might be already loaded
  // by the previous LoadCache call. At the same time, if all observers already
  // left, there is no reason to populate the cache.
  if (state_ != State::LOADING)
    return;
  for (const auto& page : pages) {
    if (IsVisibleInUI(page.client_id)) {
      std::string guid = page.client_id.id;
      DCHECK(items_.find(guid) == items_.end());
      items_[guid] = base::MakeUnique<ItemInfo>(page);
    }
  }
  model_->AddObserver(this);
  state_ = State::LOADED;
  for (Observer& observer : observers_)
    observer.ItemsLoaded();
}

void DownloadUIAdapter::NotifyItemsLoaded(Observer* observer) {
  if (observer && observers_.HasObserver(observer))
    observer->ItemsLoaded();
}

// This method is only called by OPM when a single item added.
// TODO(dimich): change OPM to have real OnPageAdded/OnPageUpdated and
// simplify this code.
void DownloadUIAdapter::OnOfflinePagesChanged(
    const MultipleOfflinePageItemResult& pages) {
  std::vector<std::string> added_guids;
  for (const auto& page : pages) {
    if (!IsVisibleInUI(page.client_id))  // Item should be filtered out.
      continue;
    const std::string& guid = page.client_id.id;
    if (items_.find(guid) != items_.end())  // Item already exists.
      continue;
    items_[guid] = base::MakeUnique<ItemInfo>(page);
    added_guids.push_back(guid);
  }
  for (auto& guid : added_guids) {
    const DownloadUIItem& item = *(items_.find(guid)->second->ui_item.get());
    for (Observer& observer : observers_)
      observer.ItemAdded(item);
  }
}

void DownloadUIAdapter::OnDeletePagesDone(DeletePageResult result) {
  // TODO(dimich): Consider adding UMA to record user actions.
}

bool DownloadUIAdapter::IsVisibleInUI(const ClientId& client_id) {
  const std::string& name_space = client_id.name_space;
  return model_->GetPolicyController()->IsSupportedByDownload(name_space) &&
         base::IsValidGUID(client_id.id);
}

}  // namespace offline_pages
