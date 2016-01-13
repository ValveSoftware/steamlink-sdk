// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/quic_crypto_client_config.h"

#include "net/quic/crypto/proof_verifier.h"
#include "net/quic/quic_server_id.h"
#include "net/quic/test_tools/mock_random.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;
using std::vector;

namespace net {
namespace test {

TEST(QuicCryptoClientConfigTest, CachedState_IsEmpty) {
  QuicCryptoClientConfig::CachedState state;
  EXPECT_TRUE(state.IsEmpty());
}

TEST(QuicCryptoClientConfigTest, CachedState_IsComplete) {
  QuicCryptoClientConfig::CachedState state;
  EXPECT_FALSE(state.IsComplete(QuicWallTime::FromUNIXSeconds(0)));
}

TEST(QuicCryptoClientConfigTest, CachedState_GenerationCounter) {
  QuicCryptoClientConfig::CachedState state;
  EXPECT_EQ(0u, state.generation_counter());
  state.SetProofInvalid();
  EXPECT_EQ(1u, state.generation_counter());
}

TEST(QuicCryptoClientConfigTest, CachedState_SetProofVerifyDetails) {
  QuicCryptoClientConfig::CachedState state;
  EXPECT_TRUE(state.proof_verify_details() == NULL);
  ProofVerifyDetails* details = new ProofVerifyDetails;
  state.SetProofVerifyDetails(details);
  EXPECT_EQ(details, state.proof_verify_details());
}

TEST(QuicCryptoClientConfigTest, CachedState_InitializeFrom) {
  QuicCryptoClientConfig::CachedState state;
  QuicCryptoClientConfig::CachedState other;
  state.set_source_address_token("TOKEN");
  // TODO(rch): Populate other fields of |state|.
  other.InitializeFrom(state);
  EXPECT_EQ(state.server_config(), other.server_config());
  EXPECT_EQ(state.source_address_token(), other.source_address_token());
  EXPECT_EQ(state.certs(), other.certs());
  EXPECT_EQ(1u, other.generation_counter());
}

TEST(QuicCryptoClientConfigTest, InchoateChlo) {
  QuicCryptoClientConfig::CachedState state;
  QuicCryptoClientConfig config;
  QuicCryptoNegotiatedParameters params;
  CryptoHandshakeMessage msg;
  QuicServerId server_id("www.google.com", 80, false, PRIVACY_MODE_DISABLED);
  config.FillInchoateClientHello(server_id, QuicVersionMax(), &state,
                                 &params, &msg);

  QuicTag cver;
  EXPECT_EQ(QUIC_NO_ERROR, msg.GetUint32(kVER, &cver));
  EXPECT_EQ(QuicVersionToQuicTag(QuicVersionMax()), cver);
}

TEST(QuicCryptoClientConfigTest, PreferAesGcm) {
  QuicCryptoClientConfig config;
  config.SetDefaults();
  if (config.aead.size() > 1)
    EXPECT_NE(kAESG, config.aead[0]);
  config.PreferAesGcm();
  EXPECT_EQ(kAESG, config.aead[0]);
}

TEST(QuicCryptoClientConfigTest, InchoateChloSecure) {
  QuicCryptoClientConfig::CachedState state;
  QuicCryptoClientConfig config;
  QuicCryptoNegotiatedParameters params;
  CryptoHandshakeMessage msg;
  QuicServerId server_id("www.google.com", 443, true, PRIVACY_MODE_DISABLED);
  config.FillInchoateClientHello(server_id, QuicVersionMax(), &state,
                                 &params, &msg);

  QuicTag pdmd;
  EXPECT_EQ(QUIC_NO_ERROR, msg.GetUint32(kPDMD, &pdmd));
  EXPECT_EQ(kX509, pdmd);
}

TEST(QuicCryptoClientConfigTest, InchoateChloSecureNoEcdsa) {
  QuicCryptoClientConfig::CachedState state;
  QuicCryptoClientConfig config;
  config.DisableEcdsa();
  QuicCryptoNegotiatedParameters params;
  CryptoHandshakeMessage msg;
  QuicServerId server_id("www.google.com", 443, true, PRIVACY_MODE_DISABLED);
  config.FillInchoateClientHello(server_id, QuicVersionMax(), &state,
                                 &params, &msg);

  QuicTag pdmd;
  EXPECT_EQ(QUIC_NO_ERROR, msg.GetUint32(kPDMD, &pdmd));
  EXPECT_EQ(kX59R, pdmd);
}

TEST(QuicCryptoClientConfigTest, FillClientHello) {
  QuicCryptoClientConfig::CachedState state;
  QuicCryptoClientConfig config;
  QuicCryptoNegotiatedParameters params;
  QuicConnectionId kConnectionId = 1234;
  string error_details;
  MockRandom rand;
  CryptoHandshakeMessage chlo;
  QuicServerId server_id("www.google.com", 80, false, PRIVACY_MODE_DISABLED);
  config.FillClientHello(server_id,
                         kConnectionId,
                         QuicVersionMax(),
                         &state,
                         QuicWallTime::Zero(),
                         &rand,
                         NULL,  // channel_id_key
                         &params,
                         &chlo,
                         &error_details);

  // Verify that certain QuicTags have been set correctly in the CHLO.
  QuicTag cver;
  EXPECT_EQ(QUIC_NO_ERROR, chlo.GetUint32(kVER, &cver));
  EXPECT_EQ(QuicVersionToQuicTag(QuicVersionMax()), cver);
}

TEST(QuicCryptoClientConfigTest, ProcessServerDowngradeAttack) {
  QuicVersionVector supported_versions = QuicSupportedVersions();
  if (supported_versions.size() == 1) {
    // No downgrade attack is possible if the client only supports one version.
    return;
  }
  QuicTagVector supported_version_tags;
  for (size_t i = supported_versions.size(); i > 0; --i) {
    supported_version_tags.push_back(
        QuicVersionToQuicTag(supported_versions[i - 1]));
  }
  CryptoHandshakeMessage msg;
  msg.set_tag(kSHLO);
  msg.SetVector(kVER, supported_version_tags);

  QuicCryptoClientConfig::CachedState cached;
  QuicCryptoNegotiatedParameters out_params;
  string error;
  QuicCryptoClientConfig config;
  EXPECT_EQ(QUIC_VERSION_NEGOTIATION_MISMATCH,
            config.ProcessServerHello(msg, 0, supported_versions,
                                      &cached, &out_params, &error));
  EXPECT_EQ("Downgrade attack detected", error);
}

TEST(QuicCryptoClientConfigTest, InitializeFrom) {
  QuicCryptoClientConfig config;
  QuicServerId canonical_server_id("www.google.com", 80, false,
                                   PRIVACY_MODE_DISABLED);
  QuicCryptoClientConfig::CachedState* state =
      config.LookupOrCreate(canonical_server_id);
  // TODO(rch): Populate other fields of |state|.
  state->set_source_address_token("TOKEN");
  state->SetProofValid();

  QuicServerId other_server_id("mail.google.com", 80, false,
                               PRIVACY_MODE_DISABLED);
  config.InitializeFrom(other_server_id, canonical_server_id, &config);
  QuicCryptoClientConfig::CachedState* other =
      config.LookupOrCreate(other_server_id);

  EXPECT_EQ(state->server_config(), other->server_config());
  EXPECT_EQ(state->source_address_token(), other->source_address_token());
  EXPECT_EQ(state->certs(), other->certs());
  EXPECT_EQ(1u, other->generation_counter());
}

TEST(QuicCryptoClientConfigTest, Canonical) {
  QuicCryptoClientConfig config;
  config.AddCanonicalSuffix(".google.com");
  QuicServerId canonical_id1("www.google.com", 80, false,
                             PRIVACY_MODE_DISABLED);
  QuicServerId canonical_id2("mail.google.com", 80, false,
                             PRIVACY_MODE_DISABLED);
  QuicCryptoClientConfig::CachedState* state =
      config.LookupOrCreate(canonical_id1);
  // TODO(rch): Populate other fields of |state|.
  state->set_source_address_token("TOKEN");
  state->SetProofValid();

  QuicCryptoClientConfig::CachedState* other =
      config.LookupOrCreate(canonical_id2);

  EXPECT_TRUE(state->IsEmpty());
  EXPECT_EQ(state->server_config(), other->server_config());
  EXPECT_EQ(state->source_address_token(), other->source_address_token());
  EXPECT_EQ(state->certs(), other->certs());
  EXPECT_EQ(1u, other->generation_counter());

  QuicServerId different_id("mail.google.org", 80, false,
                            PRIVACY_MODE_DISABLED);
  EXPECT_TRUE(config.LookupOrCreate(different_id)->IsEmpty());
}

TEST(QuicCryptoClientConfigTest, CanonicalNotUsedIfNotValid) {
  QuicCryptoClientConfig config;
  config.AddCanonicalSuffix(".google.com");
  QuicServerId canonical_id1("www.google.com", 80, false,
                             PRIVACY_MODE_DISABLED);
  QuicServerId canonical_id2("mail.google.com", 80, false,
                             PRIVACY_MODE_DISABLED);
  QuicCryptoClientConfig::CachedState* state =
      config.LookupOrCreate(canonical_id1);
  // TODO(rch): Populate other fields of |state|.
  state->set_source_address_token("TOKEN");

  // Do not set the proof as valid, and check that it is not used
  // as a canonical entry.
  EXPECT_TRUE(config.LookupOrCreate(canonical_id2)->IsEmpty());
}

TEST(QuicCryptoClientConfigTest, ClearCachedStates) {
  QuicCryptoClientConfig config;
  QuicServerId server_id("www.google.com", 80, false, PRIVACY_MODE_DISABLED);
  QuicCryptoClientConfig::CachedState* state = config.LookupOrCreate(server_id);
  // TODO(rch): Populate other fields of |state|.
  vector<string> certs(1);
  certs[0] = "Hello Cert";
  state->SetProof(certs, "signature");
  state->set_source_address_token("TOKEN");
  state->SetProofValid();
  EXPECT_EQ(1u, state->generation_counter());

  // Verify LookupOrCreate returns the same data.
  QuicCryptoClientConfig::CachedState* other = config.LookupOrCreate(server_id);

  EXPECT_EQ(state, other);
  EXPECT_EQ(1u, other->generation_counter());

  // Clear the cached states.
  config.ClearCachedStates();

  // Verify LookupOrCreate doesn't have any data.
  QuicCryptoClientConfig::CachedState* cleared_cache =
      config.LookupOrCreate(server_id);

  EXPECT_EQ(state, cleared_cache);
  EXPECT_FALSE(cleared_cache->proof_valid());
  EXPECT_TRUE(cleared_cache->server_config().empty());
  EXPECT_TRUE(cleared_cache->certs().empty());
  EXPECT_TRUE(cleared_cache->signature().empty());
  EXPECT_EQ(2u, cleared_cache->generation_counter());
}

}  // namespace test
}  // namespace net
