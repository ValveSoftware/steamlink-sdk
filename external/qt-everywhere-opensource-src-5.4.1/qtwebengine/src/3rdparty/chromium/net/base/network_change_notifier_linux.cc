// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_linux.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/threading/thread.h"
#include "net/base/address_tracker_linux.h"
#include "net/dns/dns_config_service.h"

namespace net {

class NetworkChangeNotifierLinux::Thread : public base::Thread {
 public:
  Thread();
  virtual ~Thread();

  // Plumbing for NetworkChangeNotifier::GetCurrentConnectionType.
  // Safe to call from any thread.
  NetworkChangeNotifier::ConnectionType GetCurrentConnectionType() {
    return address_tracker_.GetCurrentConnectionType();
  }

  const internal::AddressTrackerLinux* address_tracker() const {
    return &address_tracker_;
  }

 protected:
  // base::Thread
  virtual void Init() OVERRIDE;
  virtual void CleanUp() OVERRIDE;

 private:
  scoped_ptr<DnsConfigService> dns_config_service_;
  // Used to detect online/offline state and IP address changes.
  internal::AddressTrackerLinux address_tracker_;

  DISALLOW_COPY_AND_ASSIGN(Thread);
};

NetworkChangeNotifierLinux::Thread::Thread()
    : base::Thread("NetworkChangeNotifier"),
      address_tracker_(
          base::Bind(&NetworkChangeNotifier::
                     NotifyObserversOfIPAddressChange),
          base::Bind(&NetworkChangeNotifier::
                     NotifyObserversOfConnectionTypeChange),
          base::Bind(base::DoNothing)) {
}

NetworkChangeNotifierLinux::Thread::~Thread() {
  DCHECK(!Thread::IsRunning());
}

void NetworkChangeNotifierLinux::Thread::Init() {
  address_tracker_.Init();
  dns_config_service_ = DnsConfigService::CreateSystemService();
  dns_config_service_->WatchConfig(
      base::Bind(&NetworkChangeNotifier::SetDnsConfig));
}

void NetworkChangeNotifierLinux::Thread::CleanUp() {
  dns_config_service_.reset();
}

NetworkChangeNotifierLinux* NetworkChangeNotifierLinux::Create() {
  return new NetworkChangeNotifierLinux();
}

NetworkChangeNotifierLinux::NetworkChangeNotifierLinux()
    : NetworkChangeNotifier(NetworkChangeCalculatorParamsLinux()),
      notifier_thread_(new Thread()) {
  // We create this notifier thread because the notification implementation
  // needs a MessageLoopForIO, and there's no guarantee that
  // MessageLoop::current() meets that criterion.
  base::Thread::Options thread_options(base::MessageLoop::TYPE_IO, 0);
  notifier_thread_->StartWithOptions(thread_options);
}

NetworkChangeNotifierLinux::~NetworkChangeNotifierLinux() {
  // Stopping from here allows us to sanity- check that the notifier
  // thread shut down properly.
  notifier_thread_->Stop();
}

// static
NetworkChangeNotifier::NetworkChangeCalculatorParams
NetworkChangeNotifierLinux::NetworkChangeCalculatorParamsLinux() {
  NetworkChangeCalculatorParams params;
  // Delay values arrived at by simple experimentation and adjusted so as to
  // produce a single signal when switching between network connections.
  params.ip_address_offline_delay_ = base::TimeDelta::FromMilliseconds(2000);
  params.ip_address_online_delay_ = base::TimeDelta::FromMilliseconds(2000);
  params.connection_type_offline_delay_ =
      base::TimeDelta::FromMilliseconds(1500);
  params.connection_type_online_delay_ = base::TimeDelta::FromMilliseconds(500);
  return params;
}

NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierLinux::GetCurrentConnectionType() const {
  return notifier_thread_->GetCurrentConnectionType();
}

const internal::AddressTrackerLinux*
NetworkChangeNotifierLinux::GetAddressTrackerInternal() const {
  return notifier_thread_->address_tracker();
}

}  // namespace net
