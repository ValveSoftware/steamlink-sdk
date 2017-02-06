// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/ssl_config/ssl_config_service_manager.h"

#include <stdint.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/ssl_config/ssl_config_prefs.h"
#include "components/ssl_config/ssl_config_switches.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_config_service.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace {

// Converts a ListValue of StringValues into a vector of strings. Any Values
// which cannot be converted will be skipped.
std::vector<std::string> ListValueToStringVector(const base::ListValue* value) {
  std::vector<std::string> results;
  results.reserve(value->GetSize());
  std::string s;
  for (base::ListValue::const_iterator it = value->begin(); it != value->end();
       ++it) {
    if (!(*it)->GetAsString(&s))
      continue;
    results.push_back(s);
  }
  return results;
}

// Parses a vector of cipher suite strings, returning a sorted vector
// containing the underlying SSL/TLS cipher suites. Unrecognized/invalid
// cipher suites will be ignored.
std::vector<uint16_t> ParseCipherSuites(
    const std::vector<std::string>& cipher_strings) {
  std::vector<uint16_t> cipher_suites;
  cipher_suites.reserve(cipher_strings.size());

  for (std::vector<std::string>::const_iterator it = cipher_strings.begin();
       it != cipher_strings.end(); ++it) {
    uint16_t cipher_suite = 0;
    if (!net::ParseSSLCipherString(*it, &cipher_suite)) {
      LOG(ERROR) << "Ignoring unrecognized or unparsable cipher suite: " << *it;
      continue;
    }
    cipher_suites.push_back(cipher_suite);
  }
  std::sort(cipher_suites.begin(), cipher_suites.end());
  return cipher_suites;
}

// Returns the SSL protocol version (as a uint16_t) represented by a string.
// Returns 0 if the string is invalid.
uint16_t SSLProtocolVersionFromString(const std::string& version_str) {
  uint16_t version = 0;  // Invalid.
  if (version_str == switches::kSSLVersionTLSv1) {
    version = net::SSL_PROTOCOL_VERSION_TLS1;
  } else if (version_str == switches::kSSLVersionTLSv11) {
    version = net::SSL_PROTOCOL_VERSION_TLS1_1;
  } else if (version_str == switches::kSSLVersionTLSv12) {
    version = net::SSL_PROTOCOL_VERSION_TLS1_2;
  }
  return version;
}

const base::Feature kDHECiphersFeature{
    "DHECiphers", base::FEATURE_DISABLED_BY_DEFAULT,
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//  SSLConfigServicePref

// An SSLConfigService which stores a cached version of the current SSLConfig
// prefs, which are updated by SSLConfigServiceManagerPref when the prefs
// change.
class SSLConfigServicePref : public net::SSLConfigService {
 public:
  explicit SSLConfigServicePref(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);

  // Store SSL config settings in |config|. Must only be called from IO thread.
  void GetSSLConfig(net::SSLConfig* config) override;

 private:
  // Allow the pref watcher to update our internal state.
  friend class SSLConfigServiceManagerPref;

  ~SSLConfigServicePref() override {}

  // This method is posted to the IO thread from the browser thread to carry the
  // new config information.
  void SetNewSSLConfig(const net::SSLConfig& new_config);

  // Cached value of prefs, should only be accessed from IO thread.
  net::SSLConfig cached_config_;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SSLConfigServicePref);
};

SSLConfigServicePref::SSLConfigServicePref(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : io_task_runner_(io_task_runner) {}

void SSLConfigServicePref::GetSSLConfig(net::SSLConfig* config) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  *config = cached_config_;
}

void SSLConfigServicePref::SetNewSSLConfig(const net::SSLConfig& new_config) {
  net::SSLConfig orig_config = cached_config_;
  cached_config_ = new_config;
  ProcessConfigUpdate(orig_config, new_config);
}

////////////////////////////////////////////////////////////////////////////////
//  SSLConfigServiceManagerPref

// The manager for holding and updating an SSLConfigServicePref instance.
class SSLConfigServiceManagerPref : public ssl_config::SSLConfigServiceManager {
 public:
  SSLConfigServiceManagerPref(
      PrefService* local_state,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);
  ~SSLConfigServiceManagerPref() override {}

  // Register local_state SSL preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  net::SSLConfigService* Get() override;

 private:
  // Callback for preference changes.  This will post the changes to the IO
  // thread with SetNewSSLConfig.
  void OnPreferenceChanged(PrefService* prefs, const std::string& pref_name);

  // Store SSL config settings in |config|, directly from the preferences. Must
  // only be called from UI thread.
  void GetSSLConfigFromPrefs(net::SSLConfig* config);

  // Processes changes to the disabled cipher suites preference, updating the
  // cached list of parsed SSL/TLS cipher suites that are disabled.
  void OnDisabledCipherSuitesChange(PrefService* local_state);

  PrefChangeRegistrar local_state_change_registrar_;

  // The local_state prefs (should only be accessed from UI thread)
  BooleanPrefMember rev_checking_enabled_;
  BooleanPrefMember rev_checking_required_local_anchors_;
  StringPrefMember ssl_version_min_;
  StringPrefMember ssl_version_max_;
  BooleanPrefMember dhe_enabled_;

  // The cached list of disabled SSL cipher suites.
  std::vector<uint16_t> disabled_cipher_suites_;

  scoped_refptr<SSLConfigServicePref> ssl_config_service_;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SSLConfigServiceManagerPref);
};

SSLConfigServiceManagerPref::SSLConfigServiceManagerPref(
    PrefService* local_state,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : ssl_config_service_(new SSLConfigServicePref(io_task_runner)),
      io_task_runner_(io_task_runner) {
  DCHECK(local_state);

  // Restore DHE-based ciphers if enabled via features.
  // TODO(davidben): Remove this when the removal has succeeded.
  // https://crbug.com/619194.
  if (base::FeatureList::IsEnabled(kDHECiphersFeature)) {
    local_state->SetDefaultPrefValue(ssl_config::prefs::kDHEEnabled,
                                     new base::FundamentalValue(true));
  }

  PrefChangeRegistrar::NamedChangeCallback local_state_callback =
      base::Bind(&SSLConfigServiceManagerPref::OnPreferenceChanged,
                 base::Unretained(this), local_state);

  rev_checking_enabled_.Init(ssl_config::prefs::kCertRevocationCheckingEnabled,
                             local_state, local_state_callback);
  rev_checking_required_local_anchors_.Init(
      ssl_config::prefs::kCertRevocationCheckingRequiredLocalAnchors,
      local_state, local_state_callback);
  ssl_version_min_.Init(ssl_config::prefs::kSSLVersionMin, local_state,
                        local_state_callback);
  ssl_version_max_.Init(ssl_config::prefs::kSSLVersionMax, local_state,
                        local_state_callback);
  dhe_enabled_.Init(ssl_config::prefs::kDHEEnabled, local_state,
                    local_state_callback);

  local_state_change_registrar_.Init(local_state);
  local_state_change_registrar_.Add(ssl_config::prefs::kCipherSuiteBlacklist,
                                    local_state_callback);

  OnDisabledCipherSuitesChange(local_state);

  // Initialize from UI thread.  This is okay as there shouldn't be anything on
  // the IO thread trying to access it yet.
  GetSSLConfigFromPrefs(&ssl_config_service_->cached_config_);
}

// static
void SSLConfigServiceManagerPref::RegisterPrefs(PrefRegistrySimple* registry) {
  net::SSLConfig default_config;
  registry->RegisterBooleanPref(
      ssl_config::prefs::kCertRevocationCheckingEnabled,
      default_config.rev_checking_enabled);
  registry->RegisterBooleanPref(
      ssl_config::prefs::kCertRevocationCheckingRequiredLocalAnchors,
      default_config.rev_checking_required_local_anchors);
  registry->RegisterStringPref(ssl_config::prefs::kSSLVersionMin,
                               std::string());
  registry->RegisterStringPref(ssl_config::prefs::kSSLVersionMax,
                               std::string());
  registry->RegisterListPref(ssl_config::prefs::kCipherSuiteBlacklist);
  registry->RegisterBooleanPref(ssl_config::prefs::kDHEEnabled,
                                default_config.dhe_enabled);
}

net::SSLConfigService* SSLConfigServiceManagerPref::Get() {
  return ssl_config_service_.get();
}

void SSLConfigServiceManagerPref::OnPreferenceChanged(
    PrefService* prefs,
    const std::string& pref_name_in) {
  DCHECK(prefs);
  if (pref_name_in == ssl_config::prefs::kCipherSuiteBlacklist)
    OnDisabledCipherSuitesChange(prefs);

  net::SSLConfig new_config;
  GetSSLConfigFromPrefs(&new_config);

  // Post a task to |io_loop| with the new configuration, so it can
  // update |cached_config_|.
  io_task_runner_->PostTask(FROM_HERE,
                            base::Bind(&SSLConfigServicePref::SetNewSSLConfig,
                                       ssl_config_service_.get(), new_config));
}

void SSLConfigServiceManagerPref::GetSSLConfigFromPrefs(
    net::SSLConfig* config) {
  // rev_checking_enabled was formerly a user-settable preference, but now
  // it is managed-only.
  if (rev_checking_enabled_.IsManaged())
    config->rev_checking_enabled = rev_checking_enabled_.GetValue();
  else
    config->rev_checking_enabled = false;
  config->rev_checking_required_local_anchors =
      rev_checking_required_local_anchors_.GetValue();
  std::string version_min_str = ssl_version_min_.GetValue();
  std::string version_max_str = ssl_version_max_.GetValue();
  config->version_min = net::kDefaultSSLVersionMin;
  config->version_max = net::kDefaultSSLVersionMax;
  uint16_t version_min = SSLProtocolVersionFromString(version_min_str);
  uint16_t version_max = SSLProtocolVersionFromString(version_max_str);
  if (version_min) {
    config->version_min = version_min;
  }
  if (version_max) {
    uint16_t supported_version_max = config->version_max;
    config->version_max = std::min(supported_version_max, version_max);
  }
  config->disabled_cipher_suites = disabled_cipher_suites_;
  config->dhe_enabled = dhe_enabled_.GetValue();
}

void SSLConfigServiceManagerPref::OnDisabledCipherSuitesChange(
    PrefService* local_state) {
  const base::ListValue* value =
      local_state->GetList(ssl_config::prefs::kCipherSuiteBlacklist);
  disabled_cipher_suites_ = ParseCipherSuites(ListValueToStringVector(value));
}

////////////////////////////////////////////////////////////////////////////////
//  SSLConfigServiceManager

namespace ssl_config {
// static
SSLConfigServiceManager* SSLConfigServiceManager::CreateDefaultManager(
    PrefService* local_state,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner) {
  return new SSLConfigServiceManagerPref(local_state, io_task_runner);
}

// static
void SSLConfigServiceManager::RegisterPrefs(PrefRegistrySimple* registry) {
  SSLConfigServiceManagerPref::RegisterPrefs(registry);
}
}  // namespace ssl_config
