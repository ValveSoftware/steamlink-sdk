// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engines_pref_names.h"

namespace prefs {

// The GUID of the synced default search provider. Note that this acts like a
// pointer to which synced search engine should be the default, rather than the
// prefs below which describe the locally saved default search provider details
// (and are not synced). This is ignored in the case of the default search
// provider being managed by policy.
const char kSyncedDefaultSearchProviderGUID[] =
    "default_search_provider.synced_guid";

// Whether having a default search provider is enabled.
const char kDefaultSearchProviderEnabled[] =
    "default_search_provider.enabled";

// The URL (as understood by TemplateURLRef) the default search provider uses
// for searches.
const char kDefaultSearchProviderSearchURL[] =
    "default_search_provider.search_url";

// The URL (as understood by TemplateURLRef) the default search provider uses
// for suggestions.
const char kDefaultSearchProviderSuggestURL[] =
    "default_search_provider.suggest_url";

// The URL (as understood by TemplateURLRef) the default search provider uses
// for instant results.
const char kDefaultSearchProviderInstantURL[] =
    "default_search_provider.instant_url";

// The URL (as understood by TemplateURLRef) the default search provider uses
// for image search results.
const char kDefaultSearchProviderImageURL[] =
    "default_search_provider.image_url";

// The URL (as understood by TemplateURLRef) the default search provider uses
// for the new tab page.
const char kDefaultSearchProviderNewTabURL[] =
    "default_search_provider.new_tab_url";

// The string of post parameters (as understood by TemplateURLRef) the default
// search provider uses for searches by using POST.
const char kDefaultSearchProviderSearchURLPostParams[] =
    "default_search_provider.search_url_post_params";

// The string of post parameters (as understood by TemplateURLRef) the default
// search provider uses for suggestions by using POST.
const char kDefaultSearchProviderSuggestURLPostParams[] =
    "default_search_provider.suggest_url_post_params";

// The string of post parameters (as understood by TemplateURLRef) the default
// search provider uses for instant results by using POST.
const char kDefaultSearchProviderInstantURLPostParams[] =
    "default_search_provider.instant_url_post_params";

// The string of post parameters (as understood by TemplateURLRef) the default
// search provider uses for image search results by using POST.
const char kDefaultSearchProviderImageURLPostParams[] =
    "default_search_provider.image_url_post_params";

// The Favicon URL (as understood by TemplateURLRef) of the default search
// provider.
const char kDefaultSearchProviderIconURL[] =
    "default_search_provider.icon_url";

// The input encoding (as understood by TemplateURLRef) supported by the default
// search provider.  The various encodings are separated by ';'
const char kDefaultSearchProviderEncodings[] =
    "default_search_provider.encodings";

// The name of the default search provider.
const char kDefaultSearchProviderName[] = "default_search_provider.name";

// The keyword of the default search provider.
const char kDefaultSearchProviderKeyword[] = "default_search_provider.keyword";

// The id of the default search provider.
const char kDefaultSearchProviderID[] = "default_search_provider.id";

// The prepopulate id of the default search provider.
const char kDefaultSearchProviderPrepopulateID[] =
    "default_search_provider.prepopulate_id";

// The alternate urls of the default search provider.
const char kDefaultSearchProviderAlternateURLs[] =
    "default_search_provider.alternate_urls";

// Search term placement query parameter for the default search provider.
const char kDefaultSearchProviderSearchTermsReplacementKey[] =
    "default_search_provider.search_terms_replacement_key";

// The dictionary key used when the default search providers are given
// in the preferences file. Normally they are copied from the master
// preferences file.
const char kSearchProviderOverrides[] = "search_provider_overrides";
// The format version for the dictionary above.
const char kSearchProviderOverridesVersion[] =
    "search_provider_overrides_version";

// Integer containing the system Country ID the first time we checked the
// template URL prepopulate data.  This is used to avoid adding a whole bunch of
// new search engine choices if prepopulation runs when the user's Country ID
// differs from their previous Country ID.  This pref does not exist until
// prepopulation has been run at least once.
const char kCountryIDAtInstall[] = "countryid_at_install";

}  // namespace prefs
