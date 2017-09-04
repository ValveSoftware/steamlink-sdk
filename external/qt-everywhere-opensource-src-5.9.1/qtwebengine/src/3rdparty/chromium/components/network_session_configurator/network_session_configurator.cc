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
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "net/http/http_stream_factory.h"
#include "net/quic/core/quic_protocol.h"
#include "net/quic/core/quic_utils.h"
#include "net/url_request/url_fetcher.h"

namespace {

// Map from name to value for all parameters associate with a field trial.
using VariationParameters = std::map<std::string, std::string>;

const char kTCPFastOpenFieldTrialName[] = "TCPFastOpen";
const char kTCPFastOpenHttpsEnabledGroupName[] = "HttpsEnabled";

const char kQuicFieldTrialName[] = "QUIC";
const char kQuicFieldTrialEnabledGroupName[] = "Enabled";
const char kQuicFieldTrialHttpsEnabledGroupName[] = "HttpsEnabled";

// Field trial for HTTP/2.
const char kHttp2FieldTrialName[] = "HTTP2";
const char kHttp2FieldTrialDisablePrefix[] = "Disable";

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

void ConfigureHttp2Params(base::StringPiece http2_trial_group,
                          net::HttpNetworkSession::Params* params) {
  if (http2_trial_group.starts_with(kHttp2FieldTrialDisablePrefix)) {
    params->enable_http2 = false;
  }
}

bool ShouldEnableQuic(base::StringPiece quic_trial_group,
                      bool is_quic_force_disabled,
                      bool is_quic_force_enabled) {
  if (is_quic_force_disabled)
    return false;
  if (is_quic_force_enabled)
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
    const VariationParameters& quic_trial_params) {
  return !base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params,
                        "enable_alternative_service_with_different_host"),
      "false");
}

net::QuicTagVector GetQuicConnectionOptions(
    const VariationParameters& quic_trial_params) {
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

bool ShouldForceHolBlocking(const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "force_hol_blocking"), "true");
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

int GetQuicReducedPingTimeoutSeconds(
    const VariationParameters& quic_trial_params) {
  int value;
  if (base::StringToInt(
          GetVariationParam(quic_trial_params, "reduced_ping_timeout_seconds"),
          &value)) {
    return value;
  }
  return 0;
}

int GetQuicPacketReaderYieldAfterDurationMilliseconds(
    const VariationParameters& quic_trial_params) {
  int value;
  if (base::StringToInt(
          GetVariationParam(quic_trial_params,
                            "packet_reader_yield_after_duration_milliseconds"),
          &value)) {
    return value;
  }
  return 0;
}

bool ShouldQuicRaceCertVerification(
    const VariationParameters& quic_trial_params) {
   return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "race_cert_verification"),
      "true");
}

bool ShouldQuicDoNotFragment(
    const VariationParameters& quic_trial_params) {
   return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "do_not_fragment"),
      "true");
}

bool ShouldQuicDisablePreConnectIfZeroRtt(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "disable_preconnect_if_0rtt"),
      "true");
}

std::unordered_set<std::string> GetQuicHostWhitelist(
    const VariationParameters& quic_trial_params) {
  std::string whitelist =
      GetVariationParam(quic_trial_params, "quic_host_whitelist");
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

bool ShouldQuicAllowServerMigration(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params,
                        "allow_server_migration"),
      "true");
}

size_t GetQuicMaxPacketLength(const VariationParameters& quic_trial_params) {
  unsigned value;
  if (base::StringToUint(
          GetVariationParam(quic_trial_params, "max_packet_length"), &value)) {
    return value;
  }
  return 0;
}

net::QuicVersion GetQuicVersion(const VariationParameters& quic_trial_params) {
  return network_session_configurator::ParseQuicVersion(
      GetVariationParam(quic_trial_params, "quic_version"));
}

void ConfigureQuicParams(base::StringPiece quic_trial_group,
                         const VariationParameters& quic_trial_params,
                         bool is_quic_force_disabled,
                         bool is_quic_force_enabled,
                         const std::string& quic_user_agent_id,
                         net::HttpNetworkSession::Params* params) {
  params->enable_quic = ShouldEnableQuic(
      quic_trial_group, is_quic_force_disabled, is_quic_force_enabled);
  params->disable_quic_on_timeout_with_open_streams =
      ShouldDisableQuicWhenConnectionTimesOutWithOpenStreams(quic_trial_params);

  params->enable_quic_alternative_service_with_different_host =
      ShouldQuicEnableAlternativeServicesForDifferentHost(quic_trial_params);

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
    params->quic_force_hol_blocking = ShouldForceHolBlocking(quic_trial_params);
    // Default to disabling port selection on all channels.
    params->enable_quic_port_selection = false;
    params->quic_connection_options =
        GetQuicConnectionOptions(quic_trial_params);
    params->quic_close_sessions_on_ip_change =
        ShouldQuicCloseSessionsOnIpChange(quic_trial_params);
    int idle_connection_timeout_seconds =
        GetQuicIdleConnectionTimeoutSeconds(quic_trial_params);
    if (idle_connection_timeout_seconds != 0) {
      params->quic_idle_connection_timeout_seconds =
          idle_connection_timeout_seconds;
    }
    int reduced_ping_timeout_seconds =
        GetQuicReducedPingTimeoutSeconds(quic_trial_params);
    if (reduced_ping_timeout_seconds > 0 &&
        reduced_ping_timeout_seconds < net::kPingTimeoutSecs) {
      params->quic_reduced_ping_timeout_seconds = reduced_ping_timeout_seconds;
    }
    int packet_reader_yield_after_duration_milliseconds =
        GetQuicPacketReaderYieldAfterDurationMilliseconds(quic_trial_params);
    if (packet_reader_yield_after_duration_milliseconds != 0) {
      params->quic_packet_reader_yield_after_duration_milliseconds =
          packet_reader_yield_after_duration_milliseconds;
    }
    params->quic_race_cert_verification =
        ShouldQuicRaceCertVerification(quic_trial_params);
    params->quic_do_not_fragment =
        ShouldQuicDoNotFragment(quic_trial_params);
    params->quic_disable_preconnect_if_0rtt =
        ShouldQuicDisablePreConnectIfZeroRtt(quic_trial_params);
    params->quic_host_whitelist = GetQuicHostWhitelist(quic_trial_params);
    params->quic_migrate_sessions_on_network_change =
        ShouldQuicMigrateSessionsOnNetworkChange(quic_trial_params);
    params->quic_migrate_sessions_early =
        ShouldQuicMigrateSessionsEarly(quic_trial_params);
    params->quic_allow_server_migration =
        ShouldQuicAllowServerMigration(quic_trial_params);
  }

  size_t max_packet_length = GetQuicMaxPacketLength(quic_trial_params);
  if (max_packet_length != 0) {
    params->quic_max_packet_length = max_packet_length;
  }

  params->quic_user_agent_id = quic_user_agent_id;

  net::QuicVersion version = GetQuicVersion(quic_trial_params);
  if (version != net::QUIC_VERSION_UNSUPPORTED) {
    net::QuicVersionVector supported_versions;
    supported_versions.push_back(version);
    params->quic_supported_versions = supported_versions;
  }
}

}  // anonymous namespace

namespace network_session_configurator {

net::QuicVersion ParseQuicVersion(const std::string& quic_version) {
  net::QuicVersionVector supported_versions = net::AllSupportedVersions();
  for (size_t i = 0; i < supported_versions.size(); ++i) {
    net::QuicVersion version = supported_versions[i];
    if (net::QuicVersionToString(version) == quic_version) {
      return version;
    }
  }

  return net::QUIC_VERSION_UNSUPPORTED;
}

void ParseFieldTrials(bool is_quic_force_disabled,
                      bool is_quic_force_enabled,
                      const std::string& quic_user_agent_id,
                      net::HttpNetworkSession::Params* params) {
  std::string quic_trial_group =
      base::FieldTrialList::FindFullName(kQuicFieldTrialName);
  VariationParameters quic_trial_params;
  if (!variations::GetVariationParams(kQuicFieldTrialName, &quic_trial_params))
    quic_trial_params.clear();
  ConfigureQuicParams(quic_trial_group, quic_trial_params,
                      is_quic_force_disabled, is_quic_force_enabled,
                      quic_user_agent_id, params);

  std::string http2_trial_group =
      base::FieldTrialList::FindFullName(kHttp2FieldTrialName);
  ConfigureHttp2Params(http2_trial_group, params);

  const std::string tfo_trial_group =
      base::FieldTrialList::FindFullName(kTCPFastOpenFieldTrialName);
  ConfigureTCPFastOpenParams(tfo_trial_group, params);
}

}  // namespace network_session_configurator
