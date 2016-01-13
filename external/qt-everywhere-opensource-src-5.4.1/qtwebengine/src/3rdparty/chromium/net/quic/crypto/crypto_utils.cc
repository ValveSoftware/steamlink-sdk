// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/crypto_utils.h"

#include "crypto/hkdf.h"
#include "net/base/net_util.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_time.h"
#include "url/url_canon.h"

using base::StringPiece;
using std::string;

namespace net {

// static
void CryptoUtils::GenerateNonce(QuicWallTime now,
                                QuicRandom* random_generator,
                                StringPiece orbit,
                                string* nonce) {
  // a 4-byte timestamp + 28 random bytes.
  nonce->reserve(kNonceSize);
  nonce->resize(kNonceSize);
  uint32 gmt_unix_time = now.ToUNIXSeconds();
  // The time in the nonce must be encoded in big-endian because the
  // strike-register depends on the nonces being ordered by time.
  (*nonce)[0] = static_cast<char>(gmt_unix_time >> 24);
  (*nonce)[1] = static_cast<char>(gmt_unix_time >> 16);
  (*nonce)[2] = static_cast<char>(gmt_unix_time >> 8);
  (*nonce)[3] = static_cast<char>(gmt_unix_time);

  size_t bytes_written = sizeof(gmt_unix_time);
  if (orbit.size() == 8) {
    memcpy(&(*nonce)[bytes_written], orbit.data(), orbit.size());
    bytes_written += orbit.size();
  }
  random_generator->RandBytes(&(*nonce)[bytes_written],
                              kNonceSize - bytes_written);
}

// static
bool CryptoUtils::IsValidSNI(StringPiece sni) {
  // TODO(rtenneti): Support RFC2396 hostname.
  // NOTE: Microsoft does NOT enforce this spec, so if we throw away hostnames
  // based on the above spec, we may be losing some hostnames that windows
  // would consider valid. By far the most common hostname character NOT
  // accepted by the above spec is '_'.
  url::CanonHostInfo host_info;
  string canonicalized_host(CanonicalizeHost(sni.as_string(), &host_info));
  return !host_info.IsIPAddress() &&
      IsCanonicalizedHostCompliant(canonicalized_host, std::string()) &&
      sni.find_last_of('.') != string::npos;
}

// static
string CryptoUtils::NormalizeHostname(const char* hostname) {
  url::CanonHostInfo host_info;
  string host(CanonicalizeHost(hostname, &host_info));

  // Walk backwards over the string, stopping at the first trailing dot.
  size_t host_end = host.length();
  while (host_end != 0 && host[host_end - 1] == '.') {
    host_end--;
  }

  // Erase the trailing dots.
  if (host_end != host.length()) {
    host.erase(host_end, host.length() - host_end);
  }
  return host;
}

// static
bool CryptoUtils::DeriveKeys(StringPiece premaster_secret,
                             QuicTag aead,
                             StringPiece client_nonce,
                             StringPiece server_nonce,
                             const string& hkdf_input,
                             Perspective perspective,
                             CrypterPair* out) {
  out->encrypter.reset(QuicEncrypter::Create(aead));
  out->decrypter.reset(QuicDecrypter::Create(aead));
  size_t key_bytes = out->encrypter->GetKeySize();
  size_t nonce_prefix_bytes = out->encrypter->GetNoncePrefixSize();

  StringPiece nonce = client_nonce;
  string nonce_storage;
  if (!server_nonce.empty()) {
    nonce_storage = client_nonce.as_string() + server_nonce.as_string();
    nonce = nonce_storage;
  }

  crypto::HKDF hkdf(premaster_secret, nonce, hkdf_input, key_bytes,
                    nonce_prefix_bytes);
  if (perspective == SERVER) {
    if (!out->encrypter->SetKey(hkdf.server_write_key()) ||
        !out->encrypter->SetNoncePrefix(hkdf.server_write_iv()) ||
        !out->decrypter->SetKey(hkdf.client_write_key()) ||
        !out->decrypter->SetNoncePrefix(hkdf.client_write_iv())) {
      return false;
    }
  } else {
    if (!out->encrypter->SetKey(hkdf.client_write_key()) ||
        !out->encrypter->SetNoncePrefix(hkdf.client_write_iv()) ||
        !out->decrypter->SetKey(hkdf.server_write_key()) ||
        !out->decrypter->SetNoncePrefix(hkdf.server_write_iv())) {
      return false;
    }
  }

  return true;
}

}  // namespace net
