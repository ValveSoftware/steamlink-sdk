// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities for the SafeBrowsing DB code.

#ifndef COMPONENTS_SAFE_BROWSING_DB_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_DB_UTIL_H_

#include <stdint.h>

#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "components/safe_browsing_db/v4_protocol_manager_util.h"

class GURL;

namespace safe_browsing {

// Metadata that indicates what kind of URL match this is.
enum class ThreatPatternType {
  NONE = 0,                        // Pattern type didn't appear in the metadata
  MALWARE_LANDING = 1,             // The match is a malware landing page
  MALWARE_DISTRIBUTION = 2,        // The match is a malware distribution page
  SOCIAL_ENGINEERING_ADS = 3,      // The match is a social engineering ads page
  SOCIAL_ENGINEERING_LANDING = 4,  // The match is a social engineering landing
                                   // page
  PHISHING = 5,                    // The match is a phishing page
  THREAT_PATTERN_TYPE_MAX_VALUE
};

// Metadata that was returned by a GetFullHash call. This is the parsed version
// of the PB (from Pver3, or Pver4 local) or JSON (from Pver4 via GMSCore).
// Some fields are only applicable to certain lists.
struct ThreatMetadata {
  ThreatMetadata();
  ThreatMetadata(const ThreatMetadata& other);
  ~ThreatMetadata();

  bool operator==(const ThreatMetadata& other) const;
  bool operator!=(const ThreatMetadata& other) const;

  // Type of blacklisted page. Used on malware and UwS lists.
  // This will be NONE if it wasn't present in the reponse.
  ThreatPatternType threat_pattern_type;

  // Set of permissions blocked. Used with threat_type API_ABUSE.
  // This will be empty if it wasn't present in the response.
  std::set<std::string> api_permissions;

  // Opaque base64 string used for user-population experiments in pver4.
  // This will be empty if it wasn't present in the response.
  std::string population_id;
};

// A truncated hash's type.
typedef uint32_t SBPrefix;

// A full hash.
union SBFullHash {
  char full_hash[32];
  SBPrefix prefix;
};

// Used when we get a gethash response.
struct SBFullHashResult {
  SBFullHash hash;
  // TODO(shess): Refactor to allow ListType here.
  int list_id;
  ThreatMetadata metadata;
  // Used only for V4 results. The cache expire time for this result. The
  // response must not be cached after this time to avoid false positives.
  base::Time cache_expire_after;
};

// Caches individual response from GETHASH request.
struct SBCachedFullHashResult {
  SBCachedFullHashResult();
  explicit SBCachedFullHashResult(const base::Time& in_expire_after);
  SBCachedFullHashResult(const SBCachedFullHashResult& other);
  ~SBCachedFullHashResult();

  base::Time expire_after;
  std::vector<SBFullHashResult> full_hashes;
};

// SafeBrowsing list names.
extern const char kMalwareList[];
extern const char kPhishingList[];
// Binary Download list name.
extern const char kBinUrlList[];
// SafeBrowsing client-side detection whitelist list name.
extern const char kCsdWhiteList[];
// SafeBrowsing download whitelist list name.
extern const char kDownloadWhiteList[];
// SafeBrowsing extension list name.
extern const char kExtensionBlacklist[];
// SafeBrowsing csd malware IP blacklist name.
extern const char kIPBlacklist[];
// SafeBrowsing unwanted URL list.
extern const char kUnwantedUrlList[];
// SafeBrowsing module whitelist list name.
extern const char kModuleWhitelist[];
// Blacklisted resource URLs list name.
extern const char kResourceBlacklist[];
/// This array must contain all Safe Browsing lists.
extern const char* kAllLists[10];

enum ListType {
  INVALID = -1,
  MALWARE = 0,
  PHISH = 1,
  BINURL = 2,
  // Obsolete BINHASH = 3,
  CSDWHITELIST = 4,
  // SafeBrowsing lists are stored in pairs.  Keep ListType 5
  // available for a potential second list that we would store in the
  // csd-whitelist store file.
  DOWNLOADWHITELIST = 6,
  // See above comment. Leave 7 available.
  EXTENSIONBLACKLIST = 8,
  // See above comment. Leave 9 available.
  // Obsolete SIDEEFFECTFREEWHITELIST = 10,
  // See above comment. Leave 11 available.
  IPBLACKLIST = 12,
  // See above comment.  Leave 13 available.
  UNWANTEDURL = 14,
  // See above comment.  Leave 15 available.
  // Obsolete INCLUSIONWHITELIST = 16,
  // See above comment.  Leave 17 available.
  MODULEWHITELIST = 18,
  // See above comment. Leave 19 available.
  RESOURCEBLACKLIST = 20,
  // See above comment.  Leave 21 available.
};

inline bool SBFullHashEqual(const SBFullHash& a, const SBFullHash& b) {
  return !memcmp(a.full_hash, b.full_hash, sizeof(a.full_hash));
}

inline bool SBFullHashLess(const SBFullHash& a, const SBFullHash& b) {
  return memcmp(a.full_hash, b.full_hash, sizeof(a.full_hash)) < 0;
}

// Generate full hash for the given string.
SBFullHash SBFullHashForString(const base::StringPiece& str);
SBFullHash StringToSBFullHash(const std::string& hash_in);
std::string SBFullHashToString(const SBFullHash& hash_out);


// Maps a list name to ListType.
ListType GetListId(const base::StringPiece& name);

// Maps a ListId to list name. Return false if fails.
bool GetListName(ListType list_id, std::string* list);

// Generate the set of full hashes to check for |url|.  If
// |include_whitelist_hashes| is true we will generate additional path-prefixes
// to match against the csd whitelist.  E.g., if the path-prefix /foo is on the
// whitelist it should also match /foo/bar which is not the case for all the
// other lists.  We'll also always add a pattern for the empty path.
void UrlToFullHashes(const GURL& url, bool include_whitelist_hashes,
                     std::vector<SBFullHash>* full_hashes);
}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_UTIL_H_
