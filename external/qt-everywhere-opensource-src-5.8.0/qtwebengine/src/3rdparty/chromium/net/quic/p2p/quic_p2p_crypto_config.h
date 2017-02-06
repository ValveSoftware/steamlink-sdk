// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_P2P_QUIC_P2P_CRYPTO_CONFIG_H_
#define NET_QUIC_P2P_QUIC_P2P_CRYPTO_CONFIG_H_

#include <string>

#include "net/quic/crypto/crypto_protocol.h"

namespace net {

struct QuicCryptoNegotiatedParameters;

// Crypto configuration for P2P sessions.
class NET_EXPORT QuicP2PCryptoConfig {
 public:
  // |shared_key| specifies a key that's used to generate crypto keys. The key
  // must be exchanged out-of-bound when the P2P transport is negotiated.
  explicit QuicP2PCryptoConfig(const std::string& shared_key);
  ~QuicP2PCryptoConfig();

  void set_aead(QuicTag aead) { aead_ = aead; }

  // Sets suffix for HKDF. E.g. can be channel name to make sure different
  // channels use different encryption keys derived from the same |shared_key|.
  // Empty by default.
  void set_hkdf_input_suffix(const std::string& hkdf_input_suffix) {
    hkdf_input_suffix_ = hkdf_input_suffix;
  }

  bool GetNegotiatedParameters(Perspective perspective,
                               QuicCryptoNegotiatedParameters* out_params);

 private:
  QuicTag aead_ = kAESG;
  std::string shared_key_;
  std::string hkdf_input_suffix_;
};

}  // namespace net

#endif  // NET_QUIC_P2P_QUIC_P2P_CRYPTO_CONFIG_H_
