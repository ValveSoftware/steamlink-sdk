// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_METADATA_STORE_H_
#define COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_METADATA_STORE_H_

#include <stdint.h>

#include <vector>

#include "base/callback.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_store_types.h"

class GURL;

namespace offline_pages {

typedef StoreUpdateResult<OfflinePageItem> OfflinePagesUpdateResult;

// OfflinePageMetadataStore keeps metadata for the offline pages.
// Ability to create multiple instances of the store as well as behavior of
// asynchronous operations when the object is being destroyed, before such
// operation finishes will depend on implementation. It should be possible to
// issue multiple asynchronous operations in parallel.
class OfflinePageMetadataStore {
 public:
  // This enum is used in an UMA histogram. Hence the entries here shouldn't
  // be deleted or re-ordered and new ones should be added to the end.
  enum LoadStatus {
    LOAD_SUCCEEDED,
    STORE_INIT_FAILED,
    STORE_LOAD_FAILED,
    DATA_PARSING_FAILED,

    // NOTE: always keep this entry at the end.
    LOAD_STATUS_COUNT
  };

  typedef base::Callback<void(const std::vector<OfflinePageItem>&)>
      LoadCallback;
  typedef base::Callback<void(bool)> InitializeCallback;
  typedef base::Callback<void(ItemActionStatus)> AddCallback;
  typedef base::Callback<void(std::unique_ptr<OfflinePagesUpdateResult>)>
      UpdateCallback;
  typedef base::Callback<void(bool)> ResetCallback;

  OfflinePageMetadataStore();
  virtual ~OfflinePageMetadataStore();

  // Initializes the store. Should be called before any other methods.
  virtual void Initialize(const InitializeCallback& callback) = 0;

  // Get all of the offline pages from the store.
  virtual void GetOfflinePages(const LoadCallback& callback) = 0;

  // Asynchronously adds an offline page item metadata to the store.
  virtual void AddOfflinePage(const OfflinePageItem& offline_page,
                              const AddCallback& callback) = 0;

  // Asynchronously updates a set of offline page items in the store.
  virtual void UpdateOfflinePages(const std::vector<OfflinePageItem>& pages,
                                  const UpdateCallback& callback) = 0;

  // Asynchronously removes offline page metadata from the store.
  // Result of the update is passed in callback.
  virtual void RemoveOfflinePages(const std::vector<int64_t>& offline_ids,
                                  const UpdateCallback& callback) = 0;

  // Resets the store.
  virtual void Reset(const ResetCallback& callback) = 0;

  // Gets the store state.
  virtual StoreState state() const = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_METADATA_STORE_H_
