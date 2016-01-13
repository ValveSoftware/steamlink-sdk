// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_CRYPTO_TEST_UTILS_H_
#define NET_QUIC_TEST_TOOLS_CRYPTO_TEST_UTILS_H_

#include <stdarg.h>

#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "net/quic/crypto/crypto_framer.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_protocol.h"

namespace net {

class ChannelIDSource;
class CommonCertSets;
class ProofSource;
class ProofVerifier;
class ProofVerifyContext;
class QuicClock;
class QuicConfig;
class QuicCryptoClientStream;
class QuicCryptoServerConfig;
class QuicCryptoServerStream;
class QuicCryptoStream;
class QuicRandom;

namespace test {

class PacketSavingConnection;

class CryptoTestUtils {
 public:
  // FakeClientOptions bundles together a number of options for configuring
  // HandshakeWithFakeClient.
  struct FakeClientOptions {
    FakeClientOptions();

    // If dont_verify_certs is true then no ProofVerifier is set on the client.
    // Thus no certificates will be requested or checked.
    bool dont_verify_certs;

    // If channel_id_enabled is true then the client will attempt to send a
    // ChannelID.
    bool channel_id_enabled;
  };

  // returns: the number of client hellos that the client sent.
  static int HandshakeWithFakeServer(PacketSavingConnection* client_conn,
                                     QuicCryptoClientStream* client);

  // returns: the number of client hellos that the client sent.
  static int HandshakeWithFakeClient(PacketSavingConnection* server_conn,
                                     QuicCryptoServerStream* server,
                                     const FakeClientOptions& options);

  // SetupCryptoServerConfigForTest configures |config| and |crypto_config|
  // with sensible defaults for testing.
  static void SetupCryptoServerConfigForTest(
      const QuicClock* clock,
      QuicRandom* rand,
      QuicConfig* config,
      QuicCryptoServerConfig* crypto_config);

  // CommunicateHandshakeMessages moves messages from |a| to |b| and back until
  // |a|'s handshake has completed.
  static void CommunicateHandshakeMessages(PacketSavingConnection* a_conn,
                                           QuicCryptoStream* a,
                                           PacketSavingConnection* b_conn,
                                           QuicCryptoStream* b);

  // AdvanceHandshake attempts to moves messages from |a| to |b| and |b| to |a|.
  // Returns the number of messages moved.
  static std::pair<size_t, size_t> AdvanceHandshake(
      PacketSavingConnection* a_conn,
      QuicCryptoStream* a,
      size_t a_i,
      PacketSavingConnection* b_conn,
      QuicCryptoStream* b,
      size_t b_i);

  // Returns the value for the tag |tag| in the tag value map of |message|.
  static std::string GetValueForTag(const CryptoHandshakeMessage& message,
                                    QuicTag tag);

  // Returns a |ProofSource| that serves up test certificates.
  static ProofSource* ProofSourceForTesting();

  // Returns a |ProofVerifier| that uses the QUIC testing root CA.
  static ProofVerifier* ProofVerifierForTesting();

  // Returns a |ProofVerifyContext| that must be used with the verifier
  // returned by |ProofVerifierForTesting|.
  static ProofVerifyContext* ProofVerifyContextForTesting();

  // MockCommonCertSets returns a CommonCertSets that contains a single set with
  // hash |hash|, consisting of the certificate |cert| at index |index|.
  static CommonCertSets* MockCommonCertSets(base::StringPiece cert,
                                            uint64 hash,
                                            uint32 index);

  // ParseTag returns a QuicTag from parsing |tagstr|. |tagstr| may either be
  // in the format "EXMP" (i.e. ASCII format), or "#11223344" (an explicit hex
  // format). It CHECK fails if there's a parse error.
  static QuicTag ParseTag(const char* tagstr);

  // Message constructs a handshake message from a variable number of
  // arguments. |message_tag| is passed to |ParseTag| and used as the tag of
  // the resulting message. The arguments are taken in pairs and NULL
  // terminated. The first of each pair is the tag of a tag/value and is given
  // as an argument to |ParseTag|. The second is the value of the tag/value
  // pair and is either a hex dump, preceeded by a '#', or a raw value.
  //
  //   Message(
  //       "CHLO",
  //       "NOCE", "#11223344",
  //       "SNI", "www.example.com",
  //       NULL);
  static CryptoHandshakeMessage Message(const char* message_tag, ...);

  // BuildMessage is the same as |Message|, but takes the variable arguments
  // explicitly. TODO(rtenneti): Investigate whether it'd be better for
  // Message() and BuildMessage() to return a CryptoHandshakeMessage* pointer
  // instead, to avoid copying the return value.
  static CryptoHandshakeMessage BuildMessage(const char* message_tag,
                                             va_list ap);

  // ChannelIDSourceForTesting returns a ChannelIDSource that generates keys
  // deterministically based on the hostname given in the GetChannelIDKey call.
  // This ChannelIDSource works in synchronous mode, i.e., its GetChannelIDKey
  // method never returns QUIC_PENDING.
  static ChannelIDSource* ChannelIDSourceForTesting();

 private:
  static void CompareClientAndServerKeys(QuicCryptoClientStream* client,
                                         QuicCryptoServerStream* server);

  DISALLOW_COPY_AND_ASSIGN(CryptoTestUtils);
};

}  // namespace test

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_CRYPTO_TEST_UTILS_H_
