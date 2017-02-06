// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/search.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/google/core/browser/google_util.h"
#include "components/search/search_switches.h"
#include "components/search_engines/template_url.h"
#include "url/gurl.h"

namespace search {

namespace {

// Configuration options for Embedded Search.
// EmbeddedSearch field trials are named in such a way that we can parse out
// the experiment configuration from the trial's group name in order to give
// us maximum flexability in running experiments.
// Field trial groups should be named things like "Group7 espv:2 instant:1".
// The first token is always GroupN for some integer N, followed by a
// space-delimited list of key:value pairs which correspond to these flags:
const char kEmbeddedPageVersionFlagName[] = "espv";

#if defined(OS_IOS)
const uint64_t kEmbeddedPageVersionDefault = 1;
#elif defined(OS_ANDROID)
const uint64_t kEmbeddedPageVersionDefault = 1;
// Use this variant to enable EmbeddedSearch SearchBox API in the results page.
const uint64_t kEmbeddedSearchEnabledVersion = 2;
#else
const uint64_t kEmbeddedPageVersionDefault = 2;
#endif

// Constants for the field trial name and group prefix.
// Note in M30 and below this field trial was named "InstantExtended" and in
// M31 was renamed to EmbeddedSearch for clarity and cleanliness.  Since we
// can't easilly sync up Finch configs with the pushing of this change to
// Dev & Canary, for now the code accepts both names.
// TODO(dcblack): Remove the InstantExtended name once M31 hits the Beta
// channel.
const char kInstantExtendedFieldTrialName[] = "InstantExtended";
const char kEmbeddedSearchFieldTrialName[] = "EmbeddedSearch";

// If the field trial's group name ends with this string its configuration will
// be ignored and Instant Extended will not be enabled by default.
const char kDisablingSuffix[] = "DISABLED";

#if !defined(OS_IOS) && !defined(OS_ANDROID)
const char kEnableQueryExtractionFlagName[] = "query_extraction";
#endif

const char kAllowPrefetchNonDefaultMatch[] = "allow_prefetch_non_default_match";

#if defined(OS_ANDROID)
const char kPrefetchSearchResultsFlagName[] = "prefetch_results";

// Controls whether to reuse prerendered Instant Search base page to commit any
// search query.
const char kReuseInstantSearchBasePage[] = "reuse_instant_search_base_page";
#endif

}  // namespace

bool IsInstantExtendedAPIEnabled() {
#if defined(OS_IOS)
  return false;
#elif defined(OS_ANDROID)
  return EmbeddedSearchPageVersion() == kEmbeddedSearchEnabledVersion;
#else
  return true;
#endif  // defined(OS_IOS)
}

// Determine what embedded search page version to request from the user's
// default search provider. If 0, the embedded search UI should not be enabled.
uint64_t EmbeddedSearchPageVersion() {
#if defined(OS_ANDROID)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableEmbeddedSearchAPI)) {
    return kEmbeddedSearchEnabledVersion;
  }
#endif

  FieldTrialFlags flags;
  if (GetFieldTrialInfo(&flags)) {
    return GetUInt64ValueForFlagWithDefault(kEmbeddedPageVersionFlagName,
                                            kEmbeddedPageVersionDefault,
                                            flags);
  }
  return kEmbeddedPageVersionDefault;
}

bool GetFieldTrialInfo(FieldTrialFlags* flags) {
  // Get the group name.  If the EmbeddedSearch trial doesn't exist, look for
  // the older InstantExtended name.
  std::string group_name = base::FieldTrialList::FindFullName(
      kEmbeddedSearchFieldTrialName);
  if (group_name.empty()) {
    group_name = base::FieldTrialList::FindFullName(
        kInstantExtendedFieldTrialName);
  }

  if (base::EndsWith(group_name, kDisablingSuffix,
                     base::CompareCase::SENSITIVE))
    return false;

  // We have a valid trial that isn't disabled. Extract the flags.
  std::string group_prefix(group_name);
  size_t first_space = group_name.find(" ");
  if (first_space != std::string::npos) {
    // There is a flags section of the group name. Split that out and parse it.
    group_prefix = group_name.substr(0, first_space);
    if (!base::SplitStringIntoKeyValuePairs(group_name.substr(first_space),
                                            ':', ' ', flags)) {
      // Failed to parse the flags section. Assume the whole group name is
      // invalid.
      return false;
    }
  }
  return true;
}

// Given a FieldTrialFlags object, returns the string value of the provided
// flag.
std::string GetStringValueForFlagWithDefault(const std::string& flag,
                                             const std::string& default_value,
                                             const FieldTrialFlags& flags) {
  FieldTrialFlags::const_iterator i;
  for (i = flags.begin(); i != flags.end(); i++) {
    if (i->first == flag)
      return i->second;
  }
  return default_value;
}

// Given a FieldTrialFlags object, returns the uint64_t value of the provided
// flag.
uint64_t GetUInt64ValueForFlagWithDefault(const std::string& flag,
                                          uint64_t default_value,
                                          const FieldTrialFlags& flags) {
  uint64_t value;
  std::string str_value =
      GetStringValueForFlagWithDefault(flag, std::string(), flags);
  if (base::StringToUint64(str_value, &value))
    return value;
  return default_value;
}

// Given a FieldTrialFlags object, returns the boolean value of the provided
// flag.
bool GetBoolValueForFlagWithDefault(const std::string& flag,
                                    bool default_value,
                                    const FieldTrialFlags& flags) {
  return !!GetUInt64ValueForFlagWithDefault(flag, default_value ? 1 : 0, flags);
}

std::string InstantExtendedEnabledParam(bool for_search) {
  if (for_search && !IsQueryExtractionEnabled())
    return std::string();
  return std::string(google_util::kInstantExtendedAPIParam) + "=" +
         base::Uint64ToString(EmbeddedSearchPageVersion()) + "&";
}

std::string ForceInstantResultsParam(bool for_prerender) {
  return (for_prerender || !IsInstantExtendedAPIEnabled()) ? "ion=1&"
                                                           : std::string();
}

bool IsQueryExtractionEnabled() {
#if defined(OS_IOS) || defined(OS_ANDROID)
  return false;
#else
  if (!IsInstantExtendedAPIEnabled())
    return false;

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableQueryExtraction))
    return true;

  FieldTrialFlags flags;
  return GetFieldTrialInfo(&flags) &&
         GetBoolValueForFlagWithDefault(kEnableQueryExtractionFlagName, false,
                                        flags);
#endif  // defined(OS_IOS) || defined(OS_ANDROID)
}

bool ShouldPrefetchSearchResults() {
  if (!IsInstantExtendedAPIEnabled())
    return false;

#if defined(OS_ANDROID)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kPrefetchSearchResults)) {
    return true;
  }

  FieldTrialFlags flags;
  return GetFieldTrialInfo(&flags) &&
         GetBoolValueForFlagWithDefault(kPrefetchSearchResultsFlagName, false,
                                        flags);
#else
  return true;
#endif
}

bool ShouldReuseInstantSearchBasePage() {
  if (!ShouldPrefetchSearchResults())
    return false;

#if defined(OS_ANDROID)
  FieldTrialFlags flags;
  return GetFieldTrialInfo(&flags) &&
         GetBoolValueForFlagWithDefault(kReuseInstantSearchBasePage, false,
                                        flags);
#else
  return true;
#endif
}

bool ShouldAllowPrefetchNonDefaultMatch() {
  if (!ShouldPrefetchSearchResults())
    return false;

  FieldTrialFlags flags;
  return GetFieldTrialInfo(&flags) &&
         GetBoolValueForFlagWithDefault(kAllowPrefetchNonDefaultMatch, false,
                                        flags);
}

// |url| should either have a secure scheme or have a non-HTTPS base URL that
// the user specified using --google-base-url. (This allows testers to use
// --google-base-url to point at non-HTTPS servers, which eases testing.)
bool IsSuitableURLForInstant(const GURL& url, const TemplateURL* template_url) {
  return template_url->HasSearchTermsReplacementKey(url) &&
         (url.SchemeIsCryptographic() ||
          google_util::StartsWithCommandLineGoogleBaseURL(url));
}

void EnableQueryExtractionForTesting() {
#if !defined(OS_IOS) && !defined(OS_ANDROID)
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  cl->AppendSwitch(switches::kEnableQueryExtraction);
#endif
}

}  // namespace search
