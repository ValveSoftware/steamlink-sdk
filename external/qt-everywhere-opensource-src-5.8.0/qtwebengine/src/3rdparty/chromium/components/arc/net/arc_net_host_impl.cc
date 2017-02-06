// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/net/arc_net_host_impl.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/network/network_util.h"
#include "chromeos/network/onc/onc_utils.h"
#include "components/arc/arc_bridge_service.h"

namespace {

const int kGetNetworksListLimit = 100;

chromeos::NetworkStateHandler* GetStateHandler() {
  return chromeos::NetworkHandler::Get()->network_state_handler();
}

chromeos::ManagedNetworkConfigurationHandler* GetManagedConfigurationHandler() {
  return chromeos::NetworkHandler::Get()
      ->managed_network_configuration_handler();
}

chromeos::NetworkConnectionHandler* GetNetworkConnectionHandler() {
  return chromeos::NetworkHandler::Get()->network_connection_handler();
}

bool IsDeviceOwner() {
  // Check whether the logged-in Chrome OS user is allowed to add or
  // remove WiFi networks.
  return chromeos::LoginState::Get()->GetLoggedInUserType() ==
         chromeos::LoginState::LOGGED_IN_USER_OWNER;
}

std::string GetStringFromOncDictionary(const base::DictionaryValue* dict,
                                       const char* key,
                                       bool required) {
  std::string value;
  dict->GetString(key, &value);
  if (required && value.empty())
    NOTREACHED();
  return value;
}

arc::mojom::SecurityType TranslateONCWifiSecurityType(
    const base::DictionaryValue* dict) {
  std::string type = GetStringFromOncDictionary(dict, onc::wifi::kSecurity,
                                                true /* required */);
  if (type == onc::wifi::kWEP_PSK)
    return arc::mojom::SecurityType::WEP_PSK;
  else if (type == onc::wifi::kWEP_8021X)
    return arc::mojom::SecurityType::WEP_8021X;
  else if (type == onc::wifi::kWPA_PSK)
    return arc::mojom::SecurityType::WPA_PSK;
  else if (type == onc::wifi::kWPA_EAP)
    return arc::mojom::SecurityType::WPA_EAP;
  else
    return arc::mojom::SecurityType::NONE;
}

arc::mojom::WiFiPtr TranslateONCWifi(const base::DictionaryValue* dict) {
  arc::mojom::WiFiPtr wifi = arc::mojom::WiFi::New();

  // Optional; defaults to 0.
  dict->GetInteger(onc::wifi::kFrequency, &wifi->frequency);

  wifi->bssid =
      GetStringFromOncDictionary(dict, onc::wifi::kBSSID, false /* required */);
  wifi->hex_ssid = GetStringFromOncDictionary(dict, onc::wifi::kHexSSID,
                                              true /* required */);

  // Optional; defaults to false.
  dict->GetBoolean(onc::wifi::kHiddenSSID, &wifi->hidden_ssid);

  wifi->security = TranslateONCWifiSecurityType(dict);

  // Optional; defaults to 0.
  dict->GetInteger(onc::wifi::kSignalStrength, &wifi->signal_strength);

  return wifi;
}

mojo::Array<mojo::String> TranslateStringArray(const base::ListValue* list) {
  mojo::Array<mojo::String> strings = mojo::Array<mojo::String>::New(0);

  for (size_t i = 0; i < list->GetSize(); i++) {
    std::string value;
    list->GetString(i, &value);
    DCHECK(!value.empty());
    strings.push_back(static_cast<mojo::String>(value));
  }

  return strings;
}

mojo::Array<arc::mojom::IPConfigurationPtr> TranslateONCIPConfigs(
    const base::ListValue* list) {
  mojo::Array<arc::mojom::IPConfigurationPtr> configs =
      mojo::Array<arc::mojom::IPConfigurationPtr>::New(0);

  for (size_t i = 0; i < list->GetSize(); i++) {
    const base::DictionaryValue* ip_dict = nullptr;
    arc::mojom::IPConfigurationPtr configuration =
        arc::mojom::IPConfiguration::New();

    list->GetDictionary(i, &ip_dict);
    DCHECK(ip_dict);

    // crbug.com/625229 - Gateway is not always present (but it should be).
    configuration->gateway = GetStringFromOncDictionary(
        ip_dict, onc::ipconfig::kGateway, false /* required */);
    configuration->ip_address = GetStringFromOncDictionary(
        ip_dict, onc::ipconfig::kIPAddress, true /* required */);

    const base::ListValue* dns_list;
    if (!ip_dict->GetList(onc::ipconfig::kNameServers, &dns_list))
      NOTREACHED();
    configuration->name_servers = TranslateStringArray(dns_list);

    if (!ip_dict->GetInteger(onc::ipconfig::kRoutingPrefix,
                             &configuration->routing_prefix)) {
      NOTREACHED();
    }

    std::string type = GetStringFromOncDictionary(ip_dict, onc::ipconfig::kType,
                                                  true /* required */);
    configuration->type = type == onc::ipconfig::kIPv6
                              ? arc::mojom::IPAddressType::IPV6
                              : arc::mojom::IPAddressType::IPV4;

    configuration->web_proxy_auto_discovery_url = GetStringFromOncDictionary(
        ip_dict, onc::ipconfig::kWebProxyAutoDiscoveryUrl,
        false /* required */);

    configs.push_back(std::move(configuration));
  }
  return configs;
}

arc::mojom::ConnectionStateType TranslateONCConnectionState(
    const base::DictionaryValue* dict) {
  std::string connection_state = GetStringFromOncDictionary(
      dict, onc::network_config::kConnectionState, true /* required */);

  if (connection_state == onc::connection_state::kConnected)
    return arc::mojom::ConnectionStateType::CONNECTED;
  else if (connection_state == onc::connection_state::kConnecting)
    return arc::mojom::ConnectionStateType::CONNECTING;
  else if (connection_state == onc::connection_state::kNotConnected)
    return arc::mojom::ConnectionStateType::NOT_CONNECTED;

  NOTREACHED();
  return arc::mojom::ConnectionStateType::NOT_CONNECTED;
}

void TranslateONCNetworkTypeDetails(const base::DictionaryValue* dict,
                                    arc::mojom::NetworkConfiguration* mojo) {
  std::string type = GetStringFromOncDictionary(
      dict, onc::network_config::kType, true /* required */);
  if (type == onc::network_type::kCellular) {
    mojo->type = arc::mojom::NetworkType::CELLULAR;
  } else if (type == onc::network_type::kEthernet) {
    mojo->type = arc::mojom::NetworkType::ETHERNET;
  } else if (type == onc::network_type::kVPN) {
    mojo->type = arc::mojom::NetworkType::VPN;
  } else if (type == onc::network_type::kWiFi) {
    mojo->type = arc::mojom::NetworkType::WIFI;

    const base::DictionaryValue* wifi_dict = nullptr;
    dict->GetDictionary(onc::network_config::kWiFi, &wifi_dict);
    DCHECK(wifi_dict);
    mojo->wifi = TranslateONCWifi(wifi_dict);
  } else if (type == onc::network_type::kWimax) {
    mojo->type = arc::mojom::NetworkType::WIMAX;
  } else {
    NOTREACHED();
  }
}

arc::mojom::NetworkConfigurationPtr TranslateONCConfiguration(
    const base::DictionaryValue* dict) {
  arc::mojom::NetworkConfigurationPtr mojo =
      arc::mojom::NetworkConfiguration::New();

  mojo->connection_state = TranslateONCConnectionState(dict);

  mojo->guid = GetStringFromOncDictionary(dict, onc::network_config::kGUID,
                                          true /* required */);

  const base::ListValue* ip_config_list = nullptr;
  if (dict->GetList(onc::network_config::kIPConfigs, &ip_config_list)) {
    DCHECK(ip_config_list);
    mojo->ip_configs = TranslateONCIPConfigs(ip_config_list);
  }

  mojo->guid = GetStringFromOncDictionary(dict, onc::network_config::kGUID,
                                          true /* required */);
  mojo->mac_address = GetStringFromOncDictionary(
      dict, onc::network_config::kMacAddress, true /* required */);
  TranslateONCNetworkTypeDetails(dict, mojo.get());

  return mojo;
}

void ForgetNetworkSuccessCallback(
    const arc::mojom::NetHost::ForgetNetworkCallback& mojo_callback) {
  mojo_callback.Run(arc::mojom::NetworkResult::SUCCESS);
}

void ForgetNetworkFailureCallback(
    const arc::mojom::NetHost::ForgetNetworkCallback& mojo_callback,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  VLOG(1) << "ForgetNetworkFailureCallback: " << error_name;
  mojo_callback.Run(arc::mojom::NetworkResult::FAILURE);
}

void StartConnectSuccessCallback(
    const arc::mojom::NetHost::StartConnectCallback& mojo_callback) {
  mojo_callback.Run(arc::mojom::NetworkResult::SUCCESS);
}

void StartConnectFailureCallback(
    const arc::mojom::NetHost::StartConnectCallback& mojo_callback,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  VLOG(1) << "StartConnectFailureCallback: " << error_name;
  mojo_callback.Run(arc::mojom::NetworkResult::FAILURE);
}

void StartDisconnectSuccessCallback(
    const arc::mojom::NetHost::StartDisconnectCallback& mojo_callback) {
  mojo_callback.Run(arc::mojom::NetworkResult::SUCCESS);
}

void StartDisconnectFailureCallback(
    const arc::mojom::NetHost::StartDisconnectCallback& mojo_callback,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  VLOG(1) << "StartDisconnectFailureCallback: " << error_name;
  mojo_callback.Run(arc::mojom::NetworkResult::FAILURE);
}

void GetDefaultNetworkSuccessCallback(
    const arc::ArcNetHostImpl::GetDefaultNetworkCallback& callback,
    const std::string& service_path,
    const base::DictionaryValue& dictionary) {
  // TODO(cernekee): Figure out how to query Chrome for the default physical
  // service if a VPN is connected, rather than just reporting the
  // default logical service in both fields.
  callback.Run(TranslateONCConfiguration(&dictionary),
               TranslateONCConfiguration(&dictionary));
}

void GetDefaultNetworkFailureCallback(
    const arc::ArcNetHostImpl::GetDefaultNetworkCallback& callback,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  LOG(ERROR) << "Failed to query default logical network";
  callback.Run(nullptr, nullptr);
}

void DefaultNetworkFailureCallback(
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  LOG(ERROR) << "Failed to query default logical network";
}

}  // namespace

namespace arc {

ArcNetHostImpl::ArcNetHostImpl(ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this), weak_factory_(this) {
  arc_bridge_service()->net()->AddObserver(this);
  GetStateHandler()->AddObserver(this, FROM_HERE);
}

ArcNetHostImpl::~ArcNetHostImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  arc_bridge_service()->net()->RemoveObserver(this);
  if (chromeos::NetworkHandler::IsInitialized()) {
    GetStateHandler()->RemoveObserver(this, FROM_HERE);
  }
}

void ArcNetHostImpl::OnInstanceReady() {
  DCHECK(thread_checker_.CalledOnValidThread());

  mojom::NetHostPtr host;
  binding_.Bind(GetProxy(&host));
  arc_bridge_service()->net()->instance()->Init(std::move(host));
}

void ArcNetHostImpl::GetNetworksDeprecated(
    bool configured_only,
    bool visible_only,
    const GetNetworksDeprecatedCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (configured_only && visible_only) {
    VLOG(1) << "Illegal arguments - both configured and visible networks "
               "requested.";
    return;
  }

  mojom::GetNetworksRequestType type =
      mojom::GetNetworksRequestType::CONFIGURED_ONLY;
  if (visible_only) {
    type = mojom::GetNetworksRequestType::VISIBLE_ONLY;
  }

  GetNetworks(type, callback);
}

void ArcNetHostImpl::GetNetworks(mojom::GetNetworksRequestType type,
                                 const GetNetworksCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  mojom::NetworkDataPtr data = mojom::NetworkData::New();
  bool configured_only = true;
  bool visible_only = false;
  if (type == mojom::GetNetworksRequestType::VISIBLE_ONLY) {
    configured_only = false;
    visible_only = true;
  }

  // Retrieve list of nearby wifi networks
  chromeos::NetworkTypePattern network_pattern =
      chromeos::onc::NetworkTypePatternFromOncType(onc::network_type::kWiFi);
  std::unique_ptr<base::ListValue> network_properties_list =
      chromeos::network_util::TranslateNetworkListToONC(
          network_pattern, configured_only, visible_only,
          kGetNetworksListLimit);

  // Extract info for each network and add it to the list.
  // Even if there's no WiFi, an empty (size=0) list must be returned and not a
  // null one. The explicitly sized New() constructor ensures the non-null
  // property.
  mojo::Array<mojom::WifiConfigurationPtr> networks =
      mojo::Array<mojom::WifiConfigurationPtr>::New(0);
  for (const auto& value : *network_properties_list) {
    mojom::WifiConfigurationPtr wc = mojom::WifiConfiguration::New();

    base::DictionaryValue* network_dict = nullptr;
    value->GetAsDictionary(&network_dict);
    DCHECK(network_dict);

    // kName is a post-processed version of kHexSSID.
    std::string tmp;
    network_dict->GetString(onc::network_config::kName, &tmp);
    DCHECK(!tmp.empty());
    wc->ssid = tmp;

    tmp.clear();
    network_dict->GetString(onc::network_config::kGUID, &tmp);
    DCHECK(!tmp.empty());
    wc->guid = tmp;

    base::DictionaryValue* wifi_dict = nullptr;
    network_dict->GetDictionary(onc::network_config::kWiFi, &wifi_dict);
    DCHECK(wifi_dict);

    if (!wifi_dict->GetInteger(onc::wifi::kFrequency, &wc->frequency))
      wc->frequency = 0;
    if (!wifi_dict->GetInteger(onc::wifi::kSignalStrength,
                               &wc->signal_strength))
      wc->signal_strength = 0;

    if (!wifi_dict->GetString(onc::wifi::kSecurity, &tmp))
      NOTREACHED();
    DCHECK(!tmp.empty());
    wc->security = tmp;

    if (!wifi_dict->GetString(onc::wifi::kBSSID, &tmp))
      NOTREACHED();
    DCHECK(!tmp.empty());
    wc->bssid = tmp;

    mojom::VisibleNetworkDetailsPtr details =
        mojom::VisibleNetworkDetails::New();
    details->frequency = wc->frequency;
    details->signal_strength = wc->signal_strength;
    details->bssid = wc->bssid;
    wc->details = mojom::NetworkDetails::New();
    wc->details->set_visible(std::move(details));

    networks.push_back(std::move(wc));
  }
  data->networks = std::move(networks);
  callback.Run(std::move(data));
}

void ArcNetHostImpl::CreateNetworkSuccessCallback(
    const arc::mojom::NetHost::CreateNetworkCallback& mojo_callback,
    const std::string& service_path,
    const std::string& guid) {
  VLOG(1) << "CreateNetworkSuccessCallback";

  cached_guid_ = guid;
  cached_service_path_ = service_path;

  mojo_callback.Run(guid);
}

void ArcNetHostImpl::CreateNetworkFailureCallback(
    const arc::mojom::NetHost::CreateNetworkCallback& mojo_callback,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  VLOG(1) << "CreateNetworkFailureCallback: " << error_name;
  mojo_callback.Run("");
}

void ArcNetHostImpl::CreateNetwork(mojom::WifiConfigurationPtr cfg,
                                   const CreateNetworkCallback& callback) {
  if (!IsDeviceOwner()) {
    callback.Run("");
    return;
  }

  std::unique_ptr<base::DictionaryValue> properties(new base::DictionaryValue);
  std::unique_ptr<base::DictionaryValue> wifi_dict(new base::DictionaryValue);

  if (cfg->hexssid.is_null() || !cfg->details) {
    callback.Run("");
    return;
  }
  mojom::ConfiguredNetworkDetailsPtr details =
      std::move(cfg->details->get_configured());
  if (!details) {
    callback.Run("");
    return;
  }

  properties->SetStringWithoutPathExpansion(onc::network_config::kType,
                                            onc::network_config::kWiFi);
  wifi_dict->SetStringWithoutPathExpansion(onc::wifi::kHexSSID, cfg->hexssid);
  wifi_dict->SetBooleanWithoutPathExpansion(onc::wifi::kAutoConnect,
                                            details->autoconnect);
  if (cfg->security.get().empty()) {
    wifi_dict->SetStringWithoutPathExpansion(onc::wifi::kSecurity,
                                             onc::wifi::kSecurityNone);
  } else {
    wifi_dict->SetStringWithoutPathExpansion(onc::wifi::kSecurity,
                                             cfg->security);
    if (!details->passphrase.is_null()) {
      wifi_dict->SetStringWithoutPathExpansion(onc::wifi::kPassphrase,
                                               details->passphrase);
    }
  }
  properties->SetWithoutPathExpansion(onc::network_config::kWiFi,
                                      std::move(wifi_dict));

  std::string user_id_hash = chromeos::LoginState::Get()->primary_user_hash();
  GetManagedConfigurationHandler()->CreateConfiguration(
      user_id_hash, *properties,
      base::Bind(&ArcNetHostImpl::CreateNetworkSuccessCallback,
                 weak_factory_.GetWeakPtr(), callback),
      base::Bind(&ArcNetHostImpl::CreateNetworkFailureCallback,
                 weak_factory_.GetWeakPtr(), callback));
}

bool ArcNetHostImpl::GetNetworkPathFromGuid(const std::string& guid,
                                            std::string* path) {
  const chromeos::NetworkState* network =
      GetStateHandler()->GetNetworkStateFromGuid(guid);
  if (network) {
    *path = network->path();
    return true;
  }

  if (cached_guid_ == guid) {
    *path = cached_service_path_;
    return true;
  } else {
    return false;
  }
}

void ArcNetHostImpl::ForgetNetwork(const mojo::String& guid,
                                   const ForgetNetworkCallback& callback) {
  if (!IsDeviceOwner()) {
    callback.Run(mojom::NetworkResult::FAILURE);
    return;
  }

  std::string path;
  if (!GetNetworkPathFromGuid(guid, &path)) {
    callback.Run(mojom::NetworkResult::FAILURE);
    return;
  }

  cached_guid_.clear();
  GetManagedConfigurationHandler()->RemoveConfiguration(
      path, base::Bind(&ForgetNetworkSuccessCallback, callback),
      base::Bind(&ForgetNetworkFailureCallback, callback));
}

void ArcNetHostImpl::StartConnect(const mojo::String& guid,
                                  const StartConnectCallback& callback) {
  std::string path;
  if (!GetNetworkPathFromGuid(guid, &path)) {
    callback.Run(mojom::NetworkResult::FAILURE);
    return;
  }

  GetNetworkConnectionHandler()->ConnectToNetwork(
      path, base::Bind(&StartConnectSuccessCallback, callback),
      base::Bind(&StartConnectFailureCallback, callback), false);
}

void ArcNetHostImpl::StartDisconnect(const mojo::String& guid,
                                     const StartDisconnectCallback& callback) {
  std::string path;
  if (!GetNetworkPathFromGuid(guid, &path)) {
    callback.Run(mojom::NetworkResult::FAILURE);
    return;
  }

  GetNetworkConnectionHandler()->DisconnectNetwork(
      path, base::Bind(&StartDisconnectSuccessCallback, callback),
      base::Bind(&StartDisconnectFailureCallback, callback));
}

void ArcNetHostImpl::GetWifiEnabledState(
    const GetWifiEnabledStateCallback& callback) {
  bool is_enabled = GetStateHandler()->IsTechnologyEnabled(
      chromeos::NetworkTypePattern::WiFi());
  callback.Run(is_enabled);
}

void ArcNetHostImpl::SetWifiEnabledState(
    bool is_enabled,
    const SetWifiEnabledStateCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  chromeos::NetworkStateHandler::TechnologyState state =
      GetStateHandler()->GetTechnologyState(
          chromeos::NetworkTypePattern::WiFi());
  // WiFi can't be enabled or disabled in these states.
  if ((state == chromeos::NetworkStateHandler::TECHNOLOGY_PROHIBITED) ||
      (state == chromeos::NetworkStateHandler::TECHNOLOGY_UNINITIALIZED) ||
      (state == chromeos::NetworkStateHandler::TECHNOLOGY_UNAVAILABLE)) {
    VLOG(1) << "SetWifiEnabledState failed due to WiFi state: " << state;
    callback.Run(false);
  } else {
    GetStateHandler()->SetTechnologyEnabled(
        chromeos::NetworkTypePattern::WiFi(), is_enabled,
        chromeos::network_handler::ErrorCallback());
    callback.Run(true);
  }
}

void ArcNetHostImpl::StartScan() {
  GetStateHandler()->RequestScan();
}

void ArcNetHostImpl::ScanCompleted(const chromeos::DeviceState* /*unused*/) {
  if (!arc_bridge_service()->net()->instance()) {
    VLOG(2) << "NetInstance not ready yet";
    return;
  }
  if (arc_bridge_service()->net()->version() < 1) {
    VLOG(1) << "NetInstance does not support ScanCompleted.";
    return;
  }

  arc_bridge_service()->net()->instance()->ScanCompleted();
}

void ArcNetHostImpl::GetDefaultNetwork(
    const GetDefaultNetworkCallback& callback) {
  const chromeos::NetworkState* default_network =
      GetStateHandler()->DefaultNetwork();
  if (!default_network) {
    VLOG(1) << "GetDefaultNetwork: no default network";
    callback.Run(nullptr, nullptr);
    return;
  }
  VLOG(1) << "GetDefaultNetwork: default network is "
          << default_network->path();
  std::string user_id_hash = chromeos::LoginState::Get()->primary_user_hash();
  GetManagedConfigurationHandler()->GetProperties(
      user_id_hash, default_network->path(),
      base::Bind(&GetDefaultNetworkSuccessCallback, callback),
      base::Bind(&GetDefaultNetworkFailureCallback, callback));
}

void ArcNetHostImpl::DefaultNetworkSuccessCallback(
    const std::string& service_path,
    const base::DictionaryValue& dictionary) {
  if (!arc_bridge_service()->net()->instance()) {
    VLOG(2) << "NetInstance is null.";
    return;
  }
  arc_bridge_service()->net()->instance()->DefaultNetworkChanged(
      TranslateONCConfiguration(&dictionary),
      TranslateONCConfiguration(&dictionary));
}

void ArcNetHostImpl::DefaultNetworkChanged(
    const chromeos::NetworkState* network) {
  if (arc_bridge_service()->net()->version() < 2) {
    VLOG(1) << "ArcBridgeService does not support DefaultNetworkChanged.";
    return;
  }

  if (!network) {
    VLOG(1) << "No default network";
    arc_bridge_service()->net()->instance()->DefaultNetworkChanged(nullptr,
                                                                   nullptr);
    return;
  }

  VLOG(1) << "New default network: " << network->path();
  std::string user_id_hash = chromeos::LoginState::Get()->primary_user_hash();
  GetManagedConfigurationHandler()->GetProperties(
      user_id_hash, network->path(),
      base::Bind(&arc::ArcNetHostImpl::DefaultNetworkSuccessCallback,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&DefaultNetworkFailureCallback));
}

void ArcNetHostImpl::DeviceListChanged() {
  if (arc_bridge_service()->net()->version() < 3) {
    VLOG(1) << "ArcBridgeService does not support DeviceListChanged.";
    return;
  }

  bool is_enabled = GetStateHandler()->IsTechnologyEnabled(
      chromeos::NetworkTypePattern::WiFi());
  arc_bridge_service()->net()->instance()->WifiEnabledStateChanged(is_enabled);
}

void ArcNetHostImpl::OnShuttingDown() {
  GetStateHandler()->RemoveObserver(this, FROM_HERE);
}

}  // namespace arc
