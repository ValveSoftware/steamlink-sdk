// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_black_list.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/time/clock.h"
#include "components/previews/core/previews_black_list_item.h"
#include "components/previews/core/previews_experiments.h"
#include "url/gurl.h"

namespace previews {

namespace {

void EvictOldestOptOut(BlackListItemMap* black_list_item_map) {
  // TODO(ryansturm): Add UMA. crbug.com/647717
  base::Optional<base::Time> oldest_opt_out;
  BlackListItemMap::iterator item_to_delete = black_list_item_map->end();
  for (BlackListItemMap::iterator iter = black_list_item_map->begin();
       iter != black_list_item_map->end(); ++iter) {
    if (!iter->second->most_recent_opt_out_time()) {
      // If there is no opt out time, this is a good choice to evict.
      item_to_delete = iter;
      break;
    }
    if (!oldest_opt_out ||
        iter->second->most_recent_opt_out_time().value() <
            oldest_opt_out.value()) {
      oldest_opt_out = iter->second->most_recent_opt_out_time().value();
      item_to_delete = iter;
    }
  }
  black_list_item_map->erase(item_to_delete);
}

// Returns the PreviewsBlackListItem representing |host_name| in
// |black_list_item_map|. If there is no item for |host_name|, returns null.
PreviewsBlackListItem* GetBlackListItemFromMap(
    const BlackListItemMap& black_list_item_map,
    const std::string& host_name) {
  BlackListItemMap::const_iterator iter = black_list_item_map.find(host_name);
  if (iter != black_list_item_map.end())
    return iter->second.get();
  return nullptr;
}

}  // namespace

PreviewsBlackList::PreviewsBlackList(
    std::unique_ptr<PreviewsOptOutStore> opt_out_store,
    std::unique_ptr<base::Clock> clock)
    : loaded_(false),
      opt_out_store_(std::move(opt_out_store)),
      clock_(std::move(clock)),
      weak_factory_(this) {
  if (opt_out_store_) {
    opt_out_store_->LoadBlackList(base::Bind(
        &PreviewsBlackList::LoadBlackListDone, weak_factory_.GetWeakPtr()));
  } else {
    LoadBlackListDone(base::MakeUnique<BlackListItemMap>(),
                      CreateHostIndifferentBlackListItem());
  }
}

PreviewsBlackList::~PreviewsBlackList() {}

void PreviewsBlackList::AddPreviewNavigation(const GURL& url,
                                             bool opt_out,
                                             PreviewsType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(url.has_host());
  switch (type) {
    case PreviewsType::OFFLINE:
      UMA_HISTOGRAM_BOOLEAN("Previews.OptOut.UserOptedOut.Offline", opt_out);
      break;
    default:
      NOTREACHED();
  }
  if (opt_out) {
    last_opt_out_time_ = clock_->Now();
  }
  // If the |black_list_item_map_| has been loaded from |opt_out_store_|,
  // synchronous operations will be accurate. Otherwise, queue the task to run
  // asynchronously.
  if (loaded_) {
    AddPreviewNavigationSync(url, opt_out, type);
  } else {
    QueuePendingTask(base::Bind(&PreviewsBlackList::AddPreviewNavigationSync,
                                base::Unretained(this), url, opt_out, type));
  }
}

void PreviewsBlackList::AddPreviewNavigationSync(const GURL& url,
                                                 bool opt_out,
                                                 PreviewsType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(url.has_host());
  DCHECK(loaded_);
  DCHECK(host_indifferent_black_list_item_);
  DCHECK(black_list_item_map_);
  std::string host_name = url.host();
  base::Time now = clock_->Now();
  PreviewsBlackListItem* item =
      GetOrCreateBlackListItemForMap(black_list_item_map_.get(), host_name);
  item->AddPreviewNavigation(opt_out, now);
  DCHECK_LE(black_list_item_map_->size(),
            params::MaxInMemoryHostsInBlackList());
  host_indifferent_black_list_item_->AddPreviewNavigation(opt_out, now);
  if (!opt_out_store_)
    return;
  opt_out_store_->AddPreviewNavigation(opt_out, host_name, type, now);
}

PreviewsEligibilityReason PreviewsBlackList::IsLoadedAndAllowed(
    const GURL& url,
    PreviewsType type) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(url.has_host());
  if (!loaded_)
    return PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED;
  DCHECK(black_list_item_map_);
  if (last_opt_out_time_ &&
      clock_->Now() <
          last_opt_out_time_.value() + params::SingleOptOutDuration()) {
    return PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT;
  }
  if (host_indifferent_black_list_item_->IsBlackListed(clock_->Now()))
    return PreviewsEligibilityReason::USER_BLACKLISTED;
  PreviewsBlackListItem* black_list_item =
      GetBlackListItemFromMap(*black_list_item_map_, url.host());
  if (black_list_item && black_list_item->IsBlackListed(clock_->Now()))
    return PreviewsEligibilityReason::HOST_BLACKLISTED;
  return PreviewsEligibilityReason::ALLOWED;
}

void PreviewsBlackList::ClearBlackList(base::Time begin_time,
                                       base::Time end_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_LE(begin_time, end_time);
  // If the |black_list_item_map_| has been loaded from |opt_out_store_|,
  // synchronous operations will be accurate. Otherwise, queue the task to run
  // asynchronously.
  if (loaded_) {
    ClearBlackListSync(begin_time, end_time);
  } else {
    QueuePendingTask(base::Bind(&PreviewsBlackList::ClearBlackListSync,
                                base::Unretained(this), begin_time, end_time));
  }
}

void PreviewsBlackList::ClearBlackListSync(base::Time begin_time,
                                           base::Time end_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(loaded_);
  DCHECK_LE(begin_time, end_time);
  black_list_item_map_.reset();
  host_indifferent_black_list_item_.reset();
  loaded_ = false;
  // Delete relevant entries and reload the blacklist into memory.
  if (opt_out_store_) {
    opt_out_store_->ClearBlackList(begin_time, end_time);
    opt_out_store_->LoadBlackList(base::Bind(
        &PreviewsBlackList::LoadBlackListDone, weak_factory_.GetWeakPtr()));
  } else {
    LoadBlackListDone(base::MakeUnique<BlackListItemMap>(),
                      CreateHostIndifferentBlackListItem());
  }
}

void PreviewsBlackList::QueuePendingTask(base::Closure callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!loaded_);
  DCHECK(!callback.is_null());
  pending_callbacks_.emplace(callback);
}

void PreviewsBlackList::LoadBlackListDone(
    std::unique_ptr<BlackListItemMap> black_list_item_map,
    std::unique_ptr<PreviewsBlackListItem> host_indifferent_black_list_item) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(black_list_item_map);
  DCHECK(host_indifferent_black_list_item);
  DCHECK_LE(black_list_item_map->size(), params::MaxInMemoryHostsInBlackList());
  loaded_ = true;
  black_list_item_map_ = std::move(black_list_item_map);
  host_indifferent_black_list_item_ =
      std::move(host_indifferent_black_list_item);

  // Run all pending tasks. |loaded_| may change if ClearBlackList is queued.
  while (pending_callbacks_.size() > 0 && loaded_) {
    pending_callbacks_.front().Run();
    pending_callbacks_.pop();
  }
}

// static
PreviewsBlackListItem* PreviewsBlackList::GetOrCreateBlackListItemForMap(
    BlackListItemMap* black_list_item_map,
    const std::string& host_name) {
  PreviewsBlackListItem* black_list_item =
      GetBlackListItemFromMap(*black_list_item_map, host_name);
  if (black_list_item)
    return black_list_item;
  if (black_list_item_map->size() >= params::MaxInMemoryHostsInBlackList())
    EvictOldestOptOut(black_list_item_map);
  DCHECK_LT(black_list_item_map->size(), params::MaxInMemoryHostsInBlackList());
  black_list_item = new PreviewsBlackListItem(
      params::MaxStoredHistoryLengthForPerHostBlackList(),
      params::PerHostBlackListOptOutThreshold(),
      params::PerHostBlackListDuration());
  black_list_item_map->operator[](host_name) =
      base::WrapUnique(black_list_item);
  return black_list_item;
}

// static
std::unique_ptr<PreviewsBlackListItem>
PreviewsBlackList::CreateHostIndifferentBlackListItem() {
  return base::MakeUnique<PreviewsBlackListItem>(
      params::MaxStoredHistoryLengthForHostIndifferentBlackList(),
      params::HostIndifferentBlackListOptOutThreshold(),
      params::HostIndifferentBlackListPerHostDuration());
}

}  // namespace previews
