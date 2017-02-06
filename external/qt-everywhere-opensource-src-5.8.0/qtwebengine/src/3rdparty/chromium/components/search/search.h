// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_SEARCH_H_
#define COMPONENTS_SEARCH_SEARCH_H_

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/string_split.h"

class GURL;
class TemplateURL;

namespace search {

// Returns whether the Instant Extended API is enabled.
bool IsInstantExtendedAPIEnabled();

// Returns the value to pass to the &espv CGI parameter when loading the
// embedded search page from the user's default search provider. Returns 0 if
// the Instant Extended API is not enabled.
uint64_t EmbeddedSearchPageVersion();

// Type for a collection of experiment configuration parameters.
typedef base::StringPairs FieldTrialFlags;

// Finds the active field trial group name and parses out the configuration
// flags. On success, |flags| will be filled with the field trial flags. |flags|
// must not be NULL. Returns true iff the active field trial is successfully
// parsed and not disabled.
// Note that |flags| may be successfully populated in some cases when false is
// returned - in these cases it should not be used.
// Exposed for testing only.
bool GetFieldTrialInfo(FieldTrialFlags* flags);

// Given a FieldTrialFlags object, returns the string value of the provided
// flag.
// Exposed for testing only.
std::string GetStringValueForFlagWithDefault(const std::string& flag,
                                             const std::string& default_value,
                                             const FieldTrialFlags& flags);

// Given a FieldTrialFlags object, returns the uint64_t value of the provided
// flag.
// Exposed for testing only.
uint64_t GetUInt64ValueForFlagWithDefault(const std::string& flag,
                                          uint64_t default_value,
                                          const FieldTrialFlags& flags);

// Given a FieldTrialFlags object, returns the bool value of the provided flag.
// Exposed for testing only.
bool GetBoolValueForFlagWithDefault(const std::string& flag,
                                    bool default_value,
                                    const FieldTrialFlags& flags);

// Returns a string indicating whether InstantExtended is enabled, suitable
// for adding as a query string param to the homepage or search requests.
// Returns an empty string otherwise.
//
// |for_search| should be set to true for search requests, in which case this
// returns a non-empty string only if query extraction is enabled.
std::string InstantExtendedEnabledParam(bool for_search);

// Returns a string that will cause the search results page to update
// incrementally. Currently, Instant Extended passes a different param to
// search results pages that also has this effect, so by default this function
// returns the empty string when Instant Extended is enabled. However, when
// doing instant search result prerendering, we still need to pass this param,
// as Instant Extended does not cause incremental updates by default for the
// prerender page. Callers should set |for_prerender| in this case to force
// the returned string to be non-empty.
std::string ForceInstantResultsParam(bool for_prerender);

// Returns whether query extraction is enabled.
bool IsQueryExtractionEnabled();

// Returns true if 'prefetch_results' flag is set to true in field trials to
// prefetch high-confidence search suggestions.
bool ShouldPrefetchSearchResults();

// Returns true if 'reuse_instant_search_base_page' flag is set to true in field
// trials to reuse the prerendered page to commit any search query.
bool ShouldReuseInstantSearchBasePage();

// Returns true if 'allow_prefetch_non_default_match' flag is enabled in field
// trials to allow prefetching the suggestion marked to be prefetched by the
// suggest server even if it is not the default match.
bool ShouldAllowPrefetchNonDefaultMatch();

// |url| should either have a secure scheme or have a non-HTTPS base URL that
// the user specified using --google-base-url. (This allows testers to use
// --google-base-url to point at non-HTTPS servers, which eases testing.)
bool IsSuitableURLForInstant(const GURL& url, const TemplateURL* template_url);

// -----------------------------------------------------
// The following APIs are exposed for use in tests only.
// -----------------------------------------------------

// Forces query in the omnibox to be on for tests.
void EnableQueryExtractionForTesting();

}  // namespace search

#endif  // COMPONENTS_SEARCH_SEARCH_H_
