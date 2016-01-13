// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/transport_security_state.h"

#if defined(USE_OPENSSL)
#include <openssl/ecdsa.h>
#include <openssl/ssl.h>
#else  // !defined(USE_OPENSSL)
#include <cryptohi.h>
#include <hasht.h>
#include <keyhi.h>
#include <nspr.h>
#include <pk11pub.h>
#endif

#include <algorithm>

#include "base/base64.h"
#include "base/build_time.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "crypto/sha2.h"
#include "net/base/dns_util.h"
#include "net/cert/x509_cert_types.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_security_headers.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

#if defined(USE_OPENSSL)
#include "crypto/openssl_util.h"
#endif

namespace net {

namespace {

std::string HashesToBase64String(const HashValueVector& hashes) {
  std::string str;
  for (size_t i = 0; i != hashes.size(); ++i) {
    if (i != 0)
      str += ",";
    str += hashes[i].ToString();
  }
  return str;
}

std::string HashHost(const std::string& canonicalized_host) {
  char hashed[crypto::kSHA256Length];
  crypto::SHA256HashString(canonicalized_host, hashed, sizeof(hashed));
  return std::string(hashed, sizeof(hashed));
}

// Returns true if the intersection of |a| and |b| is not empty. If either
// |a| or |b| is empty, returns false.
bool HashesIntersect(const HashValueVector& a,
                     const HashValueVector& b) {
  for (HashValueVector::const_iterator i = a.begin(); i != a.end(); ++i) {
    HashValueVector::const_iterator j =
        std::find_if(b.begin(), b.end(), HashValuesEqual(*i));
    if (j != b.end())
      return true;
  }
  return false;
}

bool AddHash(const char* sha1_hash,
             HashValueVector* out) {
  HashValue hash(HASH_VALUE_SHA1);
  memcpy(hash.data(), sha1_hash, hash.size());
  out->push_back(hash);
  return true;
}

}  // namespace

TransportSecurityState::TransportSecurityState()
  : delegate_(NULL) {
  DCHECK(CalledOnValidThread());
}

TransportSecurityState::Iterator::Iterator(const TransportSecurityState& state)
    : iterator_(state.enabled_hosts_.begin()),
      end_(state.enabled_hosts_.end()) {
}

TransportSecurityState::Iterator::~Iterator() {}

bool TransportSecurityState::ShouldSSLErrorsBeFatal(const std::string& host,
                                                    bool sni_enabled) {
  DomainState state;
  if (GetStaticDomainState(host, sni_enabled, &state))
    return true;
  return GetDynamicDomainState(host, &state);
}

bool TransportSecurityState::ShouldUpgradeToSSL(const std::string& host,
                                                bool sni_enabled) {
  DomainState dynamic_state;
  if (GetDynamicDomainState(host, &dynamic_state))
    return dynamic_state.ShouldUpgradeToSSL();

  DomainState static_state;
  if (GetStaticDomainState(host, sni_enabled, &static_state) &&
      static_state.ShouldUpgradeToSSL()) {
      return true;
  }

  return false;
}

bool TransportSecurityState::CheckPublicKeyPins(const std::string& host,
                                                bool sni_enabled,
                                                const HashValueVector& hashes,
                                                std::string* failure_log) {
  DomainState dynamic_state;
  if (GetDynamicDomainState(host, &dynamic_state))
    return dynamic_state.CheckPublicKeyPins(hashes, failure_log);

  DomainState static_state;
  if (GetStaticDomainState(host, sni_enabled, &static_state) &&
      static_state.CheckPublicKeyPins(hashes, failure_log)) {
      return true;
  }

  return false;
}

bool TransportSecurityState::HasPublicKeyPins(const std::string& host,
                                              bool sni_enabled) {
  DomainState dynamic_state;
  if (GetDynamicDomainState(host, &dynamic_state))
    return dynamic_state.HasPublicKeyPins();

  DomainState static_state;
  if (GetStaticDomainState(host, sni_enabled, &static_state)) {
    if (static_state.HasPublicKeyPins())
      return true;
  }

  return false;
}

void TransportSecurityState::SetDelegate(
    TransportSecurityState::Delegate* delegate) {
  DCHECK(CalledOnValidThread());
  delegate_ = delegate;
}

void TransportSecurityState::EnableHost(const std::string& host,
                                        const DomainState& state) {
  DCHECK(CalledOnValidThread());

  const std::string canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return;

  DomainState state_copy(state);
  // No need to store this value since it is redundant. (|canonicalized_host|
  // is the map key.)
  state_copy.domain.clear();

  enabled_hosts_[HashHost(canonicalized_host)] = state_copy;
  DirtyNotify();
}

bool TransportSecurityState::DeleteDynamicDataForHost(const std::string& host) {
  DCHECK(CalledOnValidThread());

  const std::string canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return false;

  DomainStateMap::iterator i = enabled_hosts_.find(
      HashHost(canonicalized_host));
  if (i != enabled_hosts_.end()) {
    enabled_hosts_.erase(i);
    DirtyNotify();
    return true;
  }
  return false;
}

void TransportSecurityState::ClearDynamicData() {
  DCHECK(CalledOnValidThread());
  enabled_hosts_.clear();
}

void TransportSecurityState::DeleteAllDynamicDataSince(const base::Time& time) {
  DCHECK(CalledOnValidThread());

  bool dirtied = false;
  DomainStateMap::iterator i = enabled_hosts_.begin();
  while (i != enabled_hosts_.end()) {
    if (i->second.sts.last_observed >= time &&
        i->second.pkp.last_observed >= time) {
      dirtied = true;
      enabled_hosts_.erase(i++);
      continue;
    }

    if (i->second.sts.last_observed >= time) {
      dirtied = true;
      i->second.sts.upgrade_mode = DomainState::MODE_DEFAULT;
    } else if (i->second.pkp.last_observed >= time) {
      dirtied = true;
      i->second.pkp.spki_hashes.clear();
      i->second.pkp.expiry = base::Time();
    }
    ++i;
  }

  if (dirtied)
    DirtyNotify();
}

TransportSecurityState::~TransportSecurityState() {
  DCHECK(CalledOnValidThread());
}

void TransportSecurityState::DirtyNotify() {
  DCHECK(CalledOnValidThread());

  if (delegate_)
    delegate_->StateIsDirty(this);
}

// static
std::string TransportSecurityState::CanonicalizeHost(const std::string& host) {
  // We cannot perform the operations as detailed in the spec here as |host|
  // has already undergone IDN processing before it reached us. Thus, we check
  // that there are no invalid characters in the host and lowercase the result.

  std::string new_host;
  if (!DNSDomainFromDot(host, &new_host)) {
    // DNSDomainFromDot can fail if any label is > 63 bytes or if the whole
    // name is >255 bytes. However, search terms can have those properties.
    return std::string();
  }

  for (size_t i = 0; new_host[i]; i += new_host[i] + 1) {
    const unsigned label_length = static_cast<unsigned>(new_host[i]);
    if (!label_length)
      break;

    for (size_t j = 0; j < label_length; ++j) {
      new_host[i + 1 + j] = tolower(new_host[i + 1 + j]);
    }
  }

  return new_host;
}

// |ReportUMAOnPinFailure| uses these to report which domain was associated
// with the public key pinning failure.
//
// DO NOT CHANGE THE ORDERING OF THESE NAMES OR REMOVE ANY OF THEM. Add new
// domains at the END of the listing (but before DOMAIN_NUM_EVENTS).
enum SecondLevelDomainName {
  DOMAIN_NOT_PINNED,

  DOMAIN_GOOGLE_COM,
  DOMAIN_ANDROID_COM,
  DOMAIN_GOOGLE_ANALYTICS_COM,
  DOMAIN_GOOGLEPLEX_COM,
  DOMAIN_YTIMG_COM,
  DOMAIN_GOOGLEUSERCONTENT_COM,
  DOMAIN_YOUTUBE_COM,
  DOMAIN_GOOGLEAPIS_COM,
  DOMAIN_GOOGLEADSERVICES_COM,
  DOMAIN_GOOGLECODE_COM,
  DOMAIN_APPSPOT_COM,
  DOMAIN_GOOGLESYNDICATION_COM,
  DOMAIN_DOUBLECLICK_NET,
  DOMAIN_GSTATIC_COM,
  DOMAIN_GMAIL_COM,
  DOMAIN_GOOGLEMAIL_COM,
  DOMAIN_GOOGLEGROUPS_COM,

  DOMAIN_TORPROJECT_ORG,

  DOMAIN_TWITTER_COM,
  DOMAIN_TWIMG_COM,

  DOMAIN_AKAMAIHD_NET,

  DOMAIN_TOR2WEB_ORG,

  DOMAIN_YOUTU_BE,
  DOMAIN_GOOGLECOMMERCE_COM,
  DOMAIN_URCHIN_COM,
  DOMAIN_GOO_GL,
  DOMAIN_G_CO,
  DOMAIN_GOOGLE_AC,
  DOMAIN_GOOGLE_AD,
  DOMAIN_GOOGLE_AE,
  DOMAIN_GOOGLE_AF,
  DOMAIN_GOOGLE_AG,
  DOMAIN_GOOGLE_AM,
  DOMAIN_GOOGLE_AS,
  DOMAIN_GOOGLE_AT,
  DOMAIN_GOOGLE_AZ,
  DOMAIN_GOOGLE_BA,
  DOMAIN_GOOGLE_BE,
  DOMAIN_GOOGLE_BF,
  DOMAIN_GOOGLE_BG,
  DOMAIN_GOOGLE_BI,
  DOMAIN_GOOGLE_BJ,
  DOMAIN_GOOGLE_BS,
  DOMAIN_GOOGLE_BY,
  DOMAIN_GOOGLE_CA,
  DOMAIN_GOOGLE_CAT,
  DOMAIN_GOOGLE_CC,
  DOMAIN_GOOGLE_CD,
  DOMAIN_GOOGLE_CF,
  DOMAIN_GOOGLE_CG,
  DOMAIN_GOOGLE_CH,
  DOMAIN_GOOGLE_CI,
  DOMAIN_GOOGLE_CL,
  DOMAIN_GOOGLE_CM,
  DOMAIN_GOOGLE_CN,
  DOMAIN_CO_AO,
  DOMAIN_CO_BW,
  DOMAIN_CO_CK,
  DOMAIN_CO_CR,
  DOMAIN_CO_HU,
  DOMAIN_CO_ID,
  DOMAIN_CO_IL,
  DOMAIN_CO_IM,
  DOMAIN_CO_IN,
  DOMAIN_CO_JE,
  DOMAIN_CO_JP,
  DOMAIN_CO_KE,
  DOMAIN_CO_KR,
  DOMAIN_CO_LS,
  DOMAIN_CO_MA,
  DOMAIN_CO_MZ,
  DOMAIN_CO_NZ,
  DOMAIN_CO_TH,
  DOMAIN_CO_TZ,
  DOMAIN_CO_UG,
  DOMAIN_CO_UK,
  DOMAIN_CO_UZ,
  DOMAIN_CO_VE,
  DOMAIN_CO_VI,
  DOMAIN_CO_ZA,
  DOMAIN_CO_ZM,
  DOMAIN_CO_ZW,
  DOMAIN_COM_AF,
  DOMAIN_COM_AG,
  DOMAIN_COM_AI,
  DOMAIN_COM_AR,
  DOMAIN_COM_AU,
  DOMAIN_COM_BD,
  DOMAIN_COM_BH,
  DOMAIN_COM_BN,
  DOMAIN_COM_BO,
  DOMAIN_COM_BR,
  DOMAIN_COM_BY,
  DOMAIN_COM_BZ,
  DOMAIN_COM_CN,
  DOMAIN_COM_CO,
  DOMAIN_COM_CU,
  DOMAIN_COM_CY,
  DOMAIN_COM_DO,
  DOMAIN_COM_EC,
  DOMAIN_COM_EG,
  DOMAIN_COM_ET,
  DOMAIN_COM_FJ,
  DOMAIN_COM_GE,
  DOMAIN_COM_GH,
  DOMAIN_COM_GI,
  DOMAIN_COM_GR,
  DOMAIN_COM_GT,
  DOMAIN_COM_HK,
  DOMAIN_COM_IQ,
  DOMAIN_COM_JM,
  DOMAIN_COM_JO,
  DOMAIN_COM_KH,
  DOMAIN_COM_KW,
  DOMAIN_COM_LB,
  DOMAIN_COM_LY,
  DOMAIN_COM_MT,
  DOMAIN_COM_MX,
  DOMAIN_COM_MY,
  DOMAIN_COM_NA,
  DOMAIN_COM_NF,
  DOMAIN_COM_NG,
  DOMAIN_COM_NI,
  DOMAIN_COM_NP,
  DOMAIN_COM_NR,
  DOMAIN_COM_OM,
  DOMAIN_COM_PA,
  DOMAIN_COM_PE,
  DOMAIN_COM_PH,
  DOMAIN_COM_PK,
  DOMAIN_COM_PL,
  DOMAIN_COM_PR,
  DOMAIN_COM_PY,
  DOMAIN_COM_QA,
  DOMAIN_COM_RU,
  DOMAIN_COM_SA,
  DOMAIN_COM_SB,
  DOMAIN_COM_SG,
  DOMAIN_COM_SL,
  DOMAIN_COM_SV,
  DOMAIN_COM_TJ,
  DOMAIN_COM_TN,
  DOMAIN_COM_TR,
  DOMAIN_COM_TW,
  DOMAIN_COM_UA,
  DOMAIN_COM_UY,
  DOMAIN_COM_VC,
  DOMAIN_COM_VE,
  DOMAIN_COM_VN,
  DOMAIN_GOOGLE_CV,
  DOMAIN_GOOGLE_CZ,
  DOMAIN_GOOGLE_DE,
  DOMAIN_GOOGLE_DJ,
  DOMAIN_GOOGLE_DK,
  DOMAIN_GOOGLE_DM,
  DOMAIN_GOOGLE_DZ,
  DOMAIN_GOOGLE_EE,
  DOMAIN_GOOGLE_ES,
  DOMAIN_GOOGLE_FI,
  DOMAIN_GOOGLE_FM,
  DOMAIN_GOOGLE_FR,
  DOMAIN_GOOGLE_GA,
  DOMAIN_GOOGLE_GE,
  DOMAIN_GOOGLE_GG,
  DOMAIN_GOOGLE_GL,
  DOMAIN_GOOGLE_GM,
  DOMAIN_GOOGLE_GP,
  DOMAIN_GOOGLE_GR,
  DOMAIN_GOOGLE_GY,
  DOMAIN_GOOGLE_HK,
  DOMAIN_GOOGLE_HN,
  DOMAIN_GOOGLE_HR,
  DOMAIN_GOOGLE_HT,
  DOMAIN_GOOGLE_HU,
  DOMAIN_GOOGLE_IE,
  DOMAIN_GOOGLE_IM,
  DOMAIN_GOOGLE_INFO,
  DOMAIN_GOOGLE_IQ,
  DOMAIN_GOOGLE_IS,
  DOMAIN_GOOGLE_IT,
  DOMAIN_IT_AO,
  DOMAIN_GOOGLE_JE,
  DOMAIN_GOOGLE_JO,
  DOMAIN_GOOGLE_JOBS,
  DOMAIN_GOOGLE_JP,
  DOMAIN_GOOGLE_KG,
  DOMAIN_GOOGLE_KI,
  DOMAIN_GOOGLE_KZ,
  DOMAIN_GOOGLE_LA,
  DOMAIN_GOOGLE_LI,
  DOMAIN_GOOGLE_LK,
  DOMAIN_GOOGLE_LT,
  DOMAIN_GOOGLE_LU,
  DOMAIN_GOOGLE_LV,
  DOMAIN_GOOGLE_MD,
  DOMAIN_GOOGLE_ME,
  DOMAIN_GOOGLE_MG,
  DOMAIN_GOOGLE_MK,
  DOMAIN_GOOGLE_ML,
  DOMAIN_GOOGLE_MN,
  DOMAIN_GOOGLE_MS,
  DOMAIN_GOOGLE_MU,
  DOMAIN_GOOGLE_MV,
  DOMAIN_GOOGLE_MW,
  DOMAIN_GOOGLE_NE,
  DOMAIN_NE_JP,
  DOMAIN_GOOGLE_NET,
  DOMAIN_GOOGLE_NL,
  DOMAIN_GOOGLE_NO,
  DOMAIN_GOOGLE_NR,
  DOMAIN_GOOGLE_NU,
  DOMAIN_OFF_AI,
  DOMAIN_GOOGLE_PK,
  DOMAIN_GOOGLE_PL,
  DOMAIN_GOOGLE_PN,
  DOMAIN_GOOGLE_PS,
  DOMAIN_GOOGLE_PT,
  DOMAIN_GOOGLE_RO,
  DOMAIN_GOOGLE_RS,
  DOMAIN_GOOGLE_RU,
  DOMAIN_GOOGLE_RW,
  DOMAIN_GOOGLE_SC,
  DOMAIN_GOOGLE_SE,
  DOMAIN_GOOGLE_SH,
  DOMAIN_GOOGLE_SI,
  DOMAIN_GOOGLE_SK,
  DOMAIN_GOOGLE_SM,
  DOMAIN_GOOGLE_SN,
  DOMAIN_GOOGLE_SO,
  DOMAIN_GOOGLE_ST,
  DOMAIN_GOOGLE_TD,
  DOMAIN_GOOGLE_TG,
  DOMAIN_GOOGLE_TK,
  DOMAIN_GOOGLE_TL,
  DOMAIN_GOOGLE_TM,
  DOMAIN_GOOGLE_TN,
  DOMAIN_GOOGLE_TO,
  DOMAIN_GOOGLE_TP,
  DOMAIN_GOOGLE_TT,
  DOMAIN_GOOGLE_US,
  DOMAIN_GOOGLE_UZ,
  DOMAIN_GOOGLE_VG,
  DOMAIN_GOOGLE_VU,
  DOMAIN_GOOGLE_WS,

  DOMAIN_CHROMIUM_ORG,

  DOMAIN_CRYPTO_CAT,
  DOMAIN_LAVABIT_COM,

  DOMAIN_GOOGLETAGMANAGER_COM,
  DOMAIN_GOOGLETAGSERVICES_COM,

  // Boundary value for UMA_HISTOGRAM_ENUMERATION:
  DOMAIN_NUM_EVENTS
};

// PublicKeyPins contains a number of SubjectPublicKeyInfo hashes for a site.
// The validated certificate chain for the site must not include any of
// |excluded_hashes| and must include one or more of |required_hashes|.
struct PublicKeyPins {
  const char* const* required_hashes;
  const char* const* excluded_hashes;
};

struct HSTSPreload {
  uint8 length;
  bool include_subdomains;
  char dns_name[38];
  bool https_required;
  PublicKeyPins pins;
  SecondLevelDomainName second_level_domain_name;
};

static bool HasPreload(const struct HSTSPreload* entries, size_t num_entries,
                       const std::string& canonicalized_host, size_t i,
                       TransportSecurityState::DomainState* out, bool* ret) {
  for (size_t j = 0; j < num_entries; j++) {
    if (entries[j].length == canonicalized_host.size() - i &&
        memcmp(entries[j].dns_name, &canonicalized_host[i],
               entries[j].length) == 0) {
      if (!entries[j].include_subdomains && i != 0) {
        *ret = false;
      } else {
        out->sts.include_subdomains = entries[j].include_subdomains;
        out->sts.last_observed = base::GetBuildTime();
        out->pkp.include_subdomains = entries[j].include_subdomains;
        out->pkp.last_observed = base::GetBuildTime();
        *ret = true;
        out->sts.upgrade_mode =
            TransportSecurityState::DomainState::MODE_FORCE_HTTPS;
        if (!entries[j].https_required)
          out->sts.upgrade_mode =
              TransportSecurityState::DomainState::MODE_DEFAULT;
        if (entries[j].pins.required_hashes) {
          const char* const* sha1_hash = entries[j].pins.required_hashes;
          while (*sha1_hash) {
            AddHash(*sha1_hash, &out->pkp.spki_hashes);
            sha1_hash++;
          }
        }
        if (entries[j].pins.excluded_hashes) {
          const char* const* sha1_hash = entries[j].pins.excluded_hashes;
          while (*sha1_hash) {
            AddHash(*sha1_hash, &out->pkp.bad_spki_hashes);
            sha1_hash++;
          }
        }
      }
      return true;
    }
  }
  return false;
}

#include "net/http/transport_security_state_static.h"

// Returns the HSTSPreload entry for the |canonicalized_host| in |entries|,
// or NULL if there is none. Prefers exact hostname matches to those that
// match only because HSTSPreload.include_subdomains is true.
//
// |canonicalized_host| should be the hostname as canonicalized by
// CanonicalizeHost.
static const struct HSTSPreload* GetHSTSPreload(
    const std::string& canonicalized_host,
    const struct HSTSPreload* entries,
    size_t num_entries) {
  for (size_t i = 0; canonicalized_host[i]; i += canonicalized_host[i] + 1) {
    for (size_t j = 0; j < num_entries; j++) {
      const struct HSTSPreload* entry = entries + j;

      if (i != 0 && !entry->include_subdomains)
        continue;

      if (entry->length == canonicalized_host.size() - i &&
          memcmp(entry->dns_name, &canonicalized_host[i], entry->length) == 0) {
        return entry;
      }
    }
  }

  return NULL;
}

bool TransportSecurityState::AddHSTSHeader(const std::string& host,
                                           const std::string& value) {
  DCHECK(CalledOnValidThread());

  base::Time now = base::Time::Now();
  base::TimeDelta max_age;
  TransportSecurityState::DomainState domain_state;
  GetDynamicDomainState(host, &domain_state);
  if (ParseHSTSHeader(value, &max_age, &domain_state.sts.include_subdomains)) {
    // Handle max-age == 0.
    if (max_age.InSeconds() == 0)
      domain_state.sts.upgrade_mode = DomainState::MODE_DEFAULT;
    else
      domain_state.sts.upgrade_mode = DomainState::MODE_FORCE_HTTPS;
    domain_state.sts.last_observed = now;
    domain_state.sts.expiry = now + max_age;
    EnableHost(host, domain_state);
    return true;
  }
  return false;
}

bool TransportSecurityState::AddHPKPHeader(const std::string& host,
                                           const std::string& value,
                                           const SSLInfo& ssl_info) {
  DCHECK(CalledOnValidThread());

  base::Time now = base::Time::Now();
  base::TimeDelta max_age;
  TransportSecurityState::DomainState domain_state;
  GetDynamicDomainState(host, &domain_state);
  if (ParseHPKPHeader(value,
                      ssl_info.public_key_hashes,
                      &max_age,
                      &domain_state.pkp.include_subdomains,
                      &domain_state.pkp.spki_hashes)) {
    // Handle max-age == 0.
    if (max_age.InSeconds() == 0)
      domain_state.pkp.spki_hashes.clear();
    domain_state.pkp.last_observed = now;
    domain_state.pkp.expiry = now + max_age;
    EnableHost(host, domain_state);
    return true;
  }
  return false;
}

bool TransportSecurityState::AddHSTS(const std::string& host,
                                     const base::Time& expiry,
                                     bool include_subdomains) {
  DCHECK(CalledOnValidThread());

  // Copy-and-modify the existing DomainState for this host (if any).
  TransportSecurityState::DomainState domain_state;
  const std::string canonicalized_host = CanonicalizeHost(host);
  const std::string hashed_host = HashHost(canonicalized_host);
  DomainStateMap::const_iterator i = enabled_hosts_.find(
      hashed_host);
  if (i != enabled_hosts_.end())
    domain_state = i->second;

  domain_state.sts.last_observed = base::Time::Now();
  domain_state.sts.include_subdomains = include_subdomains;
  domain_state.sts.expiry = expiry;
  domain_state.sts.upgrade_mode = DomainState::MODE_FORCE_HTTPS;
  EnableHost(host, domain_state);
  return true;
}

bool TransportSecurityState::AddHPKP(const std::string& host,
                                     const base::Time& expiry,
                                     bool include_subdomains,
                                     const HashValueVector& hashes) {
  DCHECK(CalledOnValidThread());

  // Copy-and-modify the existing DomainState for this host (if any).
  TransportSecurityState::DomainState domain_state;
  const std::string canonicalized_host = CanonicalizeHost(host);
  const std::string hashed_host = HashHost(canonicalized_host);
  DomainStateMap::const_iterator i = enabled_hosts_.find(
      hashed_host);
  if (i != enabled_hosts_.end())
    domain_state = i->second;

  domain_state.pkp.last_observed = base::Time::Now();
  domain_state.pkp.include_subdomains = include_subdomains;
  domain_state.pkp.expiry = expiry;
  domain_state.pkp.spki_hashes = hashes;
  EnableHost(host, domain_state);
  return true;
}

// static
bool TransportSecurityState::IsGooglePinnedProperty(const std::string& host,
                                                    bool sni_enabled) {
  std::string canonicalized_host = CanonicalizeHost(host);
  const struct HSTSPreload* entry =
      GetHSTSPreload(canonicalized_host, kPreloadedSTS, kNumPreloadedSTS);

  if (entry && entry->pins.required_hashes == kGoogleAcceptableCerts)
    return true;

  if (sni_enabled) {
    entry = GetHSTSPreload(canonicalized_host, kPreloadedSNISTS,
                           kNumPreloadedSNISTS);
    if (entry && entry->pins.required_hashes == kGoogleAcceptableCerts)
      return true;
  }

  return false;
}

// static
void TransportSecurityState::ReportUMAOnPinFailure(const std::string& host) {
  std::string canonicalized_host = CanonicalizeHost(host);

  const struct HSTSPreload* entry =
      GetHSTSPreload(canonicalized_host, kPreloadedSTS, kNumPreloadedSTS);

  if (!entry) {
    entry = GetHSTSPreload(canonicalized_host, kPreloadedSNISTS,
                           kNumPreloadedSNISTS);
  }

  if (!entry) {
    // We don't care to report pin failures for dynamic pins.
    return;
  }

  DCHECK(entry);
  DCHECK(entry->pins.required_hashes);
  DCHECK(entry->second_level_domain_name != DOMAIN_NOT_PINNED);

  UMA_HISTOGRAM_ENUMERATION("Net.PublicKeyPinFailureDomain",
                            entry->second_level_domain_name, DOMAIN_NUM_EVENTS);
}

// static
bool TransportSecurityState::IsBuildTimely() {
  const base::Time build_time = base::GetBuildTime();
  // We consider built-in information to be timely for 10 weeks.
  return (base::Time::Now() - build_time).InDays() < 70 /* 10 weeks */;
}

bool TransportSecurityState::GetStaticDomainState(const std::string& host,
                                                  bool sni_enabled,
                                                  DomainState* out) const {
  DCHECK(CalledOnValidThread());

  const std::string canonicalized_host = CanonicalizeHost(host);

  out->sts.upgrade_mode = DomainState::MODE_FORCE_HTTPS;
  out->sts.include_subdomains = false;
  out->pkp.include_subdomains = false;

  const bool is_build_timely = IsBuildTimely();

  for (size_t i = 0; canonicalized_host[i]; i += canonicalized_host[i] + 1) {
    std::string host_sub_chunk(&canonicalized_host[i],
                               canonicalized_host.size() - i);
    out->domain = DNSDomainToString(host_sub_chunk);
    bool ret;
    if (is_build_timely &&
        HasPreload(kPreloadedSTS, kNumPreloadedSTS, canonicalized_host, i, out,
                   &ret)) {
      return ret;
    }
    if (sni_enabled &&
        is_build_timely &&
        HasPreload(kPreloadedSNISTS, kNumPreloadedSNISTS, canonicalized_host, i,
                   out, &ret)) {
      return ret;
    }
  }

  return false;
}

bool TransportSecurityState::GetDynamicDomainState(const std::string& host,
                                                   DomainState* result) {
  DCHECK(CalledOnValidThread());

  DomainState state;
  const std::string canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return false;

  base::Time current_time(base::Time::Now());

  for (size_t i = 0; canonicalized_host[i]; i += canonicalized_host[i] + 1) {
    std::string host_sub_chunk(&canonicalized_host[i],
                               canonicalized_host.size() - i);
    DomainStateMap::iterator j =
        enabled_hosts_.find(HashHost(host_sub_chunk));
    if (j == enabled_hosts_.end())
      continue;

    if (current_time > j->second.sts.expiry &&
        current_time > j->second.pkp.expiry) {
      enabled_hosts_.erase(j);
      DirtyNotify();
      continue;
    }

    state = j->second;
    state.domain = DNSDomainToString(host_sub_chunk);

    // Succeed if we matched the domain exactly or if subdomain matches are
    // allowed.
    if (i == 0 || j->second.sts.include_subdomains ||
        j->second.pkp.include_subdomains) {
      *result = state;
      return true;
    }

    return false;
  }

  return false;
}

void TransportSecurityState::AddOrUpdateEnabledHosts(
    const std::string& hashed_host, const DomainState& state) {
  DCHECK(CalledOnValidThread());
  enabled_hosts_[hashed_host] = state;
}

TransportSecurityState::DomainState::DomainState() {
  sts.upgrade_mode = MODE_DEFAULT;
  sts.include_subdomains = false;
  pkp.include_subdomains = false;
}

TransportSecurityState::DomainState::~DomainState() {
}

bool TransportSecurityState::DomainState::CheckPublicKeyPins(
    const HashValueVector& hashes, std::string* failure_log) const {
  // Validate that hashes is not empty. By the time this code is called (in
  // production), that should never happen, but it's good to be defensive.
  // And, hashes *can* be empty in some test scenarios.
  if (hashes.empty()) {
    failure_log->append(
        "Rejecting empty public key chain for public-key-pinned domains: " +
        domain);
    return false;
  }

  if (HashesIntersect(pkp.bad_spki_hashes, hashes)) {
    failure_log->append("Rejecting public key chain for domain " + domain +
                        ". Validated chain: " + HashesToBase64String(hashes) +
                        ", matches one or more bad hashes: " +
                        HashesToBase64String(pkp.bad_spki_hashes));
    return false;
  }

  // If there are no pins, then any valid chain is acceptable.
  if (pkp.spki_hashes.empty())
    return true;

  if (HashesIntersect(pkp.spki_hashes, hashes)) {
    return true;
  }

  failure_log->append("Rejecting public key chain for domain " + domain +
                      ". Validated chain: " + HashesToBase64String(hashes) +
                      ", expected: " + HashesToBase64String(pkp.spki_hashes));
  return false;
}

bool TransportSecurityState::DomainState::ShouldUpgradeToSSL() const {
  return sts.upgrade_mode == MODE_FORCE_HTTPS;
}

bool TransportSecurityState::DomainState::ShouldSSLErrorsBeFatal() const {
  return true;
}

bool TransportSecurityState::DomainState::HasPublicKeyPins() const {
  return pkp.spki_hashes.size() > 0 || pkp.bad_spki_hashes.size() > 0;
}

TransportSecurityState::DomainState::PKPState::PKPState() {
}

TransportSecurityState::DomainState::PKPState::~PKPState() {
}

}  // namespace
