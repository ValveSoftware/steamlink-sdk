// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/keygen_handler.h"

#include <openssl/ssl.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "crypto/openssl_util.h"
#include "crypto/rsa_private_key.h"
#include "net/base/openssl_private_key_store.h"

namespace net {

std::string KeygenHandler::GenKeyAndSignChallenge() {
  scoped_ptr<crypto::RSAPrivateKey> key(
      crypto::RSAPrivateKey::Create(key_size_in_bits_));
  EVP_PKEY* pkey = key->key();

  if (stores_key_)
    OpenSSLPrivateKeyStore::StoreKeyPair(url_, pkey);

  crypto::ScopedOpenSSL<NETSCAPE_SPKI, NETSCAPE_SPKI_free> spki(
       NETSCAPE_SPKI_new());
  ASN1_STRING_set(spki.get()->spkac->challenge,
                  challenge_.data(), challenge_.size());
  NETSCAPE_SPKI_set_pubkey(spki.get(), pkey);
  // Using MD5 as this is what is required in HTML5, even though the SPKI
  // structure does allow the use of a SHA-1 signature.
  NETSCAPE_SPKI_sign(spki.get(), pkey, EVP_md5());
  char* spkistr = NETSCAPE_SPKI_b64_encode(spki.get());

  std::string result(spkistr);
  OPENSSL_free(spkistr);

  return result;
}

}  // namespace net

