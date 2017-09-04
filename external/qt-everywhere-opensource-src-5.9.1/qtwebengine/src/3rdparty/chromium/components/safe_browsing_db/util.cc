// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/util.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "crypto/sha2.h"
#include "net/base/escape.h"
#include "url/gurl.h"

namespace safe_browsing {

// Utility functions -----------------------------------------------------------

namespace {

bool IsKnownList(const std::string& name) {
  for (size_t i = 0; i < arraysize(kAllLists); ++i) {
    if (!strcmp(kAllLists[i], name.c_str())) {
      return true;
    }
  }
  return false;
}

}  // namespace

// ThreatMetadata ------------------------------------------------------------
ThreatMetadata::ThreatMetadata()
    : threat_pattern_type(ThreatPatternType::NONE) {}

ThreatMetadata::ThreatMetadata(const ThreatMetadata& other) = default;

ThreatMetadata::~ThreatMetadata() {}

bool ThreatMetadata::operator==(const ThreatMetadata& other) const {
  return threat_pattern_type == other.threat_pattern_type &&
         api_permissions == other.api_permissions &&
         population_id == other.population_id;
}

bool ThreatMetadata::operator!=(const ThreatMetadata& other) const {
  return !operator==(other);
}

// SBCachedFullHashResult ------------------------------------------------------

SBCachedFullHashResult::SBCachedFullHashResult() {}

SBCachedFullHashResult::SBCachedFullHashResult(
    const base::Time& in_expire_after)
    : expire_after(in_expire_after) {}

SBCachedFullHashResult::SBCachedFullHashResult(
    const SBCachedFullHashResult& other) = default;

SBCachedFullHashResult::~SBCachedFullHashResult() {}

// Listnames that browser can process.
const char kMalwareList[] = "goog-malware-shavar";
const char kPhishingList[] = "goog-phish-shavar";
const char kBinUrlList[] = "goog-badbinurl-shavar";
const char kCsdWhiteList[] = "goog-csdwhite-sha256";
const char kDownloadWhiteList[] = "goog-downloadwhite-digest256";
const char kExtensionBlacklist[] = "goog-badcrxids-digestvar";
const char kIPBlacklist[] = "goog-badip-digest256";
const char kUnwantedUrlList[] = "goog-unwanted-shavar";
const char kModuleWhitelist[] = "goog-whitemodule-digest256";
const char kResourceBlacklist[] = "goog-badresource-shavar";

const char* kAllLists[10] = {
    kMalwareList,        kPhishingList,       kBinUrlList,  kCsdWhiteList,
    kDownloadWhiteList,  kExtensionBlacklist, kIPBlacklist, kUnwantedUrlList,
    kModuleWhitelist,    kResourceBlacklist,
};

ListType GetListId(const base::StringPiece& name) {
  ListType id;
  if (name == kMalwareList) {
    id = MALWARE;
  } else if (name == kPhishingList) {
    id = PHISH;
  } else if (name == kBinUrlList) {
    id = BINURL;
  } else if (name == kCsdWhiteList) {
    id = CSDWHITELIST;
  } else if (name == kDownloadWhiteList) {
    id = DOWNLOADWHITELIST;
  } else if (name == kExtensionBlacklist) {
    id = EXTENSIONBLACKLIST;
  } else if (name == kIPBlacklist) {
    id = IPBLACKLIST;
  } else if (name == kUnwantedUrlList) {
    id = UNWANTEDURL;
  } else if (name == kModuleWhitelist) {
    id = MODULEWHITELIST;
  } else if (name == kResourceBlacklist) {
    id = RESOURCEBLACKLIST;
  } else {
    id = INVALID;
  }
  return id;
}

bool GetListName(ListType list_id, std::string* list) {
  switch (list_id) {
    case MALWARE:
      *list = kMalwareList;
      break;
    case PHISH:
      *list = kPhishingList;
      break;
    case BINURL:
      *list = kBinUrlList;
      break;
    case CSDWHITELIST:
      *list = kCsdWhiteList;
      break;
    case DOWNLOADWHITELIST:
      *list = kDownloadWhiteList;
      break;
    case EXTENSIONBLACKLIST:
      *list = kExtensionBlacklist;
      break;
    case IPBLACKLIST:
      *list = kIPBlacklist;
      break;
    case UNWANTEDURL:
      *list = kUnwantedUrlList;
      break;
    case MODULEWHITELIST:
      *list = kModuleWhitelist;
      break;
    case RESOURCEBLACKLIST:
      *list = kResourceBlacklist;
      break;
    default:
      return false;
  }
  DCHECK(IsKnownList(*list));
  return true;
}


SBFullHash SBFullHashForString(const base::StringPiece& str) {
  SBFullHash h;
  crypto::SHA256HashString(str, &h.full_hash, sizeof(h.full_hash));
  return h;
}

SBFullHash StringToSBFullHash(const std::string& hash_in) {
  DCHECK_EQ(crypto::kSHA256Length, hash_in.size());
  SBFullHash hash_out;
  memcpy(hash_out.full_hash, hash_in.data(), crypto::kSHA256Length);
  return hash_out;
}

std::string SBFullHashToString(const SBFullHash& hash) {
  DCHECK_EQ(crypto::kSHA256Length, sizeof(hash.full_hash));
  return std::string(hash.full_hash, sizeof(hash.full_hash));
}

void UrlToFullHashes(const GURL& url,
                     bool include_whitelist_hashes,
                     std::vector<SBFullHash>* full_hashes) {
  // Include this function in traces because it's not cheap so it should be
  // called sparingly.
  TRACE_EVENT2("loader", "safe_browsing::UrlToFullHashes", "url", url.spec(),
               "include_whitelist_hashes", include_whitelist_hashes);
  std::string canon_host;
  std::string canon_path;
  std::string canon_query;
  V4ProtocolManagerUtil::CanonicalizeUrl(url, &canon_host, &canon_path,
                                         &canon_query);

  std::vector<std::string> hosts;
  if (url.HostIsIPAddress()) {
    hosts.push_back(url.host());
  } else {
    V4ProtocolManagerUtil::GenerateHostVariantsToCheck(canon_host, &hosts);
  }

  std::vector<std::string> paths;
  V4ProtocolManagerUtil::GeneratePathVariantsToCheck(canon_path, canon_query,
                                                     &paths);

  for (const std::string& host : hosts) {
    for (const std::string& path : paths) {
      full_hashes->push_back(
          SBFullHashForString(host + path));

      // We may have /foo as path-prefix in the whitelist which should
      // also match with /foo/bar and /foo?bar.  Hence, for every path
      // that ends in '/' we also add the path without the slash.
      if (include_whitelist_hashes && path.size() > 1 && path.back() == '/') {
        full_hashes->push_back(SBFullHashForString(
            host + path.substr(0, path.size() - 1)));
      }
    }
  }
}

}  // namespace safe_browsing
