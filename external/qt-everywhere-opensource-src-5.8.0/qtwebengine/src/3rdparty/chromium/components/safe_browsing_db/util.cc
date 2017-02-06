// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/util.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "crypto/sha2.h"
#include "net/base/escape.h"
#include "url/gurl.h"
#include "url/url_util.h"

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

void GenerateHostVariantsToCheck(const std::string& host,
                                 std::vector<std::string>* hosts) {
  hosts->clear();

  if (host.empty())
    return;

  // Per the Safe Browsing Protocol v2 spec, we try the host, and also up to 4
  // hostnames formed by starting with the last 5 components and successively
  // removing the leading component.  The last component isn't examined alone,
  // since it's the TLD or a subcomponent thereof.
  //
  // Note that we don't need to be clever about stopping at the "real" eTLD --
  // the data on the server side has been filtered to ensure it will not
  // blacklist a whole TLD, and it's not significantly slower on our side to
  // just check too much.
  //
  // Also note that because we have a simple blacklist, not some sort of complex
  // whitelist-in-blacklist or vice versa, it doesn't matter what order we check
  // these in.
  const size_t kMaxHostsToCheck = 4;
  bool skipped_last_component = false;
  for (std::string::const_reverse_iterator i(host.rbegin());
       i != host.rend() && hosts->size() < kMaxHostsToCheck; ++i) {
    if (*i == '.') {
      if (skipped_last_component)
        hosts->push_back(std::string(i.base(), host.end()));
      else
        skipped_last_component = true;
    }
  }
  hosts->push_back(host);
}

void GeneratePathVariantsToCheck(const std::string& path,
                                 const std::string& query,
                                 std::vector<std::string>* paths) {
  paths->clear();

  if (path.empty())
    return;

  // Per the Safe Browsing Protocol v2 spec, we try the exact path with/without
  // the query parameters, and also up to 4 paths formed by starting at the root
  // and adding more path components.
  //
  // As with the hosts above, it doesn't matter what order we check these in.
  const size_t kMaxPathsToCheck = 4;
  for (std::string::const_iterator i(path.begin());
       i != path.end() && paths->size() < kMaxPathsToCheck; ++i) {
    if (*i == '/')
      paths->push_back(std::string(path.begin(), i + 1));
  }

  if (!paths->empty() && paths->back() != path)
    paths->push_back(path);

  if (!query.empty())
    paths->push_back(path + "?" + query);
}

}  // namespace

// ThreatMetadata ------------------------------------------------------------
ThreatMetadata::ThreatMetadata()
    : threat_pattern_type(ThreatPatternType::NONE) {}

ThreatMetadata::ThreatMetadata(const ThreatMetadata& other) = default;

ThreatMetadata::~ThreatMetadata() {}

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


std::string Unescape(const std::string& url) {
  std::string unescaped_str(url);
  const int kMaxLoopIterations = 1024;
  size_t old_size = 0;
  int loop_var = 0;
  do {
    old_size = unescaped_str.size();
    unescaped_str = net::UnescapeURLComponent(
        unescaped_str,
        net::UnescapeRule::SPOOFING_AND_CONTROL_CHARS |
            net::UnescapeRule::SPACES | net::UnescapeRule::PATH_SEPARATORS |
            net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
  } while (old_size != unescaped_str.size() &&
           ++loop_var <= kMaxLoopIterations);

  return unescaped_str;
}

std::string Escape(const std::string& url) {
  std::string escaped_str;
  // The escaped string is larger so allocate double the length to reduce the
  // chance of the string being grown.
  escaped_str.reserve(url.length() * 2);
  const char* kHexString = "0123456789ABCDEF";
  for (size_t i = 0; i < url.length(); i++) {
    unsigned char c = static_cast<unsigned char>(url[i]);
    if (c <= ' ' || c > '~' || c == '#' || c == '%') {
      escaped_str += '%';
      escaped_str += kHexString[c >> 4];
      escaped_str += kHexString[c & 0xf];
    } else {
      escaped_str += c;
    }
  }

  return escaped_str;
}

std::string RemoveConsecutiveChars(base::StringPiece str, const char c) {
  std::string output;
  // Output is at most the length of the original string.
  output.reserve(str.size());

  size_t i = 0;
  while (i < str.size()) {
    output.append(1, str[i++]);
    if (str[i - 1] == c) {
      while (i < str.size() && str[i] == c) {
        i++;
      }
    }
  }

  return output;
}

// Canonicalizes url as per Google Safe Browsing Specification.
// See section 6.1 in
// http://code.google.com/p/google-safe-browsing/wiki/Protocolv2Spec.
void CanonicalizeUrl(const GURL& url,
                     std::string* canonicalized_hostname,
                     std::string* canonicalized_path,
                     std::string* canonicalized_query) {
  DCHECK(url.is_valid());

  // We only canonicalize "normal" URLs.
  if (!url.IsStandard())
    return;

  // Following canonicalization steps are excluded since url parsing takes care
  // of those :-
  // 1. Remove any tab (0x09), CR (0x0d), and LF (0x0a) chars from url.
  //    (Exclude escaped version of these chars).
  // 2. Normalize hostname to 4 dot-seperated decimal values.
  // 3. Lowercase hostname.
  // 4. Resolve path sequences "/../" and "/./".

  // That leaves us with the following :-
  // 1. Remove fragment in URL.
  GURL url_without_fragment;
  GURL::Replacements f_replacements;
  f_replacements.ClearRef();
  f_replacements.ClearUsername();
  f_replacements.ClearPassword();
  url_without_fragment = url.ReplaceComponents(f_replacements);

  // 2. Do URL unescaping until no more hex encoded characters exist.
  std::string url_unescaped_str(Unescape(url_without_fragment.spec()));
  url::Parsed parsed;
  url::ParseStandardURL(url_unescaped_str.data(), url_unescaped_str.length(),
                        &parsed);

  // 3. In hostname, remove all leading and trailing dots.
  base::StringPiece host;
  if (parsed.host.len > 0)
    host.set(url_unescaped_str.data() + parsed.host.begin, parsed.host.len);

  base::StringPiece host_without_end_dots =
      base::TrimString(host, ".", base::TrimPositions::TRIM_ALL);

  // 4. In hostname, replace consecutive dots with a single dot.
  std::string host_without_consecutive_dots(RemoveConsecutiveChars(
      host_without_end_dots, '.'));

  // 5. In path, replace runs of consecutive slashes with a single slash.
  base::StringPiece path;
  if (parsed.path.len > 0)
    path.set(url_unescaped_str.data() + parsed.path.begin, parsed.path.len);
  std::string path_without_consecutive_slash(RemoveConsecutiveChars(path, '/'));

  url::Replacements<char> hp_replacements;
  hp_replacements.SetHost(
      host_without_consecutive_dots.data(),
      url::Component(0, host_without_consecutive_dots.length()));
  hp_replacements.SetPath(
      path_without_consecutive_slash.data(),
      url::Component(0, path_without_consecutive_slash.length()));

  std::string url_unescaped_with_can_hostpath;
  url::StdStringCanonOutput output(&url_unescaped_with_can_hostpath);
  url::Parsed temp_parsed;
  url::ReplaceComponents(url_unescaped_str.data(),
                         url_unescaped_str.length(),
                         parsed,
                         hp_replacements,
                         NULL,
                         &output,
                         &temp_parsed);
  output.Complete();

  // 6. Step needed to revert escaping done in url::ReplaceComponents.
  url_unescaped_with_can_hostpath = Unescape(url_unescaped_with_can_hostpath);

  // 7. After performing all above steps, percent-escape all chars in url which
  // are <= ASCII 32, >= 127, #, %. Escapes must be uppercase hex characters.
  std::string escaped_canon_url_str(Escape(url_unescaped_with_can_hostpath));
  url::Parsed final_parsed;
  url::ParseStandardURL(escaped_canon_url_str.data(),
                        escaped_canon_url_str.length(),
                        &final_parsed);

  if (canonicalized_hostname && final_parsed.host.len > 0) {
    *canonicalized_hostname =
        escaped_canon_url_str.substr(final_parsed.host.begin,
                                     final_parsed.host.len);
  }
  if (canonicalized_path && final_parsed.path.len > 0) {
    *canonicalized_path = escaped_canon_url_str.substr(final_parsed.path.begin,
                                                       final_parsed.path.len);
  }
  if (canonicalized_query && final_parsed.query.len > 0) {
    *canonicalized_query = escaped_canon_url_str.substr(
        final_parsed.query.begin, final_parsed.query.len);
  }
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
  CanonicalizeUrl(url, &canon_host, &canon_path, &canon_query);

  std::vector<std::string> hosts;
  if (url.HostIsIPAddress()) {
    hosts.push_back(url.host());
  } else {
    GenerateHostVariantsToCheck(canon_host, &hosts);
  }

  std::vector<std::string> paths;
  GeneratePathVariantsToCheck(canon_path, canon_query, &paths);

  for (const std::string& host : hosts) {
    for (const std::string& path : paths) {
      full_hashes->push_back(
          SBFullHashForString(host + path));

      // We may have /foo as path-prefix in the whitelist which should
      // also match with /foo/bar and /foo?bar.  Hence, for every path
      // that ends in '/' we also add the path without the slash.
      if (include_whitelist_hashes && path.size() > 1 &&
          path[path.size() - 1] == '/') {
        full_hashes->push_back(SBFullHashForString(
            host + path.substr(0, path.size() - 1)));
      }
    }
  }
}

void GenerateHostsToCheck(const GURL& url, std::vector<std::string>* hosts) {
  std::string canon_host;
  CanonicalizeUrl(url, &canon_host, NULL, NULL);
  GenerateHostVariantsToCheck(canon_host, hosts);
}

void GeneratePathsToCheck(const GURL& url, std::vector<std::string>* paths) {
  std::string canon_path;
  std::string canon_query;
  CanonicalizeUrl(url, NULL, &canon_path, &canon_query);
  GeneratePathVariantsToCheck(canon_path, canon_query, paths);
}

void GeneratePatternsToCheck(const GURL& url, std::vector<std::string>* urls) {
  std::string canon_host;
  std::string canon_path;
  std::string canon_query;
  CanonicalizeUrl(url, &canon_host, &canon_path, &canon_query);

  std::vector<std::string> hosts, paths;
  GenerateHostVariantsToCheck(canon_host, &hosts);
  GeneratePathVariantsToCheck(canon_path, canon_query, &paths);
  for (size_t h = 0; h < hosts.size(); ++h) {
    for (size_t p = 0; p < paths.size(); ++p) {
      urls->push_back(hosts[h] + paths[p]);
    }
  }
}

}  // namespace safe_browsing
