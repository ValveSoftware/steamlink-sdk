// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_crypto_server_stream.h"

#include "base/base64.h"
#include "crypto/secure_hash.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/crypto/quic_crypto_server_config.h"
#include "net/quic/quic_config.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_session.h"

namespace net {

QuicCryptoServerStream::QuicCryptoServerStream(
    const QuicCryptoServerConfig& crypto_config,
    QuicSession* session)
    : QuicCryptoStream(session),
      crypto_config_(crypto_config),
      validate_client_hello_cb_(NULL),
      num_handshake_messages_(0) {
}

QuicCryptoServerStream::~QuicCryptoServerStream() {
  CancelOutstandingCallbacks();
}

void QuicCryptoServerStream::CancelOutstandingCallbacks() {
  // Detach from the validation callback.  Calling this multiple times is safe.
  if (validate_client_hello_cb_ != NULL) {
    validate_client_hello_cb_->Cancel();
  }
}

void QuicCryptoServerStream::OnHandshakeMessage(
    const CryptoHandshakeMessage& message) {
  QuicCryptoStream::OnHandshakeMessage(message);
  ++num_handshake_messages_;

  // Do not process handshake messages after the handshake is confirmed.
  if (handshake_confirmed_) {
    CloseConnection(QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE);
    return;
  }

  if (message.tag() != kCHLO) {
    CloseConnection(QUIC_INVALID_CRYPTO_MESSAGE_TYPE);
    return;
  }

  if (validate_client_hello_cb_ != NULL) {
    // Already processing some other handshake message.  The protocol
    // does not allow for clients to send multiple handshake messages
    // before the server has a chance to respond.
    CloseConnection(QUIC_CRYPTO_MESSAGE_WHILE_VALIDATING_CLIENT_HELLO);
    return;
  }

  validate_client_hello_cb_ = new ValidateCallback(this);
  return crypto_config_.ValidateClientHello(
      message,
      session()->connection()->peer_address(),
      session()->connection()->clock(),
      validate_client_hello_cb_);
}

void QuicCryptoServerStream::FinishProcessingHandshakeMessage(
    const CryptoHandshakeMessage& message,
    const ValidateClientHelloResultCallback::Result& result) {
  // Clear the callback that got us here.
  DCHECK(validate_client_hello_cb_ != NULL);
  validate_client_hello_cb_ = NULL;

  string error_details;
  CryptoHandshakeMessage reply;
  QuicErrorCode error = ProcessClientHello(
      message, result, &reply, &error_details);

  if (error != QUIC_NO_ERROR) {
    CloseConnectionWithDetails(error, error_details);
    return;
  }

  if (reply.tag() != kSHLO) {
    SendHandshakeMessage(reply);
    return;
  }

  // If we are returning a SHLO then we accepted the handshake.
  QuicConfig* config = session()->config();
  OverrideQuicConfigDefaults(config);
  error = config->ProcessPeerHello(message, CLIENT, &error_details);
  if (error != QUIC_NO_ERROR) {
    CloseConnectionWithDetails(error, error_details);
    return;
  }
  session()->OnConfigNegotiated();

  config->ToHandshakeMessage(&reply);

  // Receiving a full CHLO implies the client is prepared to decrypt with
  // the new server write key.  We can start to encrypt with the new server
  // write key.
  //
  // NOTE: the SHLO will be encrypted with the new server write key.
  session()->connection()->SetEncrypter(
      ENCRYPTION_INITIAL,
      crypto_negotiated_params_.initial_crypters.encrypter.release());
  session()->connection()->SetDefaultEncryptionLevel(
      ENCRYPTION_INITIAL);
  // Set the decrypter immediately so that we no longer accept unencrypted
  // packets.
  session()->connection()->SetDecrypter(
      crypto_negotiated_params_.initial_crypters.decrypter.release(),
      ENCRYPTION_INITIAL);
  SendHandshakeMessage(reply);

  session()->connection()->SetEncrypter(
      ENCRYPTION_FORWARD_SECURE,
      crypto_negotiated_params_.forward_secure_crypters.encrypter.release());
  session()->connection()->SetDefaultEncryptionLevel(
      ENCRYPTION_FORWARD_SECURE);
  session()->connection()->SetAlternativeDecrypter(
      crypto_negotiated_params_.forward_secure_crypters.decrypter.release(),
      ENCRYPTION_FORWARD_SECURE, false /* don't latch */);

  encryption_established_ = true;
  handshake_confirmed_ = true;
  session()->OnCryptoHandshakeEvent(QuicSession::HANDSHAKE_CONFIRMED);
}

bool QuicCryptoServerStream::GetBase64SHA256ClientChannelID(
    string* output) const {
  if (!encryption_established_ ||
      crypto_negotiated_params_.channel_id.empty()) {
    return false;
  }

  const string& channel_id(crypto_negotiated_params_.channel_id);
  scoped_ptr<crypto::SecureHash> hash(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));
  hash->Update(channel_id.data(), channel_id.size());
  uint8 digest[32];
  hash->Finish(digest, sizeof(digest));

  base::Base64Encode(string(
      reinterpret_cast<const char*>(digest), sizeof(digest)), output);
  // Remove padding.
  size_t len = output->size();
  if (len >= 2) {
    if ((*output)[len - 1] == '=') {
      len--;
      if ((*output)[len - 1] == '=') {
        len--;
      }
      output->resize(len);
    }
  }
  return true;
}

QuicErrorCode QuicCryptoServerStream::ProcessClientHello(
    const CryptoHandshakeMessage& message,
    const ValidateClientHelloResultCallback::Result& result,
    CryptoHandshakeMessage* reply,
    string* error_details) {
  return crypto_config_.ProcessClientHello(
      result,
      session()->connection()->connection_id(),
      session()->connection()->peer_address(),
      session()->connection()->version(),
      session()->connection()->supported_versions(),
      session()->connection()->clock(),
      session()->connection()->random_generator(),
      &crypto_negotiated_params_, reply, error_details);
}

void QuicCryptoServerStream::OverrideQuicConfigDefaults(QuicConfig* config) {
}

QuicCryptoServerStream::ValidateCallback::ValidateCallback(
    QuicCryptoServerStream* parent) : parent_(parent) {
}

void QuicCryptoServerStream::ValidateCallback::Cancel() {
  parent_ = NULL;
}

void QuicCryptoServerStream::ValidateCallback::RunImpl(
    const CryptoHandshakeMessage& client_hello,
    const Result& result) {
  if (parent_ != NULL) {
    parent_->FinishProcessingHandshakeMessage(client_hello, result);
  }
}

}  // namespace net
