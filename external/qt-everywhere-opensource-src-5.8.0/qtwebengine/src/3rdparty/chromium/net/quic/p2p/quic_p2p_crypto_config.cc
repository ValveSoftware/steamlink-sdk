// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/p2p/quic_p2p_crypto_config.h"

#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/crypto_utils.h"

namespace net {

QuicP2PCryptoConfig::QuicP2PCryptoConfig(const std::string& shared_key)
    : shared_key_(shared_key) {}

QuicP2PCryptoConfig::~QuicP2PCryptoConfig() {}

bool QuicP2PCryptoConfig::GetNegotiatedParameters(
    Perspective perspective,
    QuicCryptoNegotiatedParameters* out_params) {
  out_params->forward_secure_premaster_secret = shared_key_;
  out_params->aead = aead_;
  out_params->hkdf_input_suffix = hkdf_input_suffix_;

  std::string hkdf_input;
  const size_t label_len = strlen(QuicCryptoConfig::kForwardSecureLabel) + 1;
  hkdf_input.reserve(label_len + out_params->hkdf_input_suffix.size());
  hkdf_input.append(QuicCryptoConfig::kForwardSecureLabel, label_len);
  hkdf_input.append(out_params->hkdf_input_suffix);

  if (!CryptoUtils::DeriveKeys(
          out_params->forward_secure_premaster_secret, out_params->aead,
          out_params->client_nonce, out_params->server_nonce, hkdf_input,
          perspective, CryptoUtils::Diversification::Never(),
          &out_params->forward_secure_crypters, &out_params->subkey_secret)) {
    return false;
  }

  return true;
}

}  // namespace net
