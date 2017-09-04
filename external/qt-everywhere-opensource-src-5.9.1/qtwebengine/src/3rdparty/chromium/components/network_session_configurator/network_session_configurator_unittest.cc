// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_session_configurator/network_session_configurator.h"

#include <map>
#include <memory>

#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/test/mock_entropy_provider.h"
#include "components/variations/variations_associated_data.h"
#include "net/http/http_stream_factory.h"
#include "net/quic/core/quic_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace test {

class NetworkSessionConfiguratorTest : public testing::Test {
 public:
  NetworkSessionConfiguratorTest()
      : quic_user_agent_id_("Chrome/52.0.2709.0 Linux x86_64") {
    field_trial_list_.reset(
        new base::FieldTrialList(
            base::MakeUnique<base::MockEntropyProvider>()));
    variations::testing::ClearAllVariationParams();
  }

  void ParseFieldTrials() {
    network_session_configurator::ParseFieldTrials(
        /*is_quic_force_disabled=*/false,
        /*is_quic_force_enabled=*/false, quic_user_agent_id_, &params_);
  }

  std::string quic_user_agent_id_;
  std::unique_ptr<base::FieldTrialList> field_trial_list_;
  net::HttpNetworkSession::Params params_;
};

TEST_F(NetworkSessionConfiguratorTest, Defaults) {
  ParseFieldTrials();

  EXPECT_FALSE(params_.ignore_certificate_errors);
  EXPECT_EQ("Chrome/52.0.2709.0 Linux x86_64", params_.quic_user_agent_id);
  EXPECT_EQ(0u, params_.testing_fixed_http_port);
  EXPECT_EQ(0u, params_.testing_fixed_https_port);
  EXPECT_TRUE(params_.enable_http2);
  EXPECT_FALSE(params_.enable_tcp_fast_open_for_ssl);
  EXPECT_TRUE(params_.enable_quic_alternative_service_with_different_host);
  EXPECT_FALSE(params_.enable_quic);
}

TEST_F(NetworkSessionConfiguratorTest, Http2FieldTrialHttp2Disable) {
  base::FieldTrialList::CreateFieldTrial("HTTP2", "Disable");

  ParseFieldTrials();

  EXPECT_FALSE(params_.enable_http2);
}

TEST_F(NetworkSessionConfiguratorTest, EnableQuicFromFieldTrialGroup) {
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_quic);
  EXPECT_FALSE(params_.disable_quic_on_timeout_with_open_streams);
  EXPECT_EQ(1350u, params_.quic_max_packet_length);
  EXPECT_EQ(net::QuicTagVector(), params_.quic_connection_options);
  EXPECT_FALSE(params_.quic_always_require_handshake_confirmation);
  EXPECT_FALSE(params_.quic_disable_connection_pooling);
  EXPECT_EQ(0.25f, params_.quic_load_server_info_timeout_srtt_multiplier);
  EXPECT_FALSE(params_.quic_enable_connection_racing);
  EXPECT_FALSE(params_.quic_enable_non_blocking_io);
  EXPECT_FALSE(params_.quic_disable_disk_cache);
  EXPECT_FALSE(params_.quic_prefer_aes);
  EXPECT_TRUE(params_.enable_quic_alternative_service_with_different_host);
  EXPECT_TRUE(params_.quic_delay_tcp_race);
  EXPECT_FALSE(params_.quic_close_sessions_on_ip_change);
  EXPECT_EQ(net::kIdleConnectionTimeoutSeconds,
            params_.quic_idle_connection_timeout_seconds);
  EXPECT_EQ(net::kPingTimeoutSecs, params_.quic_reduced_ping_timeout_seconds);
  EXPECT_EQ(net::kQuicYieldAfterDurationMilliseconds,
            params_.quic_packet_reader_yield_after_duration_milliseconds);
  EXPECT_FALSE(params_.quic_race_cert_verification);
  EXPECT_FALSE(params_.quic_do_not_fragment);
  EXPECT_FALSE(params_.quic_disable_preconnect_if_0rtt);
  EXPECT_FALSE(params_.quic_migrate_sessions_on_network_change);
  EXPECT_FALSE(params_.quic_migrate_sessions_early);
  EXPECT_FALSE(params_.quic_allow_server_migration);
  EXPECT_TRUE(params_.quic_host_whitelist.empty());
  EXPECT_FALSE(params_.quic_force_hol_blocking);

  net::HttpNetworkSession::Params default_params;
  EXPECT_EQ(default_params.quic_supported_versions,
            params_.quic_supported_versions);
}

TEST_F(NetworkSessionConfiguratorTest, EnableQuicForDataReductionProxy) {
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  base::FieldTrialList::CreateFieldTrial("DataReductionProxyUseQuic",
                                         "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_quic);
}

TEST_F(NetworkSessionConfiguratorTest,
       DisableQuicWhenConnectionTimesOutWithOpenStreamsFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["disable_quic_on_timeout_with_open_streams"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.disable_quic_on_timeout_with_open_streams);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicCloseSessionsOnIpChangeFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["close_sessions_on_ip_change"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_close_sessions_on_ip_change);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicIdleConnectionTimeoutSecondsFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["idle_connection_timeout_seconds"] = "300";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(300, params_.quic_idle_connection_timeout_seconds);
}

TEST_F(NetworkSessionConfiguratorTest,
       NegativeQuicReducedPingTimeoutSecondsFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["reduced_ping_timeout_seconds"] = "-5";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  EXPECT_EQ(net::kPingTimeoutSecs, params_.quic_reduced_ping_timeout_seconds);
}

TEST_F(NetworkSessionConfiguratorTest,
       LargeQuicReducedPingTimeoutSecondsFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["reduced_ping_timeout_seconds"] = "50";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  EXPECT_EQ(net::kPingTimeoutSecs, params_.quic_reduced_ping_timeout_seconds);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicReducedPingTimeoutSecondsFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["reduced_ping_timeout_seconds"] = "10";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  EXPECT_EQ(10, params_.quic_reduced_ping_timeout_seconds);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicPacketReaderYieldAfterDurationMillisecondsFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["packet_reader_yield_after_duration_milliseconds"] = "10";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(10, params_.quic_packet_reader_yield_after_duration_milliseconds);
}

TEST_F(NetworkSessionConfiguratorTest, QuicRaceCertVerification) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["race_cert_verification"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_race_cert_verification);
}

TEST_F(NetworkSessionConfiguratorTest, QuicDoNotFragment) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["do_not_fragment"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_do_not_fragment);
}

TEST_F(NetworkSessionConfiguratorTest, QuicDisablePreConnectIfZeroRtt) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["disable_preconnect_if_0rtt"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_disable_preconnect_if_0rtt);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicMigrateSessionsOnNetworkChangeFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["migrate_sessions_on_network_change"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_migrate_sessions_on_network_change);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicMigrateSessionsEarlyFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["migrate_sessions_early"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_migrate_sessions_early);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicAllowServerMigrationFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["allow_server_migration"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_allow_server_migration);
}

TEST_F(NetworkSessionConfiguratorTest, PacketLengthFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["max_packet_length"] = "1450";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(1450u, params_.quic_max_packet_length);
}

TEST_F(NetworkSessionConfiguratorTest, QuicVersionFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["quic_version"] =
      net::QuicVersionToString(net::AllSupportedVersions().back());
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  net::QuicVersionVector supported_versions;
  supported_versions.push_back(net::AllSupportedVersions().back());
  EXPECT_EQ(supported_versions, params_.quic_supported_versions);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicConnectionOptionsFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["connection_options"] = "TIME,TBBR,REJ";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  net::QuicTagVector options;
  options.push_back(net::kTIME);
  options.push_back(net::kTBBR);
  options.push_back(net::kREJ);
  EXPECT_EQ(options, params_.quic_connection_options);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicAlwaysRequireHandshakeConfirmationFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["always_require_handshake_confirmation"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_always_require_handshake_confirmation);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicDisableConnectionPoolingFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["disable_connection_pooling"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_disable_connection_pooling);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicLoadServerInfoTimeToSmoothedRttFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["load_server_info_time_to_srtt"] = "0.5";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(0.5f, params_.quic_load_server_info_timeout_srtt_multiplier);
}

TEST_F(NetworkSessionConfiguratorTest, QuicEnableConnectionRacing) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["enable_connection_racing"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_enable_connection_racing);
}

TEST_F(NetworkSessionConfiguratorTest, QuicEnableNonBlockingIO) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["enable_non_blocking_io"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_enable_non_blocking_io);
}

TEST_F(NetworkSessionConfiguratorTest, QuicDisableDiskCache) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["disable_disk_cache"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_disable_disk_cache);
}

TEST_F(NetworkSessionConfiguratorTest, QuicPreferAes) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["prefer_aes"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_prefer_aes);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicEnableAlternativeServicesFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["enable_alternative_service_with_different_host"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_quic_alternative_service_with_different_host);
}

TEST_F(NetworkSessionConfiguratorTest, QuicReceiveBufferSize) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["receive_buffer_size"] = "2097152";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(2097152, params_.quic_socket_receive_buffer_size);
}

TEST_F(NetworkSessionConfiguratorTest, QuicDisableDelayTcpRace) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["disable_delay_tcp_race"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_FALSE(params_.quic_delay_tcp_race);
}

TEST_F(NetworkSessionConfiguratorTest, QuicWhitelistFromParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["quic_host_whitelist"] =
      "www.example.org, www.example.com";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(2u, params_.quic_host_whitelist.size());
  EXPECT_TRUE(
      base::ContainsKey(params_.quic_host_whitelist, "www.example.org"));
  EXPECT_TRUE(
      base::ContainsKey(params_.quic_host_whitelist, "www.example.com"));
}

TEST_F(NetworkSessionConfiguratorTest, TCPFastOpenHttpsEnabled) {
  base::FieldTrialList::CreateFieldTrial("TCPFastOpen", "HttpsEnabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_tcp_fast_open_for_ssl);
}

TEST_F(NetworkSessionConfiguratorTest, QuicForceHolBlocking) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["force_hol_blocking"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_force_hol_blocking);
}

}  // namespace test
