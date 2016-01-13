// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_CONFIG_SERVICE_MAC_H_
#define NET_PROXY_PROXY_CONFIG_SERVICE_MAC_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "net/base/network_config_watcher_mac.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace net {

// TODO(sergeyu): This class needs to be exported because remoting code
// creates it directly. Fix that and remove NET_EXPORT here.
// crbug.com/125104
class NET_EXPORT ProxyConfigServiceMac : public ProxyConfigService {
 public:
  // Constructs a ProxyConfigService that watches the Mac OS system settings.
  // This instance is expected to be operated and deleted on the same thread
  // (however it may be constructed from a different thread).
  explicit ProxyConfigServiceMac(
      base::SingleThreadTaskRunner* io_thread_task_runner);
  virtual ~ProxyConfigServiceMac();

 public:
  // ProxyConfigService implementation:
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual ConfigAvailability GetLatestProxyConfig(ProxyConfig* config) OVERRIDE;

 private:
  class Helper;

  // Forwarder just exists to keep the NetworkConfigWatcherMac API out of
  // ProxyConfigServiceMac's public API.
  class Forwarder : public NetworkConfigWatcherMac::Delegate {
   public:
    explicit Forwarder(ProxyConfigServiceMac* proxy_config_service)
        : proxy_config_service_(proxy_config_service) {}

    // NetworkConfigWatcherMac::Delegate implementation:
    virtual void StartReachabilityNotifications() OVERRIDE {}
    virtual void SetDynamicStoreNotificationKeys(
        SCDynamicStoreRef store) OVERRIDE;
    virtual void OnNetworkConfigChange(CFArrayRef changed_keys) OVERRIDE;

   private:
    ProxyConfigServiceMac* const proxy_config_service_;
    DISALLOW_COPY_AND_ASSIGN(Forwarder);
  };

  // Methods directly called by the NetworkConfigWatcherMac::Delegate:
  void SetDynamicStoreNotificationKeys(SCDynamicStoreRef store);
  void OnNetworkConfigChange(CFArrayRef changed_keys);

  // Called when the proxy configuration has changed, to notify the observers.
  void OnProxyConfigChanged(const ProxyConfig& new_config);

  Forwarder forwarder_;
  scoped_ptr<const NetworkConfigWatcherMac> config_watcher_;

  ObserverList<Observer> observers_;

  // Holds the last system proxy settings that we fetched.
  bool has_fetched_config_;
  ProxyConfig last_config_fetched_;

  scoped_refptr<Helper> helper_;

  // The thread that we expect to be operated on.
  const scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ProxyConfigServiceMac);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_CONFIG_SERVICE_MAC_H_
