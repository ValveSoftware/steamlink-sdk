// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/crypto_test_utils.h"

#include "net/quic/crypto/channel_id.h"
#include "net/quic/crypto/common_cert_set.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/quic_crypto_server_config.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/quic_crypto_server_stream.h"
#include "net/quic/quic_crypto_stream.h"
#include "net/quic/quic_server_id.h"
#include "net/quic/test_tools/quic_connection_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/simple_quic_framer.h"

using base::StringPiece;
using std::make_pair;
using std::pair;
using std::string;
using std::vector;

namespace net {
namespace test {

namespace {

const char kServerHostname[] = "test.example.com";
const uint16 kServerPort = 80;

// CryptoFramerVisitor is a framer visitor that records handshake messages.
class CryptoFramerVisitor : public CryptoFramerVisitorInterface {
 public:
  CryptoFramerVisitor()
      : error_(false) {
  }

  virtual void OnError(CryptoFramer* framer) OVERRIDE { error_ = true; }

  virtual void OnHandshakeMessage(
      const CryptoHandshakeMessage& message) OVERRIDE {
    messages_.push_back(message);
  }

  bool error() const {
    return error_;
  }

  const vector<CryptoHandshakeMessage>& messages() const {
    return messages_;
  }

 private:
  bool error_;
  vector<CryptoHandshakeMessage> messages_;
};

// MovePackets parses crypto handshake messages from packet number
// |*inout_packet_index| through to the last packet and has |dest_stream|
// process them. |*inout_packet_index| is updated with an index one greater
// than the last packet processed.
void MovePackets(PacketSavingConnection* source_conn,
                 size_t *inout_packet_index,
                 QuicCryptoStream* dest_stream,
                 PacketSavingConnection* dest_conn) {
  SimpleQuicFramer framer(source_conn->supported_versions());
  CryptoFramer crypto_framer;
  CryptoFramerVisitor crypto_visitor;

  // In order to properly test the code we need to perform encryption and
  // decryption so that the crypters latch when expected. The crypters are in
  // |dest_conn|, but we don't want to try and use them there. Instead we swap
  // them into |framer|, perform the decryption with them, and then swap them
  // back.
  QuicConnectionPeer::SwapCrypters(dest_conn, framer.framer());

  crypto_framer.set_visitor(&crypto_visitor);

  size_t index = *inout_packet_index;
  for (; index < source_conn->encrypted_packets_.size(); index++) {
    ASSERT_TRUE(framer.ProcessPacket(*source_conn->encrypted_packets_[index]));
    for (vector<QuicStreamFrame>::const_iterator
         i =  framer.stream_frames().begin();
         i != framer.stream_frames().end(); ++i) {
      scoped_ptr<string> frame_data(i->GetDataAsString());
      ASSERT_TRUE(crypto_framer.ProcessInput(*frame_data));
      ASSERT_FALSE(crypto_visitor.error());
    }
  }
  *inout_packet_index = index;

  QuicConnectionPeer::SwapCrypters(dest_conn, framer.framer());

  ASSERT_EQ(0u, crypto_framer.InputBytesRemaining());

  for (vector<CryptoHandshakeMessage>::const_iterator
       i = crypto_visitor.messages().begin();
       i != crypto_visitor.messages().end(); ++i) {
    dest_stream->OnHandshakeMessage(*i);
  }
}

// HexChar parses |c| as a hex character. If valid, it sets |*value| to the
// value of the hex character and returns true. Otherwise it returns false.
bool HexChar(char c, uint8* value) {
  if (c >= '0' && c <= '9') {
    *value = c - '0';
    return true;
  }
  if (c >= 'a' && c <= 'f') {
    *value = c - 'a' + 10;
    return true;
  }
  if (c >= 'A' && c <= 'F') {
    *value = c - 'A' + 10;
    return true;
  }
  return false;
}

}  // anonymous namespace

CryptoTestUtils::FakeClientOptions::FakeClientOptions()
    : dont_verify_certs(false),
      channel_id_enabled(false) {
}

// static
int CryptoTestUtils::HandshakeWithFakeServer(
    PacketSavingConnection* client_conn,
    QuicCryptoClientStream* client) {
  PacketSavingConnection* server_conn =
      new PacketSavingConnection(true, client_conn->supported_versions());
  TestSession server_session(server_conn, DefaultQuicConfig());

  QuicCryptoServerConfig crypto_config(QuicCryptoServerConfig::TESTING,
                                       QuicRandom::GetInstance());
  SetupCryptoServerConfigForTest(
      server_session.connection()->clock(),
      server_session.connection()->random_generator(),
      server_session.config(), &crypto_config);

  QuicCryptoServerStream server(crypto_config, &server_session);
  server_session.SetCryptoStream(&server);

  // The client's handshake must have been started already.
  CHECK_NE(0u, client_conn->packets_.size());

  CommunicateHandshakeMessages(client_conn, client, server_conn, &server);

  CompareClientAndServerKeys(client, &server);

  return client->num_sent_client_hellos();
}

// static
int CryptoTestUtils::HandshakeWithFakeClient(
    PacketSavingConnection* server_conn,
    QuicCryptoServerStream* server,
    const FakeClientOptions& options) {
  PacketSavingConnection* client_conn = new PacketSavingConnection(false);
  TestClientSession client_session(client_conn, DefaultQuicConfig());
  QuicCryptoClientConfig crypto_config;

  client_session.config()->SetDefaults();
  crypto_config.SetDefaults();
  // TODO(rtenneti): Enable testing of ProofVerifier.
  // if (!options.dont_verify_certs) {
  //   crypto_config.SetProofVerifier(ProofVerifierForTesting());
  // }
  if (options.channel_id_enabled) {
    crypto_config.SetChannelIDSource(ChannelIDSourceForTesting());
  }
  QuicServerId server_id(kServerHostname, kServerPort, false,
                         PRIVACY_MODE_DISABLED);
  QuicCryptoClientStream client(server_id, &client_session, NULL,
                                &crypto_config);
  client_session.SetCryptoStream(&client);

  CHECK(client.CryptoConnect());
  CHECK_EQ(1u, client_conn->packets_.size());

  CommunicateHandshakeMessages(client_conn, &client, server_conn, server);

  CompareClientAndServerKeys(&client, server);

  if (options.channel_id_enabled) {
    scoped_ptr<ChannelIDKey> channel_id_key;
    QuicAsyncStatus status =
        crypto_config.channel_id_source()->GetChannelIDKey(kServerHostname,
                                                           &channel_id_key,
                                                           NULL);
    EXPECT_EQ(QUIC_SUCCESS, status);
    EXPECT_EQ(channel_id_key->SerializeKey(),
              server->crypto_negotiated_params().channel_id);
  }

  return client.num_sent_client_hellos();
}

// static
void CryptoTestUtils::SetupCryptoServerConfigForTest(
    const QuicClock* clock,
    QuicRandom* rand,
    QuicConfig* config,
    QuicCryptoServerConfig* crypto_config) {
  config->SetDefaults();
  QuicCryptoServerConfig::ConfigOptions options;
  options.channel_id_enabled = true;
  scoped_ptr<CryptoHandshakeMessage> scfg(
      crypto_config->AddDefaultConfig(rand, clock, options));
}

// static
void CryptoTestUtils::CommunicateHandshakeMessages(
    PacketSavingConnection* a_conn,
    QuicCryptoStream* a,
    PacketSavingConnection* b_conn,
    QuicCryptoStream* b) {
  size_t a_i = 0, b_i = 0;
  while (!a->handshake_confirmed()) {
    ASSERT_GT(a_conn->packets_.size(), a_i);
    LOG(INFO) << "Processing " << a_conn->packets_.size() - a_i
              << " packets a->b";
    MovePackets(a_conn, &a_i, b, b_conn);

    ASSERT_GT(b_conn->packets_.size(), b_i);
    LOG(INFO) << "Processing " << b_conn->packets_.size() - b_i
              << " packets b->a";
    if (b_conn->packets_.size() - b_i == 2) {
      LOG(INFO) << "here";
    }
    MovePackets(b_conn, &b_i, a, a_conn);
  }
}

// static
pair<size_t, size_t> CryptoTestUtils::AdvanceHandshake(
    PacketSavingConnection* a_conn,
    QuicCryptoStream* a,
    size_t a_i,
    PacketSavingConnection* b_conn,
    QuicCryptoStream* b,
    size_t b_i) {
  LOG(INFO) << "Processing " << a_conn->packets_.size() - a_i
            << " packets a->b";
  MovePackets(a_conn, &a_i, b, b_conn);

  LOG(INFO) << "Processing " << b_conn->packets_.size() - b_i
            << " packets b->a";
  if (b_conn->packets_.size() - b_i == 2) {
    LOG(INFO) << "here";
  }
  MovePackets(b_conn, &b_i, a, a_conn);

  return make_pair(a_i, b_i);
}

// static
string CryptoTestUtils::GetValueForTag(const CryptoHandshakeMessage& message,
                                       QuicTag tag) {
  QuicTagValueMap::const_iterator it = message.tag_value_map().find(tag);
  if (it == message.tag_value_map().end()) {
    return string();
  }
  return it->second;
}

class MockCommonCertSets : public CommonCertSets {
 public:
  MockCommonCertSets(StringPiece cert, uint64 hash, uint32 index)
      : cert_(cert.as_string()),
        hash_(hash),
        index_(index) {
  }

  virtual StringPiece GetCommonHashes() const OVERRIDE {
    CHECK(false) << "not implemented";
    return StringPiece();
  }

  virtual StringPiece GetCert(uint64 hash, uint32 index) const OVERRIDE {
    if (hash == hash_ && index == index_) {
      return cert_;
    }
    return StringPiece();
  }

  virtual bool MatchCert(StringPiece cert,
                         StringPiece common_set_hashes,
                         uint64* out_hash,
                         uint32* out_index) const OVERRIDE {
    if (cert != cert_) {
      return false;
    }

    if (common_set_hashes.size() % sizeof(uint64) != 0) {
      return false;
    }
    bool client_has_set = false;
    for (size_t i = 0; i < common_set_hashes.size(); i += sizeof(uint64)) {
      uint64 hash;
      memcpy(&hash, common_set_hashes.data() + i, sizeof(hash));
      if (hash == hash_) {
        client_has_set = true;
        break;
      }
    }

    if (!client_has_set) {
      return false;
    }

    *out_hash = hash_;
    *out_index = index_;
    return true;
  }

 private:
  const string cert_;
  const uint64 hash_;
  const uint32 index_;
};

CommonCertSets* CryptoTestUtils::MockCommonCertSets(StringPiece cert,
                                                    uint64 hash,
                                                    uint32 index) {
  return new class MockCommonCertSets(cert, hash, index);
}

void CryptoTestUtils::CompareClientAndServerKeys(
    QuicCryptoClientStream* client,
    QuicCryptoServerStream* server) {
  const QuicEncrypter* client_encrypter(
      client->session()->connection()->encrypter(ENCRYPTION_INITIAL));
  const QuicDecrypter* client_decrypter(
      client->session()->connection()->decrypter());
  const QuicEncrypter* client_forward_secure_encrypter(
      client->session()->connection()->encrypter(ENCRYPTION_FORWARD_SECURE));
  const QuicDecrypter* client_forward_secure_decrypter(
      client->session()->connection()->alternative_decrypter());
  const QuicEncrypter* server_encrypter(
      server->session()->connection()->encrypter(ENCRYPTION_INITIAL));
  const QuicDecrypter* server_decrypter(
      server->session()->connection()->decrypter());
  const QuicEncrypter* server_forward_secure_encrypter(
      server->session()->connection()->encrypter(ENCRYPTION_FORWARD_SECURE));
  const QuicDecrypter* server_forward_secure_decrypter(
      server->session()->connection()->alternative_decrypter());

  StringPiece client_encrypter_key = client_encrypter->GetKey();
  StringPiece client_encrypter_iv = client_encrypter->GetNoncePrefix();
  StringPiece client_decrypter_key = client_decrypter->GetKey();
  StringPiece client_decrypter_iv = client_decrypter->GetNoncePrefix();
  StringPiece client_forward_secure_encrypter_key =
      client_forward_secure_encrypter->GetKey();
  StringPiece client_forward_secure_encrypter_iv =
      client_forward_secure_encrypter->GetNoncePrefix();
  StringPiece client_forward_secure_decrypter_key =
      client_forward_secure_decrypter->GetKey();
  StringPiece client_forward_secure_decrypter_iv =
      client_forward_secure_decrypter->GetNoncePrefix();
  StringPiece server_encrypter_key = server_encrypter->GetKey();
  StringPiece server_encrypter_iv = server_encrypter->GetNoncePrefix();
  StringPiece server_decrypter_key = server_decrypter->GetKey();
  StringPiece server_decrypter_iv = server_decrypter->GetNoncePrefix();
  StringPiece server_forward_secure_encrypter_key =
      server_forward_secure_encrypter->GetKey();
  StringPiece server_forward_secure_encrypter_iv =
      server_forward_secure_encrypter->GetNoncePrefix();
  StringPiece server_forward_secure_decrypter_key =
      server_forward_secure_decrypter->GetKey();
  StringPiece server_forward_secure_decrypter_iv =
      server_forward_secure_decrypter->GetNoncePrefix();

  CompareCharArraysWithHexError("client write key",
                                client_encrypter_key.data(),
                                client_encrypter_key.length(),
                                server_decrypter_key.data(),
                                server_decrypter_key.length());
  CompareCharArraysWithHexError("client write IV",
                                client_encrypter_iv.data(),
                                client_encrypter_iv.length(),
                                server_decrypter_iv.data(),
                                server_decrypter_iv.length());
  CompareCharArraysWithHexError("server write key",
                                server_encrypter_key.data(),
                                server_encrypter_key.length(),
                                client_decrypter_key.data(),
                                client_decrypter_key.length());
  CompareCharArraysWithHexError("server write IV",
                                server_encrypter_iv.data(),
                                server_encrypter_iv.length(),
                                client_decrypter_iv.data(),
                                client_decrypter_iv.length());
  CompareCharArraysWithHexError("client forward secure write key",
                                client_forward_secure_encrypter_key.data(),
                                client_forward_secure_encrypter_key.length(),
                                server_forward_secure_decrypter_key.data(),
                                server_forward_secure_decrypter_key.length());
  CompareCharArraysWithHexError("client forward secure write IV",
                                client_forward_secure_encrypter_iv.data(),
                                client_forward_secure_encrypter_iv.length(),
                                server_forward_secure_decrypter_iv.data(),
                                server_forward_secure_decrypter_iv.length());
  CompareCharArraysWithHexError("server forward secure write key",
                                server_forward_secure_encrypter_key.data(),
                                server_forward_secure_encrypter_key.length(),
                                client_forward_secure_decrypter_key.data(),
                                client_forward_secure_decrypter_key.length());
  CompareCharArraysWithHexError("server forward secure write IV",
                                server_forward_secure_encrypter_iv.data(),
                                server_forward_secure_encrypter_iv.length(),
                                client_forward_secure_decrypter_iv.data(),
                                client_forward_secure_decrypter_iv.length());
}

// static
QuicTag CryptoTestUtils::ParseTag(const char* tagstr) {
  const size_t len = strlen(tagstr);
  CHECK_NE(0u, len);

  QuicTag tag = 0;

  if (tagstr[0] == '#') {
    CHECK_EQ(static_cast<size_t>(1 + 2*4), len);
    tagstr++;

    for (size_t i = 0; i < 8; i++) {
      tag <<= 4;

      uint8 v = 0;
      CHECK(HexChar(tagstr[i], &v));
      tag |= v;
    }

    return tag;
  }

  CHECK_LE(len, 4u);
  for (size_t i = 0; i < 4; i++) {
    tag >>= 8;
    if (i < len) {
      tag |= static_cast<uint32>(tagstr[i]) << 24;
    }
  }

  return tag;
}

// static
CryptoHandshakeMessage CryptoTestUtils::Message(const char* message_tag, ...) {
  va_list ap;
  va_start(ap, message_tag);

  CryptoHandshakeMessage message = BuildMessage(message_tag, ap);
  va_end(ap);
  return message;
}

// static
CryptoHandshakeMessage CryptoTestUtils::BuildMessage(const char* message_tag,
                                                     va_list ap) {
  CryptoHandshakeMessage msg;
  msg.set_tag(ParseTag(message_tag));

  for (;;) {
    const char* tagstr = va_arg(ap, const char*);
    if (tagstr == NULL) {
      break;
    }

    if (tagstr[0] == '$') {
      // Special value.
      const char* const special = tagstr + 1;
      if (strcmp(special, "padding") == 0) {
        const int min_bytes = va_arg(ap, int);
        msg.set_minimum_size(min_bytes);
      } else {
        CHECK(false) << "Unknown special value: " << special;
      }

      continue;
    }

    const QuicTag tag = ParseTag(tagstr);
    const char* valuestr = va_arg(ap, const char*);

    size_t len = strlen(valuestr);
    if (len > 0 && valuestr[0] == '#') {
      valuestr++;
      len--;

      CHECK_EQ(0u, len % 2);
      scoped_ptr<uint8[]> buf(new uint8[len/2]);

      for (size_t i = 0; i < len/2; i++) {
        uint8 v = 0;
        CHECK(HexChar(valuestr[i*2], &v));
        buf[i] = v << 4;
        CHECK(HexChar(valuestr[i*2 + 1], &v));
        buf[i] |= v;
      }

      msg.SetStringPiece(
          tag, StringPiece(reinterpret_cast<char*>(buf.get()), len/2));
      continue;
    }

    msg.SetStringPiece(tag, valuestr);
  }

  // The CryptoHandshakeMessage needs to be serialized and parsed to ensure
  // that any padding is included.
  scoped_ptr<QuicData> bytes(CryptoFramer::ConstructHandshakeMessage(msg));
  scoped_ptr<CryptoHandshakeMessage> parsed(
      CryptoFramer::ParseMessage(bytes->AsStringPiece()));
  CHECK(parsed.get());

  return *parsed;
}

}  // namespace test
}  // namespace net
