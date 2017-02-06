// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_TYPES_H_
#define COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_TYPES_H_

#include <stdint.h>

#include <set>
#include <vector>

#include "base/callback.h"
#include "components/offline_pages/offline_page_item.h"

class GURL;

// This file contains common callbacks used by OfflinePageModel and is a
// temporary step to refactor and interface of the model out of the
// implementation.
namespace offline_pages {
// Result of saving a page offline.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.offlinepages
enum class SavePageResult {
  SUCCESS,
  CANCELLED,
  DEVICE_FULL,
  CONTENT_UNAVAILABLE,
  ARCHIVE_CREATION_FAILED,
  STORE_FAILURE,
  ALREADY_EXISTS,
  // Certain pages, i.e. file URL or NTP, will not be saved because these
  // are already locally accessible.
  SKIPPED,
  SECURITY_CERTIFICATE_ERROR,
  // NOTE: always keep this entry at the end. Add new result types only
  // immediately above this line. Make sure to update the corresponding
  // histogram enum accordingly.
  RESULT_COUNT,
};

// Result of deleting an offline page.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.offlinepages
enum class DeletePageResult {
  SUCCESS,
  CANCELLED,
  STORE_FAILURE,
  DEVICE_FAILURE,
  // Deprecated. Deleting pages which are not in metadata store would be
  // returing |SUCCESS|. Should not be used anymore.
  NOT_FOUND,
  // NOTE: always keep this entry at the end. Add new result types only
  // immediately above this line. Make sure to update the corresponding
  // histogram enum accordingly.
  RESULT_COUNT,
};

// Result of loading all pages.
enum class LoadResult {
  SUCCESS,
  CANCELLED,
  STORE_FAILURE,
};

typedef std::set<GURL> CheckPagesExistOfflineResult;
typedef std::vector<int64_t> MultipleOfflineIdResult;
typedef std::vector<OfflinePageItem> MultipleOfflinePageItemResult;

typedef base::Callback<void(SavePageResult, int64_t)> SavePageCallback;
typedef base::Callback<void(DeletePageResult)> DeletePageCallback;
typedef base::Callback<void(const CheckPagesExistOfflineResult&)>
    CheckPagesExistOfflineCallback;
typedef base::Callback<void(bool)> HasPagesCallback;
typedef base::Callback<void(const MultipleOfflineIdResult&)>
    MultipleOfflineIdCallback;
typedef base::Callback<void(const OfflinePageItem*)>
    SingleOfflinePageItemCallback;
typedef base::Callback<void(const MultipleOfflinePageItemResult&)>
    MultipleOfflinePageItemCallback;
typedef base::Callback<bool(const GURL&)> UrlPredicate;
}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_TYPES_H_
