// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "components/search/search_switches.h"

namespace switches {

#if defined(OS_ANDROID)

// Enables EmbeddedSearch API in the search results page.
const char kEnableEmbeddedSearchAPI[] = "enable-embeddedsearch-api";

// Triggers prerendering of search base page to prefetch results for the typed
// omnibox query. Only has an effect when prerender is enabled.
const char kPrefetchSearchResults[] = "prefetch-search-results";

#endif

#if !defined(OS_ANDROID) && !defined(OS_IOS)

// Enables query in the omnibox.
const char kEnableQueryExtraction[] = "enable-query-extraction";

#endif

}  // namespace switches
