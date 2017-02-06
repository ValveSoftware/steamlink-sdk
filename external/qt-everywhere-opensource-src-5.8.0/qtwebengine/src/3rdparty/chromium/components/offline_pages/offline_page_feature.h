// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_FEATURE_H_
#define COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_FEATURE_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace offline_pages {

extern const base::Feature kOfflineBookmarksFeature;
extern const base::Feature kOffliningRecentPagesFeature;
extern const base::Feature kOfflinePagesBackgroundLoadingFeature;
extern const base::Feature kOfflinePagesCTFeature;

// Returns true if offline pages, as result of one or more offline features
// being enabled, is enabled.
bool IsOfflinePagesEnabled();

// Returns true if saving bookmarked pages for offline viewing is enabled.
bool IsOfflineBookmarksEnabled();

// Returns true if offlining of recent pages (aka 'Last N pages') is enabled.
bool IsOffliningRecentPagesEnabled();

// Returns true if saving offline pages in the background is enabled.
bool IsOfflinePagesBackgroundLoadingEnabled();

// Returns true if offline CT features are enabled.  See crbug.com/620421.
bool IsOfflinePagesCTEnabled();

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_FEATURE_H_
