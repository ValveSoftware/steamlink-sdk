// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/pref_names.h"

namespace ntp_tiles {
namespace prefs {

// Ordered list of website suggestions shown on the new tab page that will allow
// retaining the order even if the suggestions change over time.
const char kNTPSuggestionsURL[] = "ntp.suggestions_url";

// Whether the suggestion was derived from personal data.
const char kNTPSuggestionsIsPersonal[] = "ntp.suggestions_is_personal";

// If set, overrides the URL for popular sites, including the individual
// overrides for country and version below.
const char kPopularSitesOverrideURL[] = "popular_sites.override_url";

// If set, this will override the country detection for popular sites.
const char kPopularSitesOverrideCountry[] = "popular_sites.override_country";

// If set, this will override the default file version for popular sites.
const char kPopularSitesOverrideVersion[] = "popular_sites.override_version";

}  // namespace prefs
}  // namespace ntp_tiles
