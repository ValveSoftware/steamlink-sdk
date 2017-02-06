// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/net/network_metrics_provider.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task_runner_util.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"

#if defined(OS_CHROMEOS)
#include "components/metrics/net/wifi_access_point_info_provider_chromeos.h"
#endif  // OS_CHROMEOS

namespace metrics {

NetworkMetricsProvider::NetworkMetricsProvider(base::TaskRunner* io_task_runner)
    : io_task_runner_(io_task_runner),
      connection_type_is_ambiguous_(false),
      wifi_phy_layer_protocol_is_ambiguous_(false),
      wifi_phy_layer_protocol_(net::WIFI_PHY_LAYER_PROTOCOL_UNKNOWN),
      total_aborts_(0),
      total_codes_(0),
      weak_ptr_factory_(this) {
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
  connection_type_ = net::NetworkChangeNotifier::GetConnectionType();
  ProbeWifiPHYLayerProtocol();
}

NetworkMetricsProvider::~NetworkMetricsProvider() {
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

void NetworkMetricsProvider::ProvideGeneralMetrics(
    ChromeUserMetricsExtension*) {
  // ProvideGeneralMetrics is called on the main thread, at the time a metrics
  // record is being finalized.
  net::NetworkChangeNotifier::FinalizingMetricsLogRecord();
  LogAggregatedMetrics();
}

void NetworkMetricsProvider::ProvideSystemProfileMetrics(
    SystemProfileProto* system_profile) {
  SystemProfileProto::Network* network = system_profile->mutable_network();
  network->set_connection_type_is_ambiguous(connection_type_is_ambiguous_);
  network->set_connection_type(GetConnectionType());
  network->set_wifi_phy_layer_protocol_is_ambiguous(
      wifi_phy_layer_protocol_is_ambiguous_);
  network->set_wifi_phy_layer_protocol(GetWifiPHYLayerProtocol());

  // Update the connection type. Note that this is necessary to set the network
  // type to "none" if there is no network connection for an entire UMA logging
  // window, since OnConnectionTypeChanged() ignores transitions to the "none"
  // state.
  connection_type_ = net::NetworkChangeNotifier::GetConnectionType();
  // Reset the "ambiguous" flags, since a new metrics log session has started.
  connection_type_is_ambiguous_ = false;
  wifi_phy_layer_protocol_is_ambiguous_ = false;

  if (!wifi_access_point_info_provider_.get()) {
#if defined(OS_CHROMEOS)
    wifi_access_point_info_provider_.reset(
        new WifiAccessPointInfoProviderChromeos());
#else
    wifi_access_point_info_provider_.reset(
        new WifiAccessPointInfoProvider());
#endif  // OS_CHROMEOS
  }

  // Connected wifi access point information.
  WifiAccessPointInfoProvider::WifiAccessPointInfo info;
  if (wifi_access_point_info_provider_->GetInfo(&info))
    WriteWifiAccessPointProto(info, network);
}

void NetworkMetricsProvider::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  // To avoid reporting an ambiguous connection type for users on flaky
  // connections, ignore transitions to the "none" state. Note that the
  // connection type is refreshed in ProvideSystemProfileMetrics() each time a
  // new UMA logging window begins, so users who genuinely transition to offline
  // mode for an extended duration will still be at least partially represented
  // in the metrics logs.
  if (type == net::NetworkChangeNotifier::CONNECTION_NONE)
    return;

  if (type != connection_type_ &&
      connection_type_ != net::NetworkChangeNotifier::CONNECTION_NONE) {
    connection_type_is_ambiguous_ = true;
  }
  connection_type_ = type;

  ProbeWifiPHYLayerProtocol();
}

SystemProfileProto::Network::ConnectionType
NetworkMetricsProvider::GetConnectionType() const {
  switch (connection_type_) {
    case net::NetworkChangeNotifier::CONNECTION_NONE:
      return SystemProfileProto::Network::CONNECTION_NONE;
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
      return SystemProfileProto::Network::CONNECTION_UNKNOWN;
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
      return SystemProfileProto::Network::CONNECTION_ETHERNET;
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
      return SystemProfileProto::Network::CONNECTION_WIFI;
    case net::NetworkChangeNotifier::CONNECTION_2G:
      return SystemProfileProto::Network::CONNECTION_2G;
    case net::NetworkChangeNotifier::CONNECTION_3G:
      return SystemProfileProto::Network::CONNECTION_3G;
    case net::NetworkChangeNotifier::CONNECTION_4G:
      return SystemProfileProto::Network::CONNECTION_4G;
    case net::NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      return SystemProfileProto::Network::CONNECTION_BLUETOOTH;
  }
  NOTREACHED();
  return SystemProfileProto::Network::CONNECTION_UNKNOWN;
}

SystemProfileProto::Network::WifiPHYLayerProtocol
NetworkMetricsProvider::GetWifiPHYLayerProtocol() const {
  switch (wifi_phy_layer_protocol_) {
    case net::WIFI_PHY_LAYER_PROTOCOL_NONE:
      return SystemProfileProto::Network::WIFI_PHY_LAYER_PROTOCOL_NONE;
    case net::WIFI_PHY_LAYER_PROTOCOL_ANCIENT:
      return SystemProfileProto::Network::WIFI_PHY_LAYER_PROTOCOL_ANCIENT;
    case net::WIFI_PHY_LAYER_PROTOCOL_A:
      return SystemProfileProto::Network::WIFI_PHY_LAYER_PROTOCOL_A;
    case net::WIFI_PHY_LAYER_PROTOCOL_B:
      return SystemProfileProto::Network::WIFI_PHY_LAYER_PROTOCOL_B;
    case net::WIFI_PHY_LAYER_PROTOCOL_G:
      return SystemProfileProto::Network::WIFI_PHY_LAYER_PROTOCOL_G;
    case net::WIFI_PHY_LAYER_PROTOCOL_N:
      return SystemProfileProto::Network::WIFI_PHY_LAYER_PROTOCOL_N;
    case net::WIFI_PHY_LAYER_PROTOCOL_UNKNOWN:
      return SystemProfileProto::Network::WIFI_PHY_LAYER_PROTOCOL_UNKNOWN;
  }
  NOTREACHED();
  return SystemProfileProto::Network::WIFI_PHY_LAYER_PROTOCOL_UNKNOWN;
}

void NetworkMetricsProvider::ProbeWifiPHYLayerProtocol() {
  PostTaskAndReplyWithResult(
      io_task_runner_,
      FROM_HERE,
      base::Bind(&net::GetWifiPHYLayerProtocol),
      base::Bind(&NetworkMetricsProvider::OnWifiPHYLayerProtocolResult,
                 weak_ptr_factory_.GetWeakPtr()));
}

void NetworkMetricsProvider::OnWifiPHYLayerProtocolResult(
    net::WifiPHYLayerProtocol mode) {
  if (wifi_phy_layer_protocol_ != net::WIFI_PHY_LAYER_PROTOCOL_UNKNOWN &&
      mode != wifi_phy_layer_protocol_) {
    wifi_phy_layer_protocol_is_ambiguous_ = true;
  }
  wifi_phy_layer_protocol_ = mode;
}

void NetworkMetricsProvider::WriteWifiAccessPointProto(
    const WifiAccessPointInfoProvider::WifiAccessPointInfo& info,
    SystemProfileProto::Network* network_proto) {
  SystemProfileProto::Network::WifiAccessPoint* access_point_info =
      network_proto->mutable_access_point_info();
  SystemProfileProto::Network::WifiAccessPoint::SecurityMode security =
      SystemProfileProto::Network::WifiAccessPoint::SECURITY_UNKNOWN;
  switch (info.security) {
    case WifiAccessPointInfoProvider::WIFI_SECURITY_NONE:
      security = SystemProfileProto::Network::WifiAccessPoint::SECURITY_NONE;
      break;
    case WifiAccessPointInfoProvider::WIFI_SECURITY_WPA:
      security = SystemProfileProto::Network::WifiAccessPoint::SECURITY_WPA;
      break;
    case WifiAccessPointInfoProvider::WIFI_SECURITY_WEP:
      security = SystemProfileProto::Network::WifiAccessPoint::SECURITY_WEP;
      break;
    case WifiAccessPointInfoProvider::WIFI_SECURITY_RSN:
      security = SystemProfileProto::Network::WifiAccessPoint::SECURITY_RSN;
      break;
    case WifiAccessPointInfoProvider::WIFI_SECURITY_802_1X:
      security = SystemProfileProto::Network::WifiAccessPoint::SECURITY_802_1X;
      break;
    case WifiAccessPointInfoProvider::WIFI_SECURITY_PSK:
      security = SystemProfileProto::Network::WifiAccessPoint::SECURITY_PSK;
      break;
    case WifiAccessPointInfoProvider::WIFI_SECURITY_UNKNOWN:
      security = SystemProfileProto::Network::WifiAccessPoint::SECURITY_UNKNOWN;
      break;
  }
  access_point_info->set_security_mode(security);

  // |bssid| is xx:xx:xx:xx:xx:xx, extract the first three components and
  // pack into a uint32_t.
  std::string bssid = info.bssid;
  if (bssid.size() == 17 && bssid[2] == ':' && bssid[5] == ':' &&
      bssid[8] == ':' && bssid[11] == ':' && bssid[14] == ':') {
    std::string vendor_prefix_str;
    uint32_t vendor_prefix;

    base::RemoveChars(bssid.substr(0, 9), ":", &vendor_prefix_str);
    DCHECK_EQ(6U, vendor_prefix_str.size());
    if (base::HexStringToUInt(vendor_prefix_str, &vendor_prefix))
      access_point_info->set_vendor_prefix(vendor_prefix);
    else
      NOTREACHED();
  }

  // Return if vendor information is not provided.
  if (info.model_number.empty() && info.model_name.empty() &&
      info.device_name.empty() && info.oui_list.empty())
    return;

  SystemProfileProto::Network::WifiAccessPoint::VendorInformation* vendor =
      access_point_info->mutable_vendor_info();
  if (!info.model_number.empty())
    vendor->set_model_number(info.model_number);
  if (!info.model_name.empty())
    vendor->set_model_name(info.model_name);
  if (!info.device_name.empty())
    vendor->set_device_name(info.device_name);

  // Return if OUI list is not provided.
  if (info.oui_list.empty())
    return;

  // Parse OUI list.
  for (const base::StringPiece& oui_str : base::SplitStringPiece(
           info.oui_list, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    uint32_t oui;
    if (base::HexStringToUInt(oui_str, &oui))
      vendor->add_element_identifier(oui);
    else
      NOTREACHED();
  }
}

void NetworkMetricsProvider::LogAggregatedMetrics() {
  base::HistogramBase* error_codes = base::SparseHistogram::FactoryGet(
      "Net.ErrorCodesForMainFrame3",
      base::HistogramBase::kUmaTargetedHistogramFlag);
  std::unique_ptr<base::HistogramSamples> samples =
      error_codes->SnapshotSamples();
  base::HistogramBase::Count new_aborts =
      samples->GetCount(-net::ERR_ABORTED) - total_aborts_;
  base::HistogramBase::Count new_codes = samples->TotalCount() - total_codes_;
  if (new_codes > 0) {
    UMA_HISTOGRAM_COUNTS("Net.ErrAborted.CountPerUpload", new_aborts);
    UMA_HISTOGRAM_PERCENTAGE("Net.ErrAborted.ProportionPerUpload",
                             (100 * new_aborts) / new_codes);
    total_codes_ += new_codes;
    total_aborts_ += new_aborts;
  }
}

}  // namespace metrics
