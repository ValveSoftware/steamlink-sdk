// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/local_strike_register_client.h"

#include "net/quic/crypto/crypto_protocol.h"

using base::StringPiece;
using std::string;

namespace net {

LocalStrikeRegisterClient::LocalStrikeRegisterClient(
    unsigned max_entries,
    uint32 current_time_external,
    uint32 window_secs,
    const uint8 orbit[8],
    StrikeRegister::StartupType startup)
    : strike_register_(max_entries, current_time_external,  window_secs,
                       orbit, startup) {
}

bool LocalStrikeRegisterClient::IsKnownOrbit(StringPiece orbit) const {
  base::AutoLock lock(m_);
  if (orbit.length() != kOrbitSize) {
    return false;
  }
  return memcmp(orbit.data(), strike_register_.orbit(), kOrbitSize) == 0;
}

void LocalStrikeRegisterClient::VerifyNonceIsValidAndUnique(
    StringPiece nonce, QuicWallTime now, ResultCallback* cb) {
  bool nonce_is_valid_and_unique;
  if (nonce.length() != kNonceSize) {
    nonce_is_valid_and_unique = false;
  } else {
    base::AutoLock lock(m_);
    nonce_is_valid_and_unique = strike_register_.Insert(
        reinterpret_cast<const uint8*>(nonce.data()),
        static_cast<uint32>(now.ToUNIXSeconds()));
  }

  // m_ must not be held when the ResultCallback runs.
  cb->Run(nonce_is_valid_and_unique);
}

}  // namespace net
