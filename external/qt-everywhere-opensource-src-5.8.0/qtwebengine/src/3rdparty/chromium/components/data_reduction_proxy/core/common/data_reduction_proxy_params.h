// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_PARAMS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_PARAMS_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_config_values.h"
#include "net/proxy/proxy_server.h"
#include "url/gurl.h"

namespace base {
class TimeDelta;
}

namespace net {
class HostPortPair;
class ProxyServer;
}

namespace data_reduction_proxy {

// The data_reduction_proxy::params namespace is a collection of methods to
// determine the operating parameters of the Data Reduction Proxy as specified
// by field trials and command line switches.
namespace params {

// Returns true if this client is part of the field trial that should display
// a promotion for the data reduction proxy.
bool IsIncludedInPromoFieldTrial();

// Returns true if this client is part of a field trial that runs a holdback
// experiment. A holdback experiment is one in which a fraction of browser
// instances will not be configured to use the data reduction proxy even if
// users have enabled it to be used. The UI will not indicate that a holdback
// is in effect.
bool IsIncludedInHoldbackFieldTrial();

// Returns the name of the trusted SPDY/HTTP2 proxy field trial.
const char* GetTrustedSpdyProxyFieldTrialName();

// Returns true if this client is part of the enabled group of the trusted
// SPDY/HTTP2 proxy field trial.
bool IsIncludedInTrustedSpdyProxyFieldTrial();

// Returns true if this client is part of the field trial that should display
// a promotion for the data reduction proxy on Android One devices.
bool IsIncludedInAndroidOnePromoFieldTrial(const char* build_fingerprint);

// Returns the name of the Lo-Fi field trial.
const char* GetLoFiFieldTrialName();

// Returns the name of the Lo-Fi field trial that configures LoFi flags when it
// is force enabled through flags.
const char* GetLoFiFlagFieldTrialName();

// Returns true if this client is part of the "Enabled" or "Enabled_Preview"
// group of the Lo-Fi field trial, both of which mean Lo-Fi should be enabled.
bool IsIncludedInLoFiEnabledFieldTrial();

// Returns true if this client is part of the "Control" group of the Lo-Fi field
// trial.
bool IsIncludedInLoFiControlFieldTrial();

// Returns true if this client is part of the "Preview" group of the Lo-Fi field
// trial.
bool IsIncludedInLoFiPreviewFieldTrial();

// Returns true if this client is part of the field trial that should enable
// server experiments for the data reduction proxy.
bool IsIncludedInServerExperimentsFieldTrial();

// Returns true if this client is part of the tamper detection experiment.
bool IsIncludedInTamperDetectionExperiment();

// Returns true if this client has any of the values to enable Lo-Fi mode for
// the "data-reduction-proxy-lo-fi" command line switch. This includes the
// "always-on", "cellular-only", and "slow-connections-only" values.
bool IsLoFiOnViaFlags();

// Returns true if this client has the command line switch to enable Lo-Fi
// mode always on.
bool IsLoFiAlwaysOnViaFlags();

// Returns true if this client has the command line switch to enable Lo-Fi
// mode only on cellular connections.
bool IsLoFiCellularOnlyViaFlags();

// Returns true if this client has the command line switch to enable Lo-Fi
// mode only on slow connections.
bool IsLoFiSlowConnectionsOnlyViaFlags();

// Returns true if this client has the command line switch to disable Lo-Fi
// mode.
bool IsLoFiDisabledViaFlags();

// Returns true if this client has the command line switch to enable Lo-Fi
// previews. This means a preview should be requested instead of placeholders
// whenever Lo-Fi mode is on.
bool AreLoFiPreviewsEnabledViaFlags();

// Returns true if this client has the command line switch to enable forced
// pageload metrics pingbacks on every page load.
bool IsForcePingbackEnabledViaFlags();

// Returns true if this client has the command line switch to show
// interstitials for data reduction proxy bypasses.
bool WarnIfNoDataReductionProxy();

// Returns true if this client is part of a field trial that sets the origin
// proxy server as quic://proxy.googlezip.net.
bool IsIncludedInQuicFieldTrial();

const char* GetQuicFieldTrialName();

// Returns true if the Data Reduction Proxy config client should be used.
bool IsConfigClientEnabled();

// If the Data Reduction Proxy is used for a page load, the URL for the
// Data Reduction Proxy Pageload Metrics service.
GURL GetPingbackURL();

// If the Data Reduction Proxy config client is being used, the URL for the
// Data Reduction Proxy config service.
GURL GetConfigServiceURL();

// Returns true if the Data Reduction Proxy is forced to be enabled from the
// command line.
bool ShouldForceEnableDataReductionProxy();

// Returns true if the secure Data Reduction Proxy should be used until the
// secure proxy check fails.
bool ShouldUseSecureProxyByDefault();

// Retrieves the int stored in |param_name| from the field trial group
// |group|. If the value is not present, cannot be parsed, or is less than
// |min_value|, returns |default_value|.
int GetFieldTrialParameterAsInteger(const std::string& group,
                                    const std::string& param_name,
                                    int default_value,
                                    int min_value);

// Returns true if the list of Data Reduction Proxies to use for HTTP requests
// has been overridden on the command line, and if so, returns the override
// proxy list in |override_proxies_for_http|.
bool GetOverrideProxiesForHttpFromCommandLine(
    std::vector<net::ProxyServer>* override_proxies_for_http);

// Returns the name of the server side experiment field trial.
const char* GetServerExperimentsFieldTrialName();

}  // namespace params

// Contains information about a given proxy server. |proxies_for_http| contains
// the configured data reduction proxy servers. |proxy_index| notes the index
// of the data reduction proxy used in the list of all data reduction proxies.
struct DataReductionProxyTypeInfo {
  DataReductionProxyTypeInfo();
  DataReductionProxyTypeInfo(const DataReductionProxyTypeInfo& other);
  ~DataReductionProxyTypeInfo();
  std::vector<net::ProxyServer> proxy_servers;
  size_t proxy_index;
};

// Provides initialization parameters. Proxy origins, and the secure proxy
// check url are are taken from flags if available and from preprocessor
// constants otherwise. The DataReductionProxySettings class and others use this
// class to determine the necessary DNS names to configure use of the Data
// Reduction Proxy.
class DataReductionProxyParams : public DataReductionProxyConfigValues {
 public:
  // Flags used during construction that specify if the data reduction proxy
  // is allowed to be used, if the fallback proxy is allowed to be used, if the
  // promotion is allowed to be shown, and if this instance is part of a
  // holdback experiment.
  static const unsigned int kAllowed = (1 << 0);
  static const unsigned int kFallbackAllowed = (1 << 1);
  static const unsigned int kAllowAllProxyConfigurations =
      kAllowed | kFallbackAllowed;
  static const unsigned int kPromoAllowed = (1 << 2);
  static const unsigned int kHoldback = (1 << 3);

  // Constructs configuration parameters. If |kAllowed|, then the standard
  // data reduction proxy configuration is allowed to be used. If
  // |kfallbackAllowed| a fallback proxy can be used if the primary proxy is
  // bypassed or disabled. Finally if |kPromoAllowed|, the client may show a
  // promotion for the data reduction proxy.
  //
  // A standard configuration has a primary proxy, and a fallback proxy for
  // HTTP traffic.
  explicit DataReductionProxyParams(int flags);

  ~DataReductionProxyParams() override;

  const std::vector<net::ProxyServer>& proxies_for_http() const override;

  const GURL& secure_proxy_check_url() const override;

  bool allowed() const override;

  bool fallback_allowed() const override;

  bool promo_allowed() const override;

  bool holdback() const override;

 protected:
  // Test constructor that optionally won't call Init();
  DataReductionProxyParams(int flags,
                           bool should_call_init);

  // Initialize the values of the proxies, and secure proxy check URL, from
  // command line flags and preprocessor constants, and check that there are
  // corresponding definitions for the allowed configurations.
  bool Init(bool allowed, bool fallback_allowed);

  // Initialize the values of the proxies, and secure proxy check URL from
  // command line flags and preprocessor constants.
  void InitWithoutChecks();

  // Returns the corresponding string from preprocessor constants if defined,
  // and an empty string otherwise.
  virtual std::string GetDefaultOrigin() const;
  virtual std::string GetDefaultFallbackOrigin() const;
  virtual std::string GetDefaultSecureProxyCheckURL() const;
  virtual std::string GetDefaultWarmupURL() const;

  std::vector<net::ProxyServer> proxies_for_http_;

 private:
  net::ProxyServer origin_;
  net::ProxyServer fallback_origin_;

  GURL secure_proxy_check_url_;
  GURL warmup_url_;

  bool allowed_;
  bool fallback_allowed_;
  bool promo_allowed_;
  bool holdback_;

  bool configured_on_command_line_;

  bool use_override_proxies_for_http_;
  std::vector<net::ProxyServer> override_proxies_for_http_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyParams);
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_PARAMS_H_
