// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_NET_ARC_NET_HOST_IMPL_H_
#define COMPONENTS_ARC_NET_ARC_NET_HOST_IMPL_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/net.mojom.h"
#include "components/arc/instance_holder.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace base {

class DictionaryValue;

}  // namespace base

namespace arc {

class ArcBridgeService;

// Private implementation of ArcNetHost.
class ArcNetHostImpl : public ArcService,
                       public InstanceHolder<mojom::NetInstance>::Observer,
                       public chromeos::NetworkStateHandlerObserver,
                       public mojom::NetHost {
 public:
  // The constructor will register an Observer with ArcBridgeService.
  explicit ArcNetHostImpl(ArcBridgeService* arc_bridge_service);
  ~ArcNetHostImpl() override;

  // ARC -> Chrome calls:

  void GetNetworksDeprecated(
      bool configured_only,
      bool visible_only,
      const GetNetworksDeprecatedCallback& callback) override;

  void GetNetworks(mojom::GetNetworksRequestType type,
                   const GetNetworksCallback& callback) override;

  void GetWifiEnabledState(
      const GetWifiEnabledStateCallback& callback) override;

  void SetWifiEnabledState(
      bool is_enabled,
      const SetWifiEnabledStateCallback& callback) override;

  void StartScan() override;

  void CreateNetwork(mojom::WifiConfigurationPtr cfg,
                     const CreateNetworkCallback& callback) override;

  void ForgetNetwork(const mojo::String& guid,
                     const ForgetNetworkCallback& callback) override;

  void StartConnect(const mojo::String& guid,
                    const StartConnectCallback& callback) override;

  void StartDisconnect(const mojo::String& guid,
                       const StartDisconnectCallback& callback) override;

  // Overriden from chromeos::NetworkStateHandlerObserver.
  void ScanCompleted(const chromeos::DeviceState* /*unused*/) override;
  void OnShuttingDown() override;
  void DefaultNetworkChanged(const chromeos::NetworkState* network) override;
  void DeviceListChanged() override;
  void GetDefaultNetwork(const GetDefaultNetworkCallback& callback) override;

  // Overridden from ArcBridgeService::InterfaceObserver<mojom::NetInstance>:
  void OnInstanceReady() override;

 private:
  void DefaultNetworkSuccessCallback(const std::string& service_path,
                                     const base::DictionaryValue& dictionary);

  // Due to a race in Chrome, GetNetworkStateFromGuid() might not know about
  // newly created networks, as that function relies on the completion of a
  // separate GetProperties shill call that completes asynchronously.  So this
  // class keeps a local cache of the path->guid mapping as a fallback.
  // This is sufficient to pass CTS but it might not handle multiple
  // successive Create operations (crbug.com/631646).
  bool GetNetworkPathFromGuid(const std::string& guid, std::string* path);

  void CreateNetworkSuccessCallback(
      const arc::mojom::NetHost::CreateNetworkCallback& mojo_callback,
      const std::string& service_path,
      const std::string& guid);

  void CreateNetworkFailureCallback(
      const arc::mojom::NetHost::CreateNetworkCallback& mojo_callback,
      const std::string& error_name,
      std::unique_ptr<base::DictionaryValue> error_data);

  std::string cached_service_path_;
  std::string cached_guid_;

  base::ThreadChecker thread_checker_;
  mojo::Binding<arc::mojom::NetHost> binding_;
  base::WeakPtrFactory<ArcNetHostImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcNetHostImpl);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_NET_ARC_NET_HOST_IMPL_H_
