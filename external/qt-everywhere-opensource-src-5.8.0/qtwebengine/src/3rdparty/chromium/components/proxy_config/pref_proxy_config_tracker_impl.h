// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXY_CONFIG_PREF_PROXY_CONFIG_TRACKER_IMPL_H_
#define COMPONENTS_PROXY_CONFIG_PREF_PROXY_CONFIG_TRACKER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/proxy_config/pref_proxy_config_tracker.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service.h"

class PrefService;
class PrefRegistrySimple;

namespace base {
class SingleThreadTaskRunner;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// A net::ProxyConfigService implementation that applies preference proxy
// settings (pushed from PrefProxyConfigTrackerImpl) as overrides to the proxy
// configuration determined by a baseline delegate ProxyConfigService on
// non-ChromeOS platforms. ChromeOS has its own implementation of overrides in
// chromeos::ProxyConfigServiceImpl.
class ProxyConfigServiceImpl : public net::ProxyConfigService,
                               public net::ProxyConfigService::Observer {
 public:
  // Takes ownership of the passed |base_service|.
  // GetLatestProxyConfig returns ConfigAvailability::CONFIG_PENDING until
  // UpdateProxyConfig has been called.
  explicit ProxyConfigServiceImpl(net::ProxyConfigService* base_service);
  ~ProxyConfigServiceImpl() override;

  // ProxyConfigService implementation:
  void AddObserver(net::ProxyConfigService::Observer* observer) override;
  void RemoveObserver(net::ProxyConfigService::Observer* observer) override;
  ConfigAvailability GetLatestProxyConfig(net::ProxyConfig* config) override;
  void OnLazyPoll() override;

  // Method on IO thread that receives the preference proxy settings pushed from
  // PrefProxyConfigTrackerImpl.
  void UpdateProxyConfig(ProxyPrefs::ConfigState config_state,
                         const net::ProxyConfig& config);

 private:
  // ProxyConfigService::Observer implementation:
  void OnProxyConfigChanged(const net::ProxyConfig& config,
                            ConfigAvailability availability) override;

  // Makes sure that the observer registration with the base service is set up.
  void RegisterObserver();

  std::unique_ptr<net::ProxyConfigService> base_service_;
  base::ObserverList<net::ProxyConfigService::Observer, true> observers_;

  // Tracks configuration state of |pref_config_|. |pref_config_| is valid only
  // if |pref_config_state_| is not CONFIG_UNSET.
  ProxyPrefs::ConfigState pref_config_state_;

  // Configuration as defined by prefs.
  net::ProxyConfig pref_config_;

  // Flag that indicates that a PrefProxyConfigTracker needs to inform us
  // about a proxy configuration before we may return any configuration.
  bool pref_config_read_pending_;

  // Indicates whether the base service registration is done.
  bool registered_observer_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ProxyConfigServiceImpl);
};

// A class that tracks proxy preferences. It translates the configuration
// to net::ProxyConfig and pushes the result over to the IO thread for
// ProxyConfigServiceImpl::UpdateProxyConfig to use.
class PROXY_CONFIG_EXPORT PrefProxyConfigTrackerImpl
    : public PrefProxyConfigTracker {
 public:
  PrefProxyConfigTrackerImpl(
      PrefService* pref_service,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~PrefProxyConfigTrackerImpl() override;

  // PrefProxyConfigTracker implementation:
  std::unique_ptr<net::ProxyConfigService> CreateTrackingProxyConfigService(
      std::unique_ptr<net::ProxyConfigService> base_service) override;

  // Notifies the tracker that the pref service passed upon construction is
  // about to go away. This must be called from the UI thread.
  void DetachFromPrefService() override;

  // Determines if |config_state| takes precedence regardless, which happens if
  // config is from policy or extension or other-precede.
  static bool PrefPrecedes(ProxyPrefs::ConfigState config_state);

  // Determines the proxy configuration that should take effect in the network
  // layer, based on prefs and system configurations.
  // |pref_state| refers to state of |pref_config|.
  // |system_availability| refers to availability of |system_config|.
  // |ignore_fallback_config| indicates if fallback config from prefs should
  // be ignored.
  // Returns effective |effective_config| and its state in
  // |effective_config_source|.
  static net::ProxyConfigService::ConfigAvailability GetEffectiveProxyConfig(
      ProxyPrefs::ConfigState pref_state,
      const net::ProxyConfig& pref_config,
      net::ProxyConfigService::ConfigAvailability system_availability,
      const net::ProxyConfig& system_config,
      bool ignore_fallback_config,
      ProxyPrefs::ConfigState* effective_config_state,
      net::ProxyConfig* effective_config);

  // Converts a ProxyConfigDictionary to net::ProxyConfig representation.
  // Returns true if the data from in the dictionary is valid, false otherwise.
  static bool PrefConfigToNetConfig(const ProxyConfigDictionary& proxy_dict,
                                    net::ProxyConfig* config);

  // Registers the proxy preferences. These are actually registered
  // the same way in local state and in user prefs.
  static void RegisterPrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Creates a proxy configuration from proxy-related preferences of
  // |pref_service|. Configuration is stored in |config|, return value indicates
  // whether the configuration is valid.
  static ProxyPrefs::ConfigState ReadPrefConfig(const PrefService* pref_service,
                                                net::ProxyConfig* config);

 protected:
  // Get the proxy configuration currently defined by preferences.
  // Status is indicated in the return value.
  // Writes the configuration to |config| unless the return value is
  // CONFIG_UNSET, in which case |config| and |config_source| are not touched.
  ProxyPrefs::ConfigState GetProxyConfig(net::ProxyConfig* config);

  // Called when there's a change in prefs proxy config.
  // Subclasses can extend it for changes in other sources of proxy config.
  virtual void OnProxyConfigChanged(ProxyPrefs::ConfigState config_state,
                                    const net::ProxyConfig& config);

  void OnProxyPrefChanged();

  const PrefService* prefs() const { return pref_service_; }
  bool update_pending() const { return update_pending_; }

 private:
  // Tracks configuration state. |pref_config_| is valid only if |config_state_|
  // is not CONFIG_UNSET.
  ProxyPrefs::ConfigState config_state_;

  // Configuration as defined by prefs.
  net::ProxyConfig pref_config_;

  PrefService* pref_service_;
  ProxyConfigServiceImpl* proxy_config_service_impl_;  // Weak ptr.
  bool update_pending_;  // True if config has not been pushed to network stack.
  PrefChangeRegistrar proxy_prefs_;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(PrefProxyConfigTrackerImpl);
};

#endif  // COMPONENTS_PROXY_CONFIG_PREF_PROXY_CONFIG_TRACKER_IMPL_H_
