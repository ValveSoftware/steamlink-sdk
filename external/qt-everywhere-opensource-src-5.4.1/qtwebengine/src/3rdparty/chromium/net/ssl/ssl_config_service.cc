// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_config_service.h"

#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"
#include "net/ssl/ssl_config_service_defaults.h"

namespace net {

SSLConfigService::SSLConfigService()
    : observer_list_(ObserverList<Observer>::NOTIFY_EXISTING_ONLY) {
}

// GlobalCRLSet holds a reference to the global CRLSet. It simply wraps a lock
// around a scoped_refptr so that getting a reference doesn't race with
// updating the CRLSet.
class GlobalCRLSet {
 public:
  void Set(const scoped_refptr<CRLSet>& new_crl_set) {
    base::AutoLock locked(lock_);
    crl_set_ = new_crl_set;
  }

  scoped_refptr<CRLSet> Get() const {
    base::AutoLock locked(lock_);
    return crl_set_;
  }

 private:
  scoped_refptr<CRLSet> crl_set_;
  mutable base::Lock lock_;
};

base::LazyInstance<GlobalCRLSet>::Leaky g_crl_set = LAZY_INSTANCE_INITIALIZER;

// static
void SSLConfigService::SetCRLSet(scoped_refptr<CRLSet> crl_set) {
  // Note: this can be called concurently with GetCRLSet().
  g_crl_set.Get().Set(crl_set);
}

// static
scoped_refptr<CRLSet> SSLConfigService::GetCRLSet() {
  return g_crl_set.Get().Get();
}

void SSLConfigService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SSLConfigService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void SSLConfigService::NotifySSLConfigChange() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnSSLConfigChanged());
}

SSLConfigService::~SSLConfigService() {
}

void SSLConfigService::ProcessConfigUpdate(const SSLConfig& orig_config,
                                           const SSLConfig& new_config) {
  bool config_changed =
      (orig_config.rev_checking_enabled != new_config.rev_checking_enabled) ||
      (orig_config.rev_checking_required_local_anchors !=
       new_config.rev_checking_required_local_anchors) ||
      (orig_config.version_min != new_config.version_min) ||
      (orig_config.version_max != new_config.version_max) ||
      (orig_config.disabled_cipher_suites !=
       new_config.disabled_cipher_suites) ||
      (orig_config.channel_id_enabled != new_config.channel_id_enabled) ||
      (orig_config.false_start_enabled != new_config.false_start_enabled) ||
      (orig_config.require_forward_secrecy !=
       new_config.require_forward_secrecy);

  if (config_changed)
    NotifySSLConfigChange();
}

// static
bool SSLConfigService::IsSNIAvailable(SSLConfigService* service) {
  if (!service)
    return false;

  SSLConfig ssl_config;
  service->GetSSLConfig(&ssl_config);
  return ssl_config.version_max >= SSL_PROTOCOL_VERSION_TLS1;
}

}  // namespace net
