// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_session_configurator/network_session_configurator.h"

#include <map>

#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/network_session_configurator/switches.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "net/http/http_stream_factory.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_utils.h"
#include "net/url_request/url_fetcher.h"

namespace {

// Map from name to value for all parameters associate with a field trial.
using VariationParameters = std::map<std::string, std::string>;

const char kTCPFastOpenFieldTrialName[] = "TCPFastOpen";
const char kTCPFastOpenHttpsEnabledGroupName[] = "HttpsEnabled";

const char kQuicFieldTrialName[] = "QUIC";
const char kQuicFieldTrialEnabledGroupName[] = "Enabled";
const char kQuicFieldTrialHttpsEnabledGroupName[] = "HttpsEnabled";

// The SPDY trial composes two different trial plus control groups:
//  * A "holdback" group with SPDY disabled, and corresponding control
//  (SPDY/3.1). The primary purpose of the holdback group is to encourage site
//  operators to do feature detection rather than UA-sniffing. As such, this
//  trial runs continuously.
//  * A SPDY/4 experiment, for SPDY/4 (aka HTTP/2) vs SPDY/3.1 comparisons and
//  eventual SPDY/4 deployment.
const char kSpdyFieldTrialName[] = "SPDY";
const char kSpdyFieldTrialHoldbackGroupNamePrefix[] = "SpdyDisabled";
const char kSpdyFieldTrialSpdy31GroupNamePrefix[] = "Spdy31Enabled";
const char kSpdyFieldTrialSpdy4GroupNamePrefix[] = "Spdy4Enabled";
const char kSpdyFieldTrialParametrizedPrefix[] = "Parametrized";

// Field trial for NPN.
const char kNpnTrialName[] = "NPN";
const char kNpnTrialEnabledGroupNamePrefix[] = "Enable";
const char kNpnTrialDisabledGroupNamePrefix[] = "Disable";

// Field trial for priority dependencies.
const char kSpdyDependenciesFieldTrial[] = "SpdyEnableDependencies";
const char kSpdyDependenciesFieldTrialEnable[] = "Enable";
const char kSpdyDepencenciesFieldTrialDisable[] = "Disable";

int GetSwitchValueAsInt(const base::CommandLine& command_line,
                        const std::string& switch_name) {
  int value;
  if (!base::StringToInt(command_line.GetSwitchValueASCII(switch_name),
                         &value)) {
    return 0;
  }
  return value;
}

// Returns the value associated with |key| in |params| or "" if the
// key is not present in the map.
const std::string& GetVariationParam(
    const std::map<std::string, std::string>& params,
    const std::string& key) {
  std::map<std::string, std::string>::const_iterator it = params.find(key);
  if (it == params.end())
    return base::EmptyString();

  return it->second;
}

void ConfigureTCPFastOpenParams(base::StringPiece tfo_trial_group,
                                net::HttpNetworkSession::Params* params) {
  if (tfo_trial_group == kTCPFastOpenHttpsEnabledGroupName)
    params->enable_tcp_fast_open_for_ssl = true;
}

void ConfigureSpdyParams(const base::CommandLine& command_line,
                         base::StringPiece spdy_trial_group,
                         const VariationParameters& spdy_trial_params,
                         bool is_spdy_allowed_by_policy,
                         net::HttpNetworkSession::Params* params) {
  // Only handle SPDY field trial parameters and command line flags if
  // "spdy.disabled" preference is not forced via policy.
  if (!is_spdy_allowed_by_policy) {
    params->enable_spdy31 = false;
    return;
  }

  if (command_line.HasSwitch(switches::kIgnoreUrlFetcherCertRequests))
    net::URLFetcher::SetIgnoreCertificateRequests(true);

  if (command_line.HasSwitch(switches::kDisableHttp2)) {
    params->enable_spdy31 = false;
    params->enable_http2 = false;
    return;
  }

  if (spdy_trial_group.starts_with(kSpdyFieldTrialHoldbackGroupNamePrefix)) {
    net::HttpStreamFactory::set_spdy_enabled(false);
    return;
  }
  if (spdy_trial_group.starts_with(kSpdyFieldTrialSpdy31GroupNamePrefix)) {
    params->enable_spdy31 = true;
    params->enable_http2 = false;
    return;
  }
  if (spdy_trial_group.starts_with(kSpdyFieldTrialSpdy4GroupNamePrefix)) {
    params->enable_spdy31 = true;
    params->enable_http2 = true;
    return;
  }
  if (spdy_trial_group.starts_with(kSpdyFieldTrialParametrizedPrefix)) {
    bool spdy_enabled = false;
    params->enable_spdy31 = false;
    params->enable_http2 = false;
    if (base::LowerCaseEqualsASCII(
            GetVariationParam(spdy_trial_params, "enable_http2"), "true")) {
      spdy_enabled = true;
      params->enable_http2 = true;
    }
    if (base::LowerCaseEqualsASCII(
            GetVariationParam(spdy_trial_params, "enable_spdy31"), "true")) {
      spdy_enabled = true;
      params->enable_spdy31 = true;
    }
    // TODO(bnc): https://crbug.com/521597
    // HttpStreamFactory::spdy_enabled_ is redundant with params->enable_http2
    // and enable_spdy31, can it be eliminated?
    net::HttpStreamFactory::set_spdy_enabled(spdy_enabled);
    return;
  }
}

void ConfigureNPNParams(const base::CommandLine& command_line,
                        base::StringPiece npn_trial_group,
                        net::HttpNetworkSession::Params* params) {
  if (npn_trial_group.starts_with(kNpnTrialEnabledGroupNamePrefix)) {
    params->enable_npn = true;
  } else if (npn_trial_group.starts_with(kNpnTrialDisabledGroupNamePrefix)) {
    params->enable_npn = false;
  }
}

void ConfigurePriorityDependencies(
    base::StringPiece priority_dependencies_trial_group,
    net::HttpNetworkSession::Params* params) {
  if (priority_dependencies_trial_group.starts_with(
          kSpdyDependenciesFieldTrialEnable)) {
    params->enable_priority_dependencies = true;
  } else if (priority_dependencies_trial_group.starts_with(
                 kSpdyDepencenciesFieldTrialDisable)) {
    params->enable_priority_dependencies = false;
  }
}

bool ShouldEnableQuic(const base::CommandLine& command_line,
                      base::StringPiece quic_trial_group,
                      bool is_quic_allowed_by_policy) {
  if (command_line.HasSwitch(switches::kDisableQuic) ||
      !is_quic_allowed_by_policy)
    return false;

  if (command_line.HasSwitch(switches::kEnableQuic))
    return true;

  return quic_trial_group.starts_with(kQuicFieldTrialEnabledGroupName) ||
         quic_trial_group.starts_with(kQuicFieldTrialHttpsEnabledGroupName);
}

bool ShouldDisableQuicWhenConnectionTimesOutWithOpenStreams(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params,
                        "disable_quic_on_timeout_with_open_streams"),
      "true");
}

bool ShouldQuicDisableConnectionPooling(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "disable_connection_pooling"),
      "true");
}

bool ShouldQuicEnableAlternativeServicesForDifferentHost(
    const base::CommandLine& command_line,
    const VariationParameters& quic_trial_params) {
  return !base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params,
                        "enable_alternative_service_with_different_host"),
      "false");
}

bool ShouldEnableQuicPortSelection(const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kDisableQuicPortSelection))
    return false;

  if (command_line.HasSwitch(switches::kEnableQuicPortSelection))
    return true;

  return false;  // Default to disabling port selection on all channels.
}

net::QuicTagVector GetQuicConnectionOptions(
    const base::CommandLine& command_line,
    const VariationParameters& quic_trial_params) {
  if (command_line.HasSwitch(switches::kQuicConnectionOptions)) {
    return net::QuicUtils::ParseQuicConnectionOptions(
        command_line.GetSwitchValueASCII(switches::kQuicConnectionOptions));
  }

  VariationParameters::const_iterator it =
      quic_trial_params.find("connection_options");
  if (it == quic_trial_params.end()) {
    return net::QuicTagVector();
  }

  return net::QuicUtils::ParseQuicConnectionOptions(it->second);
}

bool ShouldQuicAlwaysRequireHandshakeConfirmation(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params,
                        "always_require_handshake_confirmation"),
      "true");
}

float GetQuicLoadServerInfoTimeoutSrttMultiplier(
    const VariationParameters& quic_trial_params) {
  double value;
  if (base::StringToDouble(
          GetVariationParam(quic_trial_params, "load_server_info_time_to_srtt"),
          &value)) {
    return static_cast<float>(value);
  }
  return 0.0f;
}

bool ShouldQuicEnableConnectionRacing(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "enable_connection_racing"), "true");
}

bool ShouldQuicEnableNonBlockingIO(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "enable_non_blocking_io"), "true");
}

bool ShouldQuicDisableDiskCache(const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "disable_disk_cache"), "true");
}

bool ShouldQuicPreferAes(const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "prefer_aes"), "true");
}

int GetQuicMaxNumberOfLossyConnections(
    const VariationParameters& quic_trial_params) {
  int value;
  if (base::StringToInt(GetVariationParam(quic_trial_params,
                                          "max_number_of_lossy_connections"),
                        &value)) {
    return value;
  }
  return 0;
}

float GetQuicPacketLossThreshold(const VariationParameters& quic_trial_params) {
  double value;
  if (base::StringToDouble(
          GetVariationParam(quic_trial_params, "packet_loss_threshold"),
          &value)) {
    return static_cast<float>(value);
  }
  return 0.0f;
}

int GetQuicSocketReceiveBufferSize(
    const VariationParameters& quic_trial_params) {
  int value;
  if (base::StringToInt(
          GetVariationParam(quic_trial_params, "receive_buffer_size"),
          &value)) {
    return value;
  }
  return 0;
}

bool ShouldQuicDelayTcpRace(const VariationParameters& quic_trial_params) {
  return !base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "disable_delay_tcp_race"), "true");
}

bool ShouldQuicCloseSessionsOnIpChange(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "close_sessions_on_ip_change"),
      "true");
}

int GetQuicIdleConnectionTimeoutSeconds(
    const VariationParameters& quic_trial_params) {
  int value;
  if (base::StringToInt(GetVariationParam(quic_trial_params,
                                          "idle_connection_timeout_seconds"),
                        &value)) {
    return value;
  }
  return 0;
}

bool ShouldQuicDisablePreConnectIfZeroRtt(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "disable_preconnect_if_0rtt"),
      "true");
}

std::unordered_set<std::string> GetQuicHostWhitelist(
    const base::CommandLine& command_line,
    const VariationParameters& quic_trial_params) {
  std::string whitelist;
  if (command_line.HasSwitch(switches::kQuicHostWhitelist)) {
    whitelist = command_line.GetSwitchValueASCII(switches::kQuicHostWhitelist);
  } else {
    whitelist = GetVariationParam(quic_trial_params, "quic_host_whitelist");
  }
  std::unordered_set<std::string> hosts;
  for (const std::string& host : base::SplitString(
           whitelist, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    hosts.insert(host);
  }
  return hosts;
}

bool ShouldQuicMigrateSessionsOnNetworkChange(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params,
                        "migrate_sessions_on_network_change"),
      "true");
}

bool ShouldQuicMigrateSessionsEarly(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "migrate_sessions_early"), "true");
}

size_t GetQuicMaxPacketLength(const base::CommandLine& command_line,
                              const VariationParameters& quic_trial_params) {
  if (command_line.HasSwitch(switches::kQuicMaxPacketLength)) {
    unsigned value;
    if (!base::StringToUint(
            command_line.GetSwitchValueASCII(switches::kQuicMaxPacketLength),
            &value)) {
      return 0;
    }
    return value;
  }

  unsigned value;
  if (base::StringToUint(
          GetVariationParam(quic_trial_params, "max_packet_length"), &value)) {
    return value;
  }
  return 0;
}

net::QuicVersion ParseQuicVersion(const std::string& quic_version) {
  net::QuicVersionVector supported_versions = net::QuicSupportedVersions();
  for (size_t i = 0; i < supported_versions.size(); ++i) {
    net::QuicVersion version = supported_versions[i];
    if (net::QuicVersionToString(version) == quic_version) {
      return version;
    }
  }

  return net::QUIC_VERSION_UNSUPPORTED;
}

net::QuicVersion GetQuicVersion(const base::CommandLine& command_line,
                                const VariationParameters& quic_trial_params) {
  if (command_line.HasSwitch(switches::kQuicVersion)) {
    return ParseQuicVersion(
        command_line.GetSwitchValueASCII(switches::kQuicVersion));
  }

  return ParseQuicVersion(GetVariationParam(quic_trial_params, "quic_version"));
}

void ConfigureQuicParams(const base::CommandLine& command_line,
                         base::StringPiece quic_trial_group,
                         const VariationParameters& quic_trial_params,
                         bool is_quic_allowed_by_policy,
                         const std::string& quic_user_agent_id,
                         net::HttpNetworkSession::Params* params) {
  params->enable_quic = ShouldEnableQuic(command_line, quic_trial_group,
                                         is_quic_allowed_by_policy);
  params->disable_quic_on_timeout_with_open_streams =
      ShouldDisableQuicWhenConnectionTimesOutWithOpenStreams(quic_trial_params);

  params->enable_quic_alternative_service_with_different_host =
      ShouldQuicEnableAlternativeServicesForDifferentHost(command_line,
                                                          quic_trial_params);

  if (params->enable_quic) {
    params->quic_always_require_handshake_confirmation =
        ShouldQuicAlwaysRequireHandshakeConfirmation(quic_trial_params);
    params->quic_disable_connection_pooling =
        ShouldQuicDisableConnectionPooling(quic_trial_params);
    int receive_buffer_size = GetQuicSocketReceiveBufferSize(quic_trial_params);
    if (receive_buffer_size != 0) {
      params->quic_socket_receive_buffer_size = receive_buffer_size;
    }
    params->quic_delay_tcp_race = ShouldQuicDelayTcpRace(quic_trial_params);
    float load_server_info_timeout_srtt_multiplier =
        GetQuicLoadServerInfoTimeoutSrttMultiplier(quic_trial_params);
    if (load_server_info_timeout_srtt_multiplier != 0) {
      params->quic_load_server_info_timeout_srtt_multiplier =
          load_server_info_timeout_srtt_multiplier;
    }
    params->quic_enable_connection_racing =
        ShouldQuicEnableConnectionRacing(quic_trial_params);
    params->quic_enable_non_blocking_io =
        ShouldQuicEnableNonBlockingIO(quic_trial_params);
    params->quic_disable_disk_cache =
        ShouldQuicDisableDiskCache(quic_trial_params);
    params->quic_prefer_aes = ShouldQuicPreferAes(quic_trial_params);
    int max_number_of_lossy_connections =
        GetQuicMaxNumberOfLossyConnections(quic_trial_params);
    if (max_number_of_lossy_connections != 0) {
      params->quic_max_number_of_lossy_connections =
          max_number_of_lossy_connections;
    }
    float packet_loss_threshold = GetQuicPacketLossThreshold(quic_trial_params);
    if (packet_loss_threshold != 0)
      params->quic_packet_loss_threshold = packet_loss_threshold;
    params->enable_quic_port_selection =
        ShouldEnableQuicPortSelection(command_line);
    params->quic_connection_options =
        GetQuicConnectionOptions(command_line, quic_trial_params);
    params->quic_close_sessions_on_ip_change =
        ShouldQuicCloseSessionsOnIpChange(quic_trial_params);
    int idle_connection_timeout_seconds =
        GetQuicIdleConnectionTimeoutSeconds(quic_trial_params);
    if (idle_connection_timeout_seconds != 0) {
      params->quic_idle_connection_timeout_seconds =
          idle_connection_timeout_seconds;
    }
    params->quic_disable_preconnect_if_0rtt =
        ShouldQuicDisablePreConnectIfZeroRtt(quic_trial_params);
    params->quic_host_whitelist =
        GetQuicHostWhitelist(command_line, quic_trial_params);
    params->quic_migrate_sessions_on_network_change =
        ShouldQuicMigrateSessionsOnNetworkChange(quic_trial_params);
    params->quic_migrate_sessions_early =
        ShouldQuicMigrateSessionsEarly(quic_trial_params);
  }

  size_t max_packet_length =
      GetQuicMaxPacketLength(command_line, quic_trial_params);
  if (max_packet_length != 0) {
    params->quic_max_packet_length = max_packet_length;
  }

  params->quic_user_agent_id = quic_user_agent_id;

  net::QuicVersion version = GetQuicVersion(command_line, quic_trial_params);
  if (version != net::QUIC_VERSION_UNSUPPORTED) {
    net::QuicVersionVector supported_versions;
    supported_versions.push_back(version);
    params->quic_supported_versions = supported_versions;
  }

  if (command_line.HasSwitch(switches::kOriginToForceQuicOn)) {
    std::string origins =
        command_line.GetSwitchValueASCII(switches::kOriginToForceQuicOn);
    for (const std::string& host_port : base::SplitString(
             origins, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      net::HostPortPair quic_origin = net::HostPortPair::FromString(host_port);
      if (!quic_origin.IsEmpty())
        params->origins_to_force_quic_on.insert(quic_origin);
    }
  }
}

void ParseFieldTrialsAndCommandLineInternal(
    const base::CommandLine& command_line,
    bool is_spdy_allowed_by_policy,
    bool is_quic_allowed_by_policy,
    const std::string& quic_user_agent_id,
    net::HttpNetworkSession::Params* params) {
  // Parameters only controlled by command line.
  if (command_line.HasSwitch(switches::kIgnoreCertificateErrors))
    params->ignore_certificate_errors = true;
  if (command_line.HasSwitch(switches::kTestingFixedHttpPort)) {
    params->testing_fixed_http_port =
        GetSwitchValueAsInt(command_line, switches::kTestingFixedHttpPort);
  }
  if (command_line.HasSwitch(switches::kTestingFixedHttpsPort)) {
    params->testing_fixed_https_port =
        GetSwitchValueAsInt(command_line, switches::kTestingFixedHttpsPort);
  }

  // Always fetch the field trial groups to ensure they are reported correctly.
  // The command line flags will be associated with a group that is reported so
  // long as trial is actually queried.

  std::string quic_trial_group =
      base::FieldTrialList::FindFullName(kQuicFieldTrialName);
  VariationParameters quic_trial_params;
  if (!variations::GetVariationParams(kQuicFieldTrialName, &quic_trial_params))
    quic_trial_params.clear();
  ConfigureQuicParams(command_line, quic_trial_group, quic_trial_params,
                      is_quic_allowed_by_policy, quic_user_agent_id, params);

  if (!is_spdy_allowed_by_policy) {
    base::FieldTrial* trial = base::FieldTrialList::Find(kSpdyFieldTrialName);
    if (trial)
      trial->Disable();
  }
  std::string spdy_trial_group =
      base::FieldTrialList::FindFullName(kSpdyFieldTrialName);
  VariationParameters spdy_trial_params;
  if (!variations::GetVariationParams(kSpdyFieldTrialName, &spdy_trial_params))
    spdy_trial_params.clear();
  ConfigureSpdyParams(command_line, spdy_trial_group, spdy_trial_params,
                      is_spdy_allowed_by_policy, params);

  const std::string tfo_trial_group =
      base::FieldTrialList::FindFullName(kTCPFastOpenFieldTrialName);
  ConfigureTCPFastOpenParams(tfo_trial_group, params);

  std::string npn_trial_group =
      base::FieldTrialList::FindFullName(kNpnTrialName);
  ConfigureNPNParams(command_line, npn_trial_group, params);

  std::string priority_dependencies_trial_group =
      base::FieldTrialList::FindFullName(kSpdyDependenciesFieldTrial);
  ConfigurePriorityDependencies(priority_dependencies_trial_group, params);
}

}  // anonymous namespace

namespace network_session_configurator {

void ParseFieldTrials(bool is_spdy_allowed_by_policy,
                      bool is_quic_allowed_by_policy,
                      const std::string& quic_user_agent_id,
                      net::HttpNetworkSession::Params* params) {
  const base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  ParseFieldTrialsAndCommandLineInternal(
      command_line, is_spdy_allowed_by_policy, is_quic_allowed_by_policy,
      quic_user_agent_id, params);
}

void ParseFieldTrialsAndCommandLine(bool is_spdy_allowed_by_policy,
                                    bool is_quic_allowed_by_policy,
                                    const std::string& quic_user_agent_id,
                                    net::HttpNetworkSession::Params* params) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  ParseFieldTrialsAndCommandLineInternal(
      command_line, is_spdy_allowed_by_policy, is_quic_allowed_by_policy,
      quic_user_agent_id, params);
}

}  // namespace network_session_configurator
