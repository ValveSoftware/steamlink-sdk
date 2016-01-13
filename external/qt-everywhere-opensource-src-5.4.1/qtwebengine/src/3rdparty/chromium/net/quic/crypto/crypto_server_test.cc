// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>
#include <vector>

#include "base/basictypes.h"
#include "base/strings/string_number_conversions.h"
#include "crypto/secure_hash.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/crypto/quic_crypto_server_config.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_socket_address_coder.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/delayed_verify_strike_register_client.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/mock_random.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using std::ostream;
using std::string;
using std::vector;

namespace net {
namespace test {

class QuicCryptoServerConfigPeer {
 public:
  explicit QuicCryptoServerConfigPeer(QuicCryptoServerConfig* server_config)
      : server_config_(server_config) {}

  base::Lock* GetStrikeRegisterClientLock() {
    return &server_config_->strike_register_client_lock_;
  }

 private:
  QuicCryptoServerConfig* server_config_;
};

// Run tests with combinations of
// {FLAGS_use_early_return_when_verifying_chlo,
//  FLAGS_send_quic_crypto_reject_reason}.
struct TestParams {
  TestParams(bool use_early_return_when_verifying_chlo,
             bool send_quic_crypto_reject_reason)
      : use_early_return_when_verifying_chlo(
            use_early_return_when_verifying_chlo),
        send_quic_crypto_reject_reason(send_quic_crypto_reject_reason) {
  }

  friend ostream& operator<<(ostream& os, const TestParams& p) {
    os << "{ use_early_return_when_verifying_chlo: "
       << p.use_early_return_when_verifying_chlo
       << " send_quic_crypto_reject_reason: "
       << p.send_quic_crypto_reject_reason << " }";
    return os;
  }

  bool use_early_return_when_verifying_chlo;
  bool send_quic_crypto_reject_reason;
};

// Constructs various test permutations.
vector<TestParams> GetTestParams() {
  vector<TestParams> params;
  params.push_back(TestParams(false, false));
  params.push_back(TestParams(false, true));
  params.push_back(TestParams(true, false));
  params.push_back(TestParams(true, true));
  return params;
}

class CryptoServerTest : public ::testing::TestWithParam<TestParams> {
 public:
  CryptoServerTest()
      : rand_(QuicRandom::GetInstance()),
        client_address_(Loopback4(), 1234),
        config_(QuicCryptoServerConfig::TESTING, rand_) {
    config_.SetProofSource(CryptoTestUtils::ProofSourceForTesting());
    supported_versions_ = QuicSupportedVersions();
    client_version_ = QuicUtils::TagToString(
        QuicVersionToQuicTag(supported_versions_.front()));

    FLAGS_use_early_return_when_verifying_chlo =
        GetParam().use_early_return_when_verifying_chlo;
    FLAGS_send_quic_crypto_reject_reason =
        GetParam().send_quic_crypto_reject_reason;
  }

  virtual void SetUp() {
    scoped_ptr<CryptoHandshakeMessage> msg(
        config_.AddDefaultConfig(rand_, &clock_,
        config_options_));

    StringPiece orbit;
    CHECK(msg->GetStringPiece(kORBT, &orbit));
    CHECK_EQ(sizeof(orbit_), orbit.size());
    memcpy(orbit_, orbit.data(), orbit.size());

    char public_value[32];
    memset(public_value, 42, sizeof(public_value));

    const string nonce_str = GenerateNonce();
    nonce_hex_ = "#" + base::HexEncode(nonce_str.data(), nonce_str.size());
    pub_hex_ = "#" + base::HexEncode(public_value, sizeof(public_value));

    CryptoHandshakeMessage client_hello = CryptoTestUtils::Message(
        "CHLO",
        "AEAD", "AESG",
        "KEXS", "C255",
        "PUBS", pub_hex_.c_str(),
        "NONC", nonce_hex_.c_str(),
        "VER\0", client_version_.data(),
        "$padding", static_cast<int>(kClientHelloMinimumSize),
        NULL);
    ShouldSucceed(client_hello);
    // The message should be rejected because the source-address token is
    // missing.
    ASSERT_EQ(kREJ, out_.tag());
    const HandshakeFailureReason kRejectReasons[] = {
      SERVER_CONFIG_INCHOATE_HELLO_FAILURE
    };
    CheckRejectReasons(kRejectReasons, arraysize(kRejectReasons));

    StringPiece srct;
    ASSERT_TRUE(out_.GetStringPiece(kSourceAddressTokenTag, &srct));
    srct_hex_ = "#" + base::HexEncode(srct.data(), srct.size());

    StringPiece scfg;
    ASSERT_TRUE(out_.GetStringPiece(kSCFG, &scfg));
    server_config_.reset(CryptoFramer::ParseMessage(scfg));

    StringPiece scid;
    ASSERT_TRUE(server_config_->GetStringPiece(kSCID, &scid));
    scid_hex_ = "#" + base::HexEncode(scid.data(), scid.size());
  }

  // Helper used to accept the result of ValidateClientHello and pass
  // it on to ProcessClientHello.
  class ValidateCallback : public ValidateClientHelloResultCallback {
   public:
    ValidateCallback(CryptoServerTest* test,
                     bool should_succeed,
                     const char* error_substr,
                     bool* called)
        : test_(test),
          should_succeed_(should_succeed),
          error_substr_(error_substr),
          called_(called) {
      *called_ = false;
    }

    virtual void RunImpl(const CryptoHandshakeMessage& client_hello,
                         const Result& result) OVERRIDE {
      {
        // Ensure that the strike register client lock is not held.
        QuicCryptoServerConfigPeer peer(&test_->config_);
        base::Lock* m = peer.GetStrikeRegisterClientLock();
        // In Chromium, we will dead lock if the lock is held by the current
        // thread. Chromium doesn't have AssertNotHeld API call.
        // m->AssertNotHeld();
        base::AutoLock lock(*m);
      }
      ASSERT_FALSE(*called_);
      test_->ProcessValidationResult(
          client_hello, result, should_succeed_, error_substr_);
      *called_ = true;
    }

   private:
    CryptoServerTest* test_;
    bool should_succeed_;
    const char* error_substr_;
    bool* called_;
  };

  void CheckServerHello(const CryptoHandshakeMessage& server_hello) {
    const QuicTag* versions;
    size_t num_versions;
    server_hello.GetTaglist(kVER, &versions, &num_versions);
    ASSERT_EQ(QuicSupportedVersions().size(), num_versions);
    for (size_t i = 0; i < num_versions; ++i) {
      EXPECT_EQ(QuicVersionToQuicTag(QuicSupportedVersions()[i]), versions[i]);
    }

    StringPiece address;
    ASSERT_TRUE(server_hello.GetStringPiece(kCADR, &address));
    QuicSocketAddressCoder decoder;
    ASSERT_TRUE(decoder.Decode(address.data(), address.size()));
    EXPECT_EQ(client_address_.address(), decoder.ip());
    EXPECT_EQ(client_address_.port(), decoder.port());
  }

  void ShouldSucceed(const CryptoHandshakeMessage& message) {
    bool called = false;
    RunValidate(message, new ValidateCallback(this, true, "", &called));
    EXPECT_TRUE(called);
  }

  void RunValidate(
      const CryptoHandshakeMessage& message,
      ValidateClientHelloResultCallback* cb) {
    config_.ValidateClientHello(message, client_address_, &clock_, cb);
  }

  void ShouldFailMentioning(const char* error_substr,
                            const CryptoHandshakeMessage& message) {
    bool called = false;
    ShouldFailMentioning(error_substr, message, &called);
    EXPECT_TRUE(called);
  }

  void ShouldFailMentioning(const char* error_substr,
                            const CryptoHandshakeMessage& message,
                            bool* called) {
    config_.ValidateClientHello(
        message, client_address_, &clock_,
        new ValidateCallback(this, false, error_substr, called));
  }

  void ProcessValidationResult(const CryptoHandshakeMessage& message,
                               const ValidateCallback::Result& result,
                               bool should_succeed,
                               const char* error_substr) {
    string error_details;
    QuicErrorCode error = config_.ProcessClientHello(
        result, 1 /* ConnectionId */, client_address_,
        supported_versions_.front(), supported_versions_, &clock_, rand_,
        &params_, &out_, &error_details);

    if (should_succeed) {
      ASSERT_EQ(error, QUIC_NO_ERROR)
          << "Message failed with error " << error_details << ": "
          << message.DebugString();
    } else {
      ASSERT_NE(error, QUIC_NO_ERROR)
          << "Message didn't fail: " << message.DebugString();

      EXPECT_TRUE(error_details.find(error_substr) != string::npos)
          << error_substr << " not in " << error_details;
    }
  }

  CryptoHandshakeMessage InchoateClientHello(const char* message_tag, ...) {
    va_list ap;
    va_start(ap, message_tag);

    CryptoHandshakeMessage message =
        CryptoTestUtils::BuildMessage(message_tag, ap);
    va_end(ap);

    message.SetStringPiece(kPAD, string(kClientHelloMinimumSize, '-'));
    return message;
  }

  string GenerateNonce() {
    string nonce;
    CryptoUtils::GenerateNonce(
        clock_.WallNow(), rand_,
        StringPiece(reinterpret_cast<const char*>(orbit_), sizeof(orbit_)),
        &nonce);
    return nonce;
  }

  void CheckRejectReasons(
      const HandshakeFailureReason* expected_handshake_failures,
      size_t expected_count) {
    const QuicTag* reject_reason_tags;
    size_t num_reject_reasons;
    QuicErrorCode error_code = out_.GetTaglist(kRREJ, &reject_reason_tags,
                                               &num_reject_reasons);
    if (!FLAGS_send_quic_crypto_reject_reason) {
      ASSERT_EQ(QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND, error_code);
      return;
    }
    ASSERT_EQ(QUIC_NO_ERROR, error_code);

    if (FLAGS_use_early_return_when_verifying_chlo) {
      EXPECT_EQ(1u, num_reject_reasons);
    } else {
      EXPECT_EQ(expected_count, num_reject_reasons);
    }
    for (size_t i = 0; i < num_reject_reasons; ++i) {
      EXPECT_EQ(expected_handshake_failures[i], reject_reason_tags[i]);
    }
  }

 protected:
  QuicRandom* const rand_;
  MockClock clock_;
  const IPEndPoint client_address_;
  QuicVersionVector supported_versions_;
  string client_version_;
  QuicCryptoServerConfig config_;
  QuicCryptoServerConfig::ConfigOptions config_options_;
  QuicCryptoNegotiatedParameters params_;
  CryptoHandshakeMessage out_;
  uint8 orbit_[kOrbitSize];

  // These strings contain hex escaped values from the server suitable for
  // passing to |InchoateClientHello| when constructing client hello messages.
  string nonce_hex_, pub_hex_, srct_hex_, scid_hex_;
  scoped_ptr<CryptoHandshakeMessage> server_config_;
};

// Run all CryptoServerTest with all combinations of
// FLAGS_use_early_return_when_verifying_chlo and
// FLAGS_send_quic_crypto_reject_reason.
INSTANTIATE_TEST_CASE_P(CryptoServerTests,
                        CryptoServerTest,
                        ::testing::ValuesIn(GetTestParams()));

TEST_P(CryptoServerTest, BadSNI) {
  static const char* kBadSNIs[] = {
    "",
    "foo",
    "#00",
    "#ff00",
    "127.0.0.1",
    "ffee::1",
  };

  string client_version = QuicUtils::TagToString(
      QuicVersionToQuicTag(supported_versions_.front()));

  for (size_t i = 0; i < arraysize(kBadSNIs); i++) {
    ShouldFailMentioning("SNI", InchoateClientHello(
        "CHLO",
        "SNI", kBadSNIs[i],
        "VER\0", client_version.data(),
        NULL));
    const HandshakeFailureReason kRejectReasons[] = {
      SERVER_CONFIG_INCHOATE_HELLO_FAILURE
    };
    CheckRejectReasons(kRejectReasons, arraysize(kRejectReasons));
  }
}

// TODO(rtenneti): Enable the DefaultCert test after implementing ProofSource.
TEST_F(CryptoServerTest, DISABLED_DefaultCert) {
  // Check that the server replies with a default certificate when no SNI is
  // specified.
  ShouldSucceed(InchoateClientHello(
      "CHLO",
      "AEAD", "AESG",
      "KEXS", "C255",
      "SCID", scid_hex_.c_str(),
      "#004b5453", srct_hex_.c_str(),
      "PUBS", pub_hex_.c_str(),
      "NONC", nonce_hex_.c_str(),
      "$padding", static_cast<int>(kClientHelloMinimumSize),
      "PDMD", "X509",
      "VER\0", client_version_.data(),
      NULL));

  StringPiece cert, proof;
  EXPECT_TRUE(out_.GetStringPiece(kCertificateTag, &cert));
  EXPECT_TRUE(out_.GetStringPiece(kPROF, &proof));
  EXPECT_NE(0u, cert.size());
  EXPECT_NE(0u, proof.size());
  const HandshakeFailureReason kRejectReasons[] = {
    CLIENT_NONCE_UNKNOWN_FAILURE
  };
  CheckRejectReasons(kRejectReasons, arraysize(kRejectReasons));
}

TEST_P(CryptoServerTest, TooSmall) {
  ShouldFailMentioning("too small", CryptoTestUtils::Message(
        "CHLO",
        "VER\0", client_version_.data(),
        NULL));
  const HandshakeFailureReason kRejectReasons[] = {
    SERVER_CONFIG_INCHOATE_HELLO_FAILURE
  };
  CheckRejectReasons(kRejectReasons, arraysize(kRejectReasons));
}

TEST_P(CryptoServerTest, BadSourceAddressToken) {
  // Invalid source-address tokens should be ignored.
  static const char* kBadSourceAddressTokens[] = {
    "",
    "foo",
    "#0000",
    "#0000000000000000000000000000000000000000",
  };

  for (size_t i = 0; i < arraysize(kBadSourceAddressTokens); i++) {
    ShouldSucceed(InchoateClientHello(
        "CHLO",
        "STK", kBadSourceAddressTokens[i],
        "VER\0", client_version_.data(),
        NULL));
    const HandshakeFailureReason kRejectReasons[] = {
      SERVER_CONFIG_INCHOATE_HELLO_FAILURE
    };
    CheckRejectReasons(kRejectReasons, arraysize(kRejectReasons));
  }
}

TEST_P(CryptoServerTest, BadClientNonce) {
  // Invalid nonces should be ignored.
  static const char* kBadNonces[] = {
    "",
    "#0000",
    "#0000000000000000000000000000000000000000",
  };

  for (size_t i = 0; i < arraysize(kBadNonces); i++) {
    ShouldSucceed(InchoateClientHello(
        "CHLO",
        "NONC", kBadNonces[i],
        "VER\0", client_version_.data(),
        NULL));
    const HandshakeFailureReason kRejectReasons[] = {
      SERVER_CONFIG_INCHOATE_HELLO_FAILURE
    };
    CheckRejectReasons(kRejectReasons, arraysize(kRejectReasons));
  }
}

TEST_P(CryptoServerTest, DowngradeAttack) {
  if (supported_versions_.size() == 1) {
    // No downgrade attack is possible if the server only supports one version.
    return;
  }
  // Set the client's preferred version to a supported version that
  // is not the "current" version (supported_versions_.front()).
  string bad_version = QuicUtils::TagToString(
      QuicVersionToQuicTag(supported_versions_.back()));

  ShouldFailMentioning("Downgrade", InchoateClientHello(
      "CHLO",
      "VER\0", bad_version.data(),
      NULL));
  const HandshakeFailureReason kRejectReasons[] = {
    SERVER_CONFIG_INCHOATE_HELLO_FAILURE
  };
  CheckRejectReasons(kRejectReasons, arraysize(kRejectReasons));
}

TEST_P(CryptoServerTest, CorruptServerConfig) {
  // This tests corrupted server config.
  CryptoHandshakeMessage msg = CryptoTestUtils::Message(
      "CHLO",
      "AEAD", "AESG",
      "KEXS", "C255",
      "SCID", (string(1, 'X') + scid_hex_).c_str(),
      "#004b5453", srct_hex_.c_str(),
      "PUBS", pub_hex_.c_str(),
      "NONC", nonce_hex_.c_str(),
      "VER\0", client_version_.data(),
      "$padding", static_cast<int>(kClientHelloMinimumSize),
      NULL);
  ShouldSucceed(msg);
  ASSERT_EQ(kREJ, out_.tag());
  const HandshakeFailureReason kRejectReasons[] = {
    SERVER_CONFIG_UNKNOWN_CONFIG_FAILURE
  };
  CheckRejectReasons(kRejectReasons, arraysize(kRejectReasons));
}

TEST_P(CryptoServerTest, CorruptSourceAddressToken) {
  // This tests corrupted source address token.
  CryptoHandshakeMessage msg = CryptoTestUtils::Message(
      "CHLO",
      "AEAD", "AESG",
      "KEXS", "C255",
      "SCID", scid_hex_.c_str(),
      "#004b5453", (string(1, 'X') + srct_hex_).c_str(),
      "PUBS", pub_hex_.c_str(),
      "NONC", nonce_hex_.c_str(),
      "VER\0", client_version_.data(),
      "$padding", static_cast<int>(kClientHelloMinimumSize),
      NULL);
  ShouldSucceed(msg);
  ASSERT_EQ(kREJ, out_.tag());
  const HandshakeFailureReason kRejectReasons[] = {
    SOURCE_ADDRESS_TOKEN_DECRYPTION_FAILURE
  };
  CheckRejectReasons(kRejectReasons, arraysize(kRejectReasons));
}

TEST_P(CryptoServerTest, CorruptClientNonceAndSourceAddressToken) {
  // This test corrupts client nonce and source address token.
  CryptoHandshakeMessage msg = CryptoTestUtils::Message(
      "CHLO",
      "AEAD", "AESG",
      "KEXS", "C255",
      "SCID", scid_hex_.c_str(),
      "#004b5453", (string(1, 'X') + srct_hex_).c_str(),
      "PUBS", pub_hex_.c_str(),
      "NONC", (string(1, 'X') + nonce_hex_).c_str(),
      "VER\0", client_version_.data(),
      "$padding", static_cast<int>(kClientHelloMinimumSize),
      NULL);
  ShouldSucceed(msg);
  ASSERT_EQ(kREJ, out_.tag());
  const HandshakeFailureReason kRejectReasons[] = {
    SOURCE_ADDRESS_TOKEN_DECRYPTION_FAILURE,
    CLIENT_NONCE_INVALID_FAILURE
  };
  CheckRejectReasons(kRejectReasons, arraysize(kRejectReasons));
}

TEST_P(CryptoServerTest, CorruptMultipleTags) {
  // This test corrupts client nonce, server nonce and source address token.
  CryptoHandshakeMessage msg = CryptoTestUtils::Message(
      "CHLO",
      "AEAD", "AESG",
      "KEXS", "C255",
      "SCID", scid_hex_.c_str(),
      "#004b5453", (string(1, 'X') + srct_hex_).c_str(),
      "PUBS", pub_hex_.c_str(),
      "NONC", (string(1, 'X') + nonce_hex_).c_str(),
      "SNO\0", (string(1, 'X') + nonce_hex_).c_str(),
      "VER\0", client_version_.data(),
      "$padding", static_cast<int>(kClientHelloMinimumSize),
      NULL);
  ShouldSucceed(msg);
  ASSERT_EQ(kREJ, out_.tag());
  const HandshakeFailureReason kRejectReasons[] = {
    SOURCE_ADDRESS_TOKEN_DECRYPTION_FAILURE,
    CLIENT_NONCE_INVALID_FAILURE,
    SERVER_NONCE_DECRYPTION_FAILURE,
  };
  CheckRejectReasons(kRejectReasons, arraysize(kRejectReasons));
}

TEST_P(CryptoServerTest, ReplayProtection) {
  // This tests that disabling replay protection works.
  CryptoHandshakeMessage msg = CryptoTestUtils::Message(
      "CHLO",
      "AEAD", "AESG",
      "KEXS", "C255",
      "SCID", scid_hex_.c_str(),
      "#004b5453", srct_hex_.c_str(),
      "PUBS", pub_hex_.c_str(),
      "NONC", nonce_hex_.c_str(),
      "VER\0", client_version_.data(),
      "$padding", static_cast<int>(kClientHelloMinimumSize),
      NULL);
  ShouldSucceed(msg);
  // The message should be rejected because the strike-register is still
  // quiescent.
  ASSERT_EQ(kREJ, out_.tag());

  const HandshakeFailureReason kRejectReasons[] = {
    CLIENT_NONCE_UNKNOWN_FAILURE
  };
  CheckRejectReasons(kRejectReasons, arraysize(kRejectReasons));

  config_.set_replay_protection(false);

  ShouldSucceed(msg);
  // The message should be accepted now.
  ASSERT_EQ(kSHLO, out_.tag());
  CheckServerHello(out_);

  ShouldSucceed(msg);
  // The message should accepted twice when replay protection is off.
  ASSERT_EQ(kSHLO, out_.tag());
  CheckServerHello(out_);
}

TEST(CryptoServerConfigGenerationTest, Determinism) {
  // Test that using a deterministic PRNG causes the server-config to be
  // deterministic.

  MockRandom rand_a, rand_b;
  const QuicCryptoServerConfig::ConfigOptions options;
  MockClock clock;

  QuicCryptoServerConfig a(QuicCryptoServerConfig::TESTING, &rand_a);
  QuicCryptoServerConfig b(QuicCryptoServerConfig::TESTING, &rand_b);
  scoped_ptr<CryptoHandshakeMessage> scfg_a(
      a.AddDefaultConfig(&rand_a, &clock, options));
  scoped_ptr<CryptoHandshakeMessage> scfg_b(
      b.AddDefaultConfig(&rand_b, &clock, options));

  ASSERT_EQ(scfg_a->DebugString(), scfg_b->DebugString());
}

TEST(CryptoServerConfigGenerationTest, SCIDVaries) {
  // This test ensures that the server config ID varies for different server
  // configs.

  MockRandom rand_a, rand_b;
  const QuicCryptoServerConfig::ConfigOptions options;
  MockClock clock;

  QuicCryptoServerConfig a(QuicCryptoServerConfig::TESTING, &rand_a);
  rand_b.ChangeValue();
  QuicCryptoServerConfig b(QuicCryptoServerConfig::TESTING, &rand_b);
  scoped_ptr<CryptoHandshakeMessage> scfg_a(
      a.AddDefaultConfig(&rand_a, &clock, options));
  scoped_ptr<CryptoHandshakeMessage> scfg_b(
      b.AddDefaultConfig(&rand_b, &clock, options));

  StringPiece scid_a, scid_b;
  EXPECT_TRUE(scfg_a->GetStringPiece(kSCID, &scid_a));
  EXPECT_TRUE(scfg_b->GetStringPiece(kSCID, &scid_b));

  EXPECT_NE(scid_a, scid_b);
}


TEST(CryptoServerConfigGenerationTest, SCIDIsHashOfServerConfig) {
  MockRandom rand_a;
  const QuicCryptoServerConfig::ConfigOptions options;
  MockClock clock;

  QuicCryptoServerConfig a(QuicCryptoServerConfig::TESTING, &rand_a);
  scoped_ptr<CryptoHandshakeMessage> scfg(
      a.AddDefaultConfig(&rand_a, &clock, options));

  StringPiece scid;
  EXPECT_TRUE(scfg->GetStringPiece(kSCID, &scid));
  // Need to take a copy of |scid| has we're about to call |Erase|.
  const string scid_str(scid.as_string());

  scfg->Erase(kSCID);
  scfg->MarkDirty();
  const QuicData& serialized(scfg->GetSerialized());

  scoped_ptr<crypto::SecureHash> hash(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));
  hash->Update(serialized.data(), serialized.length());
  uint8 digest[16];
  hash->Finish(digest, sizeof(digest));

  ASSERT_EQ(scid.size(), sizeof(digest));
  EXPECT_EQ(0, memcmp(digest, scid_str.data(), sizeof(digest)));
}

class CryptoServerTestNoConfig : public CryptoServerTest {
 public:
  virtual void SetUp() {
    // Deliberately don't add a config so that we can test this situation.
  }
};

TEST_P(CryptoServerTestNoConfig, DontCrash) {
  ShouldFailMentioning("No config", InchoateClientHello(
      "CHLO",
      "VER\0", client_version_.data(),
      NULL));

  const HandshakeFailureReason kRejectReasons[] = {
    CLIENT_NONCE_UNKNOWN_FAILURE
  };
  CheckRejectReasons(kRejectReasons, arraysize(kRejectReasons));
}

class AsyncStrikeServerVerificationTest : public CryptoServerTest {
 protected:
  AsyncStrikeServerVerificationTest() {
  }

  virtual void SetUp() {
    const string kOrbit = "12345678";
    config_options_.orbit = kOrbit;
    strike_register_client_ = new DelayedVerifyStrikeRegisterClient(
        10000,  // strike_register_max_entries
        static_cast<uint32>(clock_.WallNow().ToUNIXSeconds()),
        60,  // strike_register_window_secs
        reinterpret_cast<const uint8 *>(kOrbit.data()),
        StrikeRegister::NO_STARTUP_PERIOD_NEEDED);
    config_.SetStrikeRegisterClient(strike_register_client_);
    CryptoServerTest::SetUp();
    strike_register_client_->StartDelayingVerification();
  }

  DelayedVerifyStrikeRegisterClient* strike_register_client_;
};

TEST_P(AsyncStrikeServerVerificationTest, AsyncReplayProtection) {
  // This tests async validation with a strike register works.
  CryptoHandshakeMessage msg = CryptoTestUtils::Message(
      "CHLO",
      "AEAD", "AESG",
      "KEXS", "C255",
      "SCID", scid_hex_.c_str(),
      "#004b5453", srct_hex_.c_str(),
      "PUBS", pub_hex_.c_str(),
      "NONC", nonce_hex_.c_str(),
      "VER\0", client_version_.data(),
      "$padding", static_cast<int>(kClientHelloMinimumSize),
      NULL);

  // Clear the message tag.
  out_.set_tag(0);

  bool called = false;
  RunValidate(msg, new ValidateCallback(this, true, "", &called));
  // The verification request was queued.
  ASSERT_FALSE(called);
  EXPECT_EQ(0u, out_.tag());
  EXPECT_EQ(1, strike_register_client_->PendingVerifications());

  // Continue processing the verification request.
  strike_register_client_->RunPendingVerifications();
  ASSERT_TRUE(called);
  EXPECT_EQ(0, strike_register_client_->PendingVerifications());
  // The message should be accepted now.
  EXPECT_EQ(kSHLO, out_.tag());

  // Rejected if replayed.
  RunValidate(msg, new ValidateCallback(this, true, "", &called));
  // The verification request was queued.
  ASSERT_FALSE(called);
  EXPECT_EQ(1, strike_register_client_->PendingVerifications());

  strike_register_client_->RunPendingVerifications();
  ASSERT_TRUE(called);
  EXPECT_EQ(0, strike_register_client_->PendingVerifications());
  // The message should be rejected now.
  EXPECT_EQ(kREJ, out_.tag());
}

}  // namespace test
}  // namespace net
