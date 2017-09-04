// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_NET_NETWORK_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_NET_NETWORK_METRICS_PROVIDER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_base.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/net/wifi_access_point_info_provider.h"
#include "components/metrics/proto/system_profile.pb.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"

namespace metrics {

// Registers as observer with net::NetworkChangeNotifier and keeps track of
// the network environment.
class NetworkMetricsProvider
    : public MetricsProvider,
      public net::NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  // Creates a NetworkMetricsProvider, where |io_task_runner| is used to post
  // network info collection tasks.
  explicit NetworkMetricsProvider(base::TaskRunner* io_task_runner);
  ~NetworkMetricsProvider() override;

 private:
  // MetricsProvider:
  void ProvideGeneralMetrics(ChromeUserMetricsExtension* uma_proto) override;
  void ProvideSystemProfileMetrics(SystemProfileProto* system_profile) override;

  // ConnectionTypeObserver:
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  SystemProfileProto::Network::ConnectionType GetConnectionType() const;
  SystemProfileProto::Network::WifiPHYLayerProtocol GetWifiPHYLayerProtocol()
      const;

  // Posts a call to net::GetWifiPHYLayerProtocol on the blocking pool.
  void ProbeWifiPHYLayerProtocol();
  // Callback from the blocking pool with the result of
  // net::GetWifiPHYLayerProtocol.
  void OnWifiPHYLayerProtocolResult(net::WifiPHYLayerProtocol mode);

  // Writes info about the wireless access points that this system is
  // connected to.
  void WriteWifiAccessPointProto(
      const WifiAccessPointInfoProvider::WifiAccessPointInfo& info,
      SystemProfileProto::Network* network_proto);

  // Logs metrics that are functions of other metrics being uploaded.
  void LogAggregatedMetrics();

  // Task runner used for blocking file I/O.
  base::TaskRunner* io_task_runner_;

  // True if |connection_type_| changed during the lifetime of the log.
  bool connection_type_is_ambiguous_;
  // The connection type according to net::NetworkChangeNotifier.
  net::NetworkChangeNotifier::ConnectionType connection_type_;

  // True if |wifi_phy_layer_protocol_| changed during the lifetime of the log.
  bool wifi_phy_layer_protocol_is_ambiguous_;
  // The PHY mode of the currently associated access point obtained via
  // net::GetWifiPHYLayerProtocol.
  net::WifiPHYLayerProtocol wifi_phy_layer_protocol_;

  // Helper object for retrieving connected wifi access point information.
  std::unique_ptr<WifiAccessPointInfoProvider> wifi_access_point_info_provider_;

  // These metrics track histogram totals for the Net.ErrorCodesForMainFrame3
  // histogram. They are used to compute deltas at upload time.
  base::HistogramBase::Count total_aborts_;
  base::HistogramBase::Count total_codes_;

  base::WeakPtrFactory<NetworkMetricsProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkMetricsProvider);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_NET_NETWORK_METRICS_PROVIDER_H_
