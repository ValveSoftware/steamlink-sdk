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
extern const base::Feature kOfflinePagesSvelteConcurrentLoadingFeature;
extern const base::Feature kOfflinePagesCTFeature;
extern const base::Feature kOfflinePagesSharingFeature;
extern const base::Feature kBackgroundLoaderForDownloadsFeature;
extern const base::Feature kOfflinePagesAsyncDownloadFeature;

// Returns true if saving bookmarked pages for offline viewing is enabled.
bool IsOfflineBookmarksEnabled();

// Returns true if offlining of recent pages (aka 'Last N pages') is enabled.
bool IsOffliningRecentPagesEnabled();

// Returns true if offline CT features are enabled.  See crbug.com/620421.
bool IsOfflinePagesCTEnabled();

// Returns true if offline page sharing is enabled.
bool IsOfflinePagesSharingEnabled();

// Returns true if saving a foreground tab that is taking too long using the
// background scheduler is enabled.
bool IsBackgroundLoaderForDownloadsEnabled();

// Returns true if concurrent background loading is enabled for svelte.
bool IsOfflinePagesSvelteConcurrentLoadingEnabled();

// Returns true if downloading a page asynchonously is enabled.
bool IsOfflinePagesAsyncDownloadEnabled();

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_FEATURE_H_
