// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/cup/client_update_protocol.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/sha1.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "crypto/hmac.h"
#include "crypto/random.h"

namespace {

base::StringPiece ByteVectorToSP(const std::vector<uint8>& vec) {
  if (vec.empty())
    return base::StringPiece();

  return base::StringPiece(reinterpret_cast<const char*>(&vec[0]), vec.size());
}

// This class needs to implement the same hashing and signing functions as the
// Google Update server; for now, this is SHA-1 and HMAC-SHA1, but this may
// change to SHA-256 in the near future.  For this reason, all primitives are
// wrapped.  The name "SymSign" is used to mirror the CUP specification.
size_t HashDigestSize() {
  return base::kSHA1Length;
}

std::vector<uint8> Hash(const std::vector<uint8>& data) {
  std::vector<uint8> result(HashDigestSize());
  base::SHA1HashBytes(data.empty() ? NULL : &data[0],
                      data.size(),
                      &result[0]);
  return result;
}

std::vector<uint8> Hash(const base::StringPiece& sdata) {
  std::vector<uint8> result(HashDigestSize());
  base::SHA1HashBytes(sdata.empty() ?
                          NULL :
                          reinterpret_cast<const unsigned char*>(sdata.data()),
                      sdata.length(),
                      &result[0]);
  return result;
}

std::vector<uint8> SymConcat(uint8 id,
                             const std::vector<uint8>* h1,
                             const std::vector<uint8>* h2,
                             const std::vector<uint8>* h3) {
  std::vector<uint8> result;
  result.push_back(id);
  const std::vector<uint8>* args[] = { h1, h2, h3 };
  for (size_t i = 0; i != arraysize(args); ++i) {
    if (args[i]) {
      DCHECK_EQ(args[i]->size(), HashDigestSize());
      result.insert(result.end(), args[i]->begin(), args[i]->end());
    }
  }

  return result;
}

std::vector<uint8> SymSign(const std::vector<uint8>& key,
                           const std::vector<uint8>& hashes) {
  DCHECK(!key.empty());
  DCHECK(!hashes.empty());

  crypto::HMAC hmac(crypto::HMAC::SHA1);
  if (!hmac.Init(&key[0], key.size()))
    return std::vector<uint8>();

  std::vector<uint8> result(hmac.DigestLength());
  if (!hmac.Sign(ByteVectorToSP(hashes), &result[0], result.size()))
    return std::vector<uint8>();

  return result;
}

bool SymSignVerify(const std::vector<uint8>& key,
                   const std::vector<uint8>& hashes,
                   const std::vector<uint8>& server_proof) {
  DCHECK(!key.empty());
  DCHECK(!hashes.empty());
  DCHECK(!server_proof.empty());

  crypto::HMAC hmac(crypto::HMAC::SHA1);
  if (!hmac.Init(&key[0], key.size()))
    return false;

  return hmac.Verify(ByteVectorToSP(hashes), ByteVectorToSP(server_proof));
}

// RsaPad() is implemented as described in the CUP spec.  It is NOT a general
// purpose padding algorithm.
std::vector<uint8> RsaPad(size_t rsa_key_size,
                          const std::vector<uint8>& entropy) {
  DCHECK_GE(rsa_key_size, HashDigestSize());

  // The result gets padded with zeros if the result size is greater than
  // the size of the buffer provided by the caller.
  std::vector<uint8> result(entropy);
  result.resize(rsa_key_size - HashDigestSize());

  // For use with RSA, the input needs to be smaller than the RSA modulus,
  // which has always the msb set.
  result[0] &= 127;  // Reset msb
  result[0] |= 64;   // Set second highest bit.

  std::vector<uint8> digest = Hash(result);
  result.insert(result.end(), digest.begin(), digest.end());
  DCHECK_EQ(result.size(), rsa_key_size);
  return result;
}

// CUP passes the versioned secret in the query portion of the URL for the
// update check service -- and that means that a URL-safe variant of Base64 is
// needed.  Call the standard Base64 encoder/decoder and then apply fixups.
std::string UrlSafeB64Encode(const std::vector<uint8>& data) {
  std::string result;
  base::Base64Encode(ByteVectorToSP(data), &result);

  // Do an tr|+/|-_| on the output, and strip any '=' padding.
  for (std::string::iterator it = result.begin(); it != result.end(); ++it) {
    switch (*it) {
      case '+':
        *it = '-';
        break;
      case '/':
        *it = '_';
        break;
      default:
        break;
    }
  }
  base::TrimString(result, "=", &result);

  return result;
}

std::vector<uint8> UrlSafeB64Decode(const base::StringPiece& input) {
  std::string unsafe(input.begin(), input.end());
  for (std::string::iterator it = unsafe.begin(); it != unsafe.end(); ++it) {
    switch (*it) {
      case '-':
        *it = '+';
        break;
      case '_':
        *it = '/';
        break;
      default:
        break;
    }
  }
  if (unsafe.length() % 4)
    unsafe.append(4 - (unsafe.length() % 4), '=');

  std::string decoded;
  if (!base::Base64Decode(unsafe, &decoded))
    return std::vector<uint8>();

  return std::vector<uint8>(decoded.begin(), decoded.end());
}

}  // end namespace

ClientUpdateProtocol::ClientUpdateProtocol(int key_version)
    : pub_key_version_(key_version) {
}

scoped_ptr<ClientUpdateProtocol> ClientUpdateProtocol::Create(
    int key_version,
    const base::StringPiece& public_key) {
  DCHECK_GT(key_version, 0);
  DCHECK(!public_key.empty());

  scoped_ptr<ClientUpdateProtocol> result(
      new ClientUpdateProtocol(key_version));
  if (!result)
    return scoped_ptr<ClientUpdateProtocol>();

  if (!result->LoadPublicKey(public_key))
    return scoped_ptr<ClientUpdateProtocol>();

  if (!result->BuildRandomSharedKey())
    return scoped_ptr<ClientUpdateProtocol>();

  return result.Pass();
}

std::string ClientUpdateProtocol::GetVersionedSecret() const {
  return base::StringPrintf("%d:%s",
                            pub_key_version_,
                            UrlSafeB64Encode(encrypted_key_source_).c_str());
}

bool ClientUpdateProtocol::SignRequest(const base::StringPiece& url,
                                       const base::StringPiece& request_body,
                                       std::string* client_proof) {
  DCHECK(!encrypted_key_source_.empty());
  DCHECK(!url.empty());
  DCHECK(!request_body.empty());
  DCHECK(client_proof);

  // Compute the challenge hash:
  //   hw = HASH(HASH(v|w)|HASH(request_url)|HASH(body)).
  // Keep the challenge hash for later to validate the server's response.
  std::vector<uint8> internal_hashes;

  std::vector<uint8> h;
  h = Hash(GetVersionedSecret());
  internal_hashes.insert(internal_hashes.end(), h.begin(), h.end());
  h = Hash(url);
  internal_hashes.insert(internal_hashes.end(), h.begin(), h.end());
  h = Hash(request_body);
  internal_hashes.insert(internal_hashes.end(), h.begin(), h.end());
  DCHECK_EQ(internal_hashes.size(), 3 * HashDigestSize());

  client_challenge_hash_ = Hash(internal_hashes);

  // Sign the challenge hash (hw) using the shared key (sk) to produce the
  // client proof (cp).
  std::vector<uint8> raw_client_proof =
      SymSign(shared_key_, SymConcat(3, &client_challenge_hash_, NULL, NULL));
  if (raw_client_proof.empty()) {
    client_challenge_hash_.clear();
    return false;
  }

  *client_proof = UrlSafeB64Encode(raw_client_proof);
  return true;
}

bool ClientUpdateProtocol::ValidateResponse(
    const base::StringPiece& response_body,
    const base::StringPiece& server_cookie,
    const base::StringPiece& server_proof) {
  DCHECK(!client_challenge_hash_.empty());

  if (response_body.empty() || server_cookie.empty() || server_proof.empty())
    return false;

  // Decode the server proof from URL-safe Base64 to a binary HMAC for the
  // response.
  std::vector<uint8> sp_decoded = UrlSafeB64Decode(server_proof);
  if (sp_decoded.empty())
    return false;

  // If the request was received by the server, the server will use its
  // private key to decrypt |w_|, yielding the original contents of |r_|.
  // The server can then recreate |sk_|, compute |hw_|, and SymSign(3|hw)
  // to ensure that the cp matches the contents.  It will then use |sk_|
  // to sign its response, producing the server proof |sp|.
  std::vector<uint8> hm = Hash(response_body);
  std::vector<uint8> hc = Hash(server_cookie);
  return SymSignVerify(shared_key_,
                       SymConcat(1, &client_challenge_hash_, &hm, &hc),
                       sp_decoded);
}

bool ClientUpdateProtocol::BuildRandomSharedKey() {
  DCHECK_GE(PublicKeyLength(), HashDigestSize());

  // Start by generating some random bytes that are suitable to be encrypted;
  // this will be the source of the shared HMAC key that client and server use.
  // (CUP specification calls this "r".)
  std::vector<uint8> key_source;
  std::vector<uint8> entropy(PublicKeyLength() - HashDigestSize());
  crypto::RandBytes(&entropy[0], entropy.size());
  key_source = RsaPad(PublicKeyLength(), entropy);

  return DeriveSharedKey(key_source);
}

bool ClientUpdateProtocol::SetSharedKeyForTesting(
  const base::StringPiece& key_source) {
  DCHECK_EQ(key_source.length(), PublicKeyLength());

  return DeriveSharedKey(std::vector<uint8>(key_source.begin(),
                                            key_source.end()));
}

bool ClientUpdateProtocol::DeriveSharedKey(const std::vector<uint8>& source) {
  DCHECK(!source.empty());
  DCHECK_GE(source.size(), HashDigestSize());
  DCHECK_EQ(source.size(), PublicKeyLength());

  // Hash the key source (r) to generate a new shared HMAC key (sk').
  shared_key_ = Hash(source);

  // Encrypt the key source (r) using the public key (pk[v]) to generate the
  // encrypted key source (w).
  if (!EncryptKeySource(source))
    return false;
  if (encrypted_key_source_.size() != PublicKeyLength())
    return false;

  return true;
}
