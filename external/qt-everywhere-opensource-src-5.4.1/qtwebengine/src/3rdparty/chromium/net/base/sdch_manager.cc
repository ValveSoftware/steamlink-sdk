// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/sdch_manager.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "crypto/sha2.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/url_request/url_request_http_job.h"

namespace net {

//------------------------------------------------------------------------------
// static

// Adjust SDCH limits downwards for mobile.
#if defined(OS_ANDROID) || defined(OS_IOS)
// static
const size_t SdchManager::kMaxDictionaryCount = 1;
const size_t SdchManager::kMaxDictionarySize = 150 * 1000;
#else
// static
const size_t SdchManager::kMaxDictionaryCount = 20;
const size_t SdchManager::kMaxDictionarySize = 1000 * 1000;
#endif

// static
bool SdchManager::g_sdch_enabled_ = true;

// static
bool SdchManager::g_secure_scheme_supported_ = false;

//------------------------------------------------------------------------------
SdchManager::Dictionary::Dictionary(const std::string& dictionary_text,
                                    size_t offset,
                                    const std::string& client_hash,
                                    const GURL& gurl,
                                    const std::string& domain,
                                    const std::string& path,
                                    const base::Time& expiration,
                                    const std::set<int>& ports)
    : text_(dictionary_text, offset),
      client_hash_(client_hash),
      url_(gurl),
      domain_(domain),
      path_(path),
      expiration_(expiration),
      ports_(ports) {
}

SdchManager::Dictionary::~Dictionary() {
}

bool SdchManager::Dictionary::CanAdvertise(const GURL& target_url) {
  /* The specific rules of when a dictionary should be advertised in an
     Avail-Dictionary header are modeled after the rules for cookie scoping. The
     terms "domain-match" and "pathmatch" are defined in RFC 2965 [6]. A
     dictionary may be advertised in the Avail-Dictionaries header exactly when
     all of the following are true:
      1. The server's effective host name domain-matches the Domain attribute of
         the dictionary.
      2. If the dictionary has a Port attribute, the request port is one of the
         ports listed in the Port attribute.
      3. The request URI path-matches the path header of the dictionary.
      4. The request is not an HTTPS request.
     We can override (ignore) item (4) only when we have explicitly enabled
     HTTPS support AND dictionary has been acquired over HTTPS.
    */
  if (!DomainMatch(target_url, domain_))
    return false;
  if (!ports_.empty() && 0 == ports_.count(target_url.EffectiveIntPort()))
    return false;
  if (path_.size() && !PathMatch(target_url.path(), path_))
    return false;
  if (!SdchManager::secure_scheme_supported() && target_url.SchemeIsSecure())
    return false;
  if (target_url.SchemeIsSecure() && !url_.SchemeIsSecure())
    return false;
  if (base::Time::Now() > expiration_)
    return false;
  return true;
}

//------------------------------------------------------------------------------
// Security functions restricting loads and use of dictionaries.

// static
bool SdchManager::Dictionary::CanSet(const std::string& domain,
                                     const std::string& path,
                                     const std::set<int>& ports,
                                     const GURL& dictionary_url) {
  /*
  A dictionary is invalid and must not be stored if any of the following are
  true:
    1. The dictionary has no Domain attribute.
    2. The effective host name that derives from the referer URL host name does
      not domain-match the Domain attribute.
    3. The Domain attribute is a top level domain.
    4. The referer URL host is a host domain name (not IP address) and has the
      form HD, where D is the value of the Domain attribute, and H is a string
      that contains one or more dots.
    5. If the dictionary has a Port attribute and the referer URL's port was not
      in the list.
  */

  // TODO(jar): Redirects in dictionary fetches might plausibly be problematic,
  // and hence the conservative approach is to not allow any redirects (if there
  // were any... then don't allow the dictionary to be set).

  if (domain.empty()) {
    SdchErrorRecovery(DICTIONARY_MISSING_DOMAIN_SPECIFIER);
    return false;  // Domain is required.
  }
  if (registry_controlled_domains::GetDomainAndRegistry(
        domain,
        registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES).empty()) {
    SdchErrorRecovery(DICTIONARY_SPECIFIES_TOP_LEVEL_DOMAIN);
    return false;  // domain was a TLD.
  }
  if (!Dictionary::DomainMatch(dictionary_url, domain)) {
    SdchErrorRecovery(DICTIONARY_DOMAIN_NOT_MATCHING_SOURCE_URL);
    return false;
  }

  std::string referrer_url_host = dictionary_url.host();
  size_t postfix_domain_index = referrer_url_host.rfind(domain);
  // See if it is indeed a postfix, or just an internal string.
  if (referrer_url_host.size() == postfix_domain_index + domain.size()) {
    // It is a postfix... so check to see if there's a dot in the prefix.
    size_t end_of_host_index = referrer_url_host.find_first_of('.');
    if (referrer_url_host.npos != end_of_host_index  &&
        end_of_host_index < postfix_domain_index) {
      SdchErrorRecovery(DICTIONARY_REFERER_URL_HAS_DOT_IN_PREFIX);
      return false;
    }
  }

  if (!ports.empty()
      && 0 == ports.count(dictionary_url.EffectiveIntPort())) {
    SdchErrorRecovery(DICTIONARY_PORT_NOT_MATCHING_SOURCE_URL);
    return false;
  }
  return true;
}

// static
bool SdchManager::Dictionary::CanUse(const GURL& referring_url) {
  /*
    1. The request URL's host name domain-matches the Domain attribute of the
      dictionary.
    2. If the dictionary has a Port attribute, the request port is one of the
      ports listed in the Port attribute.
    3. The request URL path-matches the path attribute of the dictionary.
    4. The request is not an HTTPS request.
    We can override (ignore) item (4) only when we have explicitly enabled
    HTTPS support AND dictionary has been acquired over HTTPS.
*/
  if (!DomainMatch(referring_url, domain_)) {
    SdchErrorRecovery(DICTIONARY_FOUND_HAS_WRONG_DOMAIN);
    return false;
  }
  if (!ports_.empty()
      && 0 == ports_.count(referring_url.EffectiveIntPort())) {
    SdchErrorRecovery(DICTIONARY_FOUND_HAS_WRONG_PORT_LIST);
    return false;
  }
  if (path_.size() && !PathMatch(referring_url.path(), path_)) {
    SdchErrorRecovery(DICTIONARY_FOUND_HAS_WRONG_PATH);
    return false;
  }
  if (!SdchManager::secure_scheme_supported() &&
      referring_url.SchemeIsSecure()) {
    SdchErrorRecovery(DICTIONARY_FOUND_HAS_WRONG_SCHEME);
    return false;
  }
  if (referring_url.SchemeIsSecure() && !url_.SchemeIsSecure()) {
    SdchErrorRecovery(DICTIONARY_FOUND_HAS_WRONG_SCHEME);
    return false;
  }

  // TODO(jar): Remove overly restrictive failsafe test (added per security
  // review) when we have a need to be more general.
  if (!referring_url.SchemeIsHTTPOrHTTPS()) {
    SdchErrorRecovery(ATTEMPT_TO_DECODE_NON_HTTP_DATA);
    return false;
  }

  return true;
}

bool SdchManager::Dictionary::PathMatch(const std::string& path,
                                        const std::string& restriction) {
  /*  Must be either:
  1. P2 is equal to P1
  2. P2 is a prefix of P1 and either the final character in P2 is "/" or the
      character following P2 in P1 is "/".
      */
  if (path == restriction)
    return true;
  size_t prefix_length = restriction.size();
  if (prefix_length > path.size())
    return false;  // Can't be a prefix.
  if (0 != path.compare(0, prefix_length, restriction))
    return false;
  return restriction[prefix_length - 1] == '/' || path[prefix_length] == '/';
}

// static
bool SdchManager::Dictionary::DomainMatch(const GURL& gurl,
                                          const std::string& restriction) {
  // TODO(jar): This is not precisely a domain match definition.
  return gurl.DomainIs(restriction.data(), restriction.size());
}

//------------------------------------------------------------------------------
SdchManager::SdchManager() {
  DCHECK(CalledOnValidThread());
}

SdchManager::~SdchManager() {
  DCHECK(CalledOnValidThread());
  while (!dictionaries_.empty()) {
    DictionaryMap::iterator it = dictionaries_.begin();
    dictionaries_.erase(it->first);
  }
}

void SdchManager::ClearData() {
  blacklisted_domains_.clear();
  exponential_blacklist_count_.clear();
  allow_latency_experiment_.clear();
  if (fetcher_.get())
    fetcher_->Cancel();

  // Note that this may result in not having dictionaries we've advertised
  // for incoming responses.  The window is relatively small (as ClearData()
  // is not expected to be called frequently), so we rely on meta-refresh
  // to handle this case.
  dictionaries_.clear();
}

// static
void SdchManager::SdchErrorRecovery(ProblemCodes problem) {
  UMA_HISTOGRAM_ENUMERATION("Sdch3.ProblemCodes_4", problem, MAX_PROBLEM_CODE);
}

void SdchManager::set_sdch_fetcher(SdchFetcher* fetcher) {
  DCHECK(CalledOnValidThread());
  fetcher_.reset(fetcher);
}

// static
void SdchManager::EnableSdchSupport(bool enabled) {
  g_sdch_enabled_ = enabled;
}

// static
void SdchManager::EnableSecureSchemeSupport(bool enabled) {
  g_secure_scheme_supported_ = enabled;
}

void SdchManager::BlacklistDomain(const GURL& url) {
  SetAllowLatencyExperiment(url, false);

  std::string domain(StringToLowerASCII(url.host()));
  int count = blacklisted_domains_[domain];
  if (count > 0)
    return;  // Domain is already blacklisted.

  count = 1 + 2 * exponential_blacklist_count_[domain];
  if (count > 0)
    exponential_blacklist_count_[domain] = count;
  else
    count = INT_MAX;

  blacklisted_domains_[domain] = count;
}

void SdchManager::BlacklistDomainForever(const GURL& url) {
  SetAllowLatencyExperiment(url, false);

  std::string domain(StringToLowerASCII(url.host()));
  exponential_blacklist_count_[domain] = INT_MAX;
  blacklisted_domains_[domain] = INT_MAX;
}

void SdchManager::ClearBlacklistings() {
  blacklisted_domains_.clear();
  exponential_blacklist_count_.clear();
}

void SdchManager::ClearDomainBlacklisting(const std::string& domain) {
  blacklisted_domains_.erase(StringToLowerASCII(domain));
}

int SdchManager::BlackListDomainCount(const std::string& domain) {
  if (blacklisted_domains_.end() == blacklisted_domains_.find(domain))
    return 0;
  return blacklisted_domains_[StringToLowerASCII(domain)];
}

int SdchManager::BlacklistDomainExponential(const std::string& domain) {
  if (exponential_blacklist_count_.end() ==
      exponential_blacklist_count_.find(domain))
    return 0;
  return exponential_blacklist_count_[StringToLowerASCII(domain)];
}

bool SdchManager::IsInSupportedDomain(const GURL& url) {
  DCHECK(CalledOnValidThread());
  if (!g_sdch_enabled_ )
    return false;

  if (!secure_scheme_supported() && url.SchemeIsSecure())
    return false;

  if (blacklisted_domains_.empty())
    return true;

  std::string domain(StringToLowerASCII(url.host()));
  DomainCounter::iterator it = blacklisted_domains_.find(domain);
  if (blacklisted_domains_.end() == it)
    return true;

  int count = it->second - 1;
  if (count > 0)
    blacklisted_domains_[domain] = count;
  else
    blacklisted_domains_.erase(domain);
  SdchErrorRecovery(DOMAIN_BLACKLIST_INCLUDES_TARGET);
  return false;
}

void SdchManager::FetchDictionary(const GURL& request_url,
                                  const GURL& dictionary_url) {
  DCHECK(CalledOnValidThread());
  if (CanFetchDictionary(request_url, dictionary_url) && fetcher_.get())
    fetcher_->Schedule(dictionary_url);
}

bool SdchManager::CanFetchDictionary(const GURL& referring_url,
                                     const GURL& dictionary_url) const {
  DCHECK(CalledOnValidThread());
  /* The user agent may retrieve a dictionary from the dictionary URL if all of
     the following are true:
       1 The dictionary URL host name matches the referrer URL host name and
           scheme.
       2 The dictionary URL host name domain matches the parent domain of the
           referrer URL host name
       3 The parent domain of the referrer URL host name is not a top level
           domain
       4 The dictionary URL is not an HTTPS URL.
   */
  // Item (1) above implies item (2).  Spec should be updated.
  // I take "host name match" to be "is identical to"
  if (referring_url.host() != dictionary_url.host() ||
      referring_url.scheme() != dictionary_url.scheme()) {
    SdchErrorRecovery(DICTIONARY_LOAD_ATTEMPT_FROM_DIFFERENT_HOST);
    return false;
  }
  if (!secure_scheme_supported() && referring_url.SchemeIsSecure()) {
    SdchErrorRecovery(DICTIONARY_SELECTED_FOR_SSL);
    return false;
  }

  // TODO(jar): Remove this failsafe conservative hack which is more restrictive
  // than current SDCH spec when needed, and justified by security audit.
  if (!referring_url.SchemeIsHTTPOrHTTPS()) {
    SdchErrorRecovery(DICTIONARY_SELECTED_FROM_NON_HTTP);
    return false;
  }

  return true;
}

bool SdchManager::AddSdchDictionary(const std::string& dictionary_text,
    const GURL& dictionary_url) {
  DCHECK(CalledOnValidThread());
  std::string client_hash;
  std::string server_hash;
  GenerateHash(dictionary_text, &client_hash, &server_hash);
  if (dictionaries_.find(server_hash) != dictionaries_.end()) {
    SdchErrorRecovery(DICTIONARY_ALREADY_LOADED);
    return false;  // Already loaded.
  }

  std::string domain, path;
  std::set<int> ports;
  base::Time expiration(base::Time::Now() + base::TimeDelta::FromDays(30));

  if (dictionary_text.empty()) {
    SdchErrorRecovery(DICTIONARY_HAS_NO_TEXT);
    return false;  // Missing header.
  }

  size_t header_end = dictionary_text.find("\n\n");
  if (std::string::npos == header_end) {
    SdchErrorRecovery(DICTIONARY_HAS_NO_HEADER);
    return false;  // Missing header.
  }
  size_t line_start = 0;  // Start of line being parsed.
  while (1) {
    size_t line_end = dictionary_text.find('\n', line_start);
    DCHECK(std::string::npos != line_end);
    DCHECK_LE(line_end, header_end);

    size_t colon_index = dictionary_text.find(':', line_start);
    if (std::string::npos == colon_index) {
      SdchErrorRecovery(DICTIONARY_HEADER_LINE_MISSING_COLON);
      return false;  // Illegal line missing a colon.
    }

    if (colon_index > line_end)
      break;

    size_t value_start = dictionary_text.find_first_not_of(" \t",
                                                           colon_index + 1);
    if (std::string::npos != value_start) {
      if (value_start >= line_end)
        break;
      std::string name(dictionary_text, line_start, colon_index - line_start);
      std::string value(dictionary_text, value_start, line_end - value_start);
      name = StringToLowerASCII(name);
      if (name == "domain") {
        domain = value;
      } else if (name == "path") {
        path = value;
      } else if (name == "format-version") {
        if (value != "1.0")
          return false;
      } else if (name == "max-age") {
        int64 seconds;
        base::StringToInt64(value, &seconds);
        expiration = base::Time::Now() + base::TimeDelta::FromSeconds(seconds);
      } else if (name == "port") {
        int port;
        base::StringToInt(value, &port);
        if (port >= 0)
          ports.insert(port);
      }
    }

    if (line_end >= header_end)
      break;
    line_start = line_end + 1;
  }

  if (!IsInSupportedDomain(dictionary_url))
    return false;

  if (!Dictionary::CanSet(domain, path, ports, dictionary_url))
    return false;

  // TODO(jar): Remove these hacks to preclude a DOS attack involving piles of
  // useless dictionaries.  We should probably have a cache eviction plan,
  // instead of just blocking additions.  For now, with the spec in flux, it
  // is probably not worth doing eviction handling.
  if (kMaxDictionarySize < dictionary_text.size()) {
    SdchErrorRecovery(DICTIONARY_IS_TOO_LARGE);
    return false;
  }
  if (kMaxDictionaryCount <= dictionaries_.size()) {
    SdchErrorRecovery(DICTIONARY_COUNT_EXCEEDED);
    return false;
  }

  UMA_HISTOGRAM_COUNTS("Sdch3.Dictionary size loaded", dictionary_text.size());
  DVLOG(1) << "Loaded dictionary with client hash " << client_hash
           << " and server hash " << server_hash;
  Dictionary* dictionary =
      new Dictionary(dictionary_text, header_end + 2, client_hash,
                     dictionary_url, domain, path, expiration, ports);
  dictionaries_[server_hash] = dictionary;
  return true;
}

void SdchManager::GetVcdiffDictionary(
    const std::string& server_hash,
    const GURL& referring_url,
    scoped_refptr<Dictionary>* dictionary) {
  DCHECK(CalledOnValidThread());
  *dictionary = NULL;
  DictionaryMap::iterator it = dictionaries_.find(server_hash);
  if (it == dictionaries_.end()) {
    return;
  }
  scoped_refptr<Dictionary> matching_dictionary = it->second;
  if (!IsInSupportedDomain(referring_url))
    return;
  if (!matching_dictionary->CanUse(referring_url))
    return;
  *dictionary = matching_dictionary;
}

// TODO(jar): If we have evictions from the dictionaries_, then we need to
// change this interface to return a list of reference counted Dictionary
// instances that can be used if/when a server specifies one.
void SdchManager::GetAvailDictionaryList(const GURL& target_url,
                                         std::string* list) {
  DCHECK(CalledOnValidThread());
  int count = 0;
  for (DictionaryMap::iterator it = dictionaries_.begin();
       it != dictionaries_.end(); ++it) {
    if (!IsInSupportedDomain(target_url))
      continue;
    if (!it->second->CanAdvertise(target_url))
      continue;
    ++count;
    if (!list->empty())
      list->append(",");
    list->append(it->second->client_hash());
  }
  // Watch to see if we have corrupt or numerous dictionaries.
  if (count > 0)
    UMA_HISTOGRAM_COUNTS("Sdch3.Advertisement_Count", count);
}

// static
void SdchManager::GenerateHash(const std::string& dictionary_text,
    std::string* client_hash, std::string* server_hash) {
  char binary_hash[32];
  crypto::SHA256HashString(dictionary_text, binary_hash, sizeof(binary_hash));

  std::string first_48_bits(&binary_hash[0], 6);
  std::string second_48_bits(&binary_hash[6], 6);
  UrlSafeBase64Encode(first_48_bits, client_hash);
  UrlSafeBase64Encode(second_48_bits, server_hash);

  DCHECK_EQ(server_hash->length(), 8u);
  DCHECK_EQ(client_hash->length(), 8u);
}

//------------------------------------------------------------------------------
// Methods for supporting latency experiments.

bool SdchManager::AllowLatencyExperiment(const GURL& url) const {
  DCHECK(CalledOnValidThread());
  return allow_latency_experiment_.end() !=
      allow_latency_experiment_.find(url.host());
}

void SdchManager::SetAllowLatencyExperiment(const GURL& url, bool enable) {
  DCHECK(CalledOnValidThread());
  if (enable) {
    allow_latency_experiment_.insert(url.host());
    return;
  }
  ExperimentSet::iterator it = allow_latency_experiment_.find(url.host());
  if (allow_latency_experiment_.end() == it)
    return;  // It was already erased, or never allowed.
  SdchErrorRecovery(LATENCY_TEST_DISALLOWED);
  allow_latency_experiment_.erase(it);
}

// static
void SdchManager::UrlSafeBase64Encode(const std::string& input,
                                      std::string* output) {
  // Since this is only done during a dictionary load, and hashes are only 8
  // characters, we just do the simple fixup, rather than rewriting the encoder.
  base::Base64Encode(input, output);
  for (size_t i = 0; i < output->size(); ++i) {
    switch (output->data()[i]) {
      case '+':
        (*output)[i] = '-';
        continue;
      case '/':
        (*output)[i] = '_';
        continue;
      default:
        continue;
    }
  }
}

}  // namespace net
