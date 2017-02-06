// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_METADATA_STORE_H_
#define COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_METADATA_STORE_H_

#include <stdint.h>

#include <vector>

#include "base/callback.h"

class GURL;

namespace offline_pages {

struct OfflinePageItem;

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

  typedef base::Callback<void(LoadStatus, const std::vector<OfflinePageItem>&)>
      LoadCallback;
  typedef base::Callback<void(bool)> UpdateCallback;
  typedef base::Callback<void(bool)> ResetCallback;

  OfflinePageMetadataStore();
  virtual ~OfflinePageMetadataStore();

  // Get all of the offline pages from the store.
  virtual void Load(const LoadCallback& callback) = 0;

  // Asynchronously adds or updates offline page metadata to the store.
  // Result of the update is passed in callback.
  virtual void AddOrUpdateOfflinePage(const OfflinePageItem& offline_page,
                                      const UpdateCallback& callback) = 0;

  // Asynchronously removes offline page metadata from the store.
  // Result of the update is passed in callback.
  virtual void RemoveOfflinePages(const std::vector<int64_t>& offline_ids,
                                  const UpdateCallback& callback) = 0;

  // Resets the store.
  virtual void Reset(const ResetCallback& callback) = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_METADATA_STORE_H_
