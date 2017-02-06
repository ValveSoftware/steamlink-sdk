// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/p2p/quic_p2p_crypto_stream.h"

#include "crypto/hkdf.h"
#include "net/quic/crypto/crypto_handshake_message.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_session.h"

namespace net {

QuicP2PCryptoStream::QuicP2PCryptoStream(QuicSession* session,
                                         const QuicP2PCryptoConfig& config)
    : QuicCryptoStream(session), config_(config) {}

QuicP2PCryptoStream::~QuicP2PCryptoStream() {}

bool QuicP2PCryptoStream::Connect() {
  if (!config_.GetNegotiatedParameters(session()->connection()->perspective(),
                                       &crypto_negotiated_params_)) {
    return false;
  }

  CrypterPair* crypters = &crypto_negotiated_params_.forward_secure_crypters;

  session()->connection()->SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                                        crypters->encrypter.release());
  session()->connection()->SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  session()->connection()->SetAlternativeDecrypter(
      ENCRYPTION_FORWARD_SECURE, crypters->decrypter.release(),
      false /* don't latch */);
  encryption_established_ = true;
  session()->OnCryptoHandshakeEvent(QuicSession::ENCRYPTION_FIRST_ESTABLISHED);
  session()->OnConfigNegotiated();
  session()->connection()->OnHandshakeComplete();
  handshake_confirmed_ = true;
  session()->OnCryptoHandshakeEvent(QuicSession::HANDSHAKE_CONFIRMED);
  if (session()->connection()->perspective() == Perspective::IS_CLIENT) {
    // Send a ping from the client to the server to satisfy QUIC's version
    // negotiation.
    session()->connection()->SendPing();
  }
  return true;
}

}  // namespace net
