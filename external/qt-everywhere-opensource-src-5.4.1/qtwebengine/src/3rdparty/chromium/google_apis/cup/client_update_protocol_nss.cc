// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/cup/client_update_protocol.h"

#include <keyhi.h>
#include <pk11pub.h>
#include <seccomon.h>

#include "base/logging.h"
#include "crypto/nss_util.h"
#include "crypto/scoped_nss_types.h"

typedef scoped_ptr<CERTSubjectPublicKeyInfo,
                   crypto::NSSDestroyer<CERTSubjectPublicKeyInfo,
                                        SECKEY_DestroySubjectPublicKeyInfo> >
    ScopedCERTSubjectPublicKeyInfo;

ClientUpdateProtocol::~ClientUpdateProtocol() {
  if (public_key_)
    SECKEY_DestroyPublicKey(public_key_);
}

bool ClientUpdateProtocol::LoadPublicKey(const base::StringPiece& public_key) {
  crypto::EnsureNSSInit();

  // The binary blob |public_key| is expected to be a DER-encoded ASN.1
  // Subject Public Key Info.
  SECItem spki_item;
  spki_item.type = siBuffer;
  spki_item.data =
      reinterpret_cast<unsigned char*>(const_cast<char*>(public_key.data()));
  spki_item.len = static_cast<unsigned int>(public_key.size());

  ScopedCERTSubjectPublicKeyInfo spki(
      SECKEY_DecodeDERSubjectPublicKeyInfo(&spki_item));
  if (!spki.get())
    return false;

  public_key_ = SECKEY_ExtractPublicKey(spki.get());
  if (!public_key_)
    return false;

  if (!PublicKeyLength())
    return false;

  return true;
}

size_t ClientUpdateProtocol::PublicKeyLength() {
  if (!public_key_)
    return 0;

  return SECKEY_PublicKeyStrength(public_key_);
}

bool ClientUpdateProtocol::EncryptKeySource(
    const std::vector<uint8>& key_source) {
  // WARNING: This call bypasses the usual PKCS #1 padding and does direct RSA
  // exponentiation.  This is not secure without taking measures to ensure that
  // the contents of r are suitable.  This is done to remain compatible with
  // the implementation on the Google Update servers; don't copy-paste this
  // code arbitrarily and expect it to work and/or remain secure!
  if (!public_key_)
    return false;

  size_t keysize = SECKEY_PublicKeyStrength(public_key_);
  if (key_source.size() != keysize)
    return false;

  encrypted_key_source_.resize(keysize);
  return SECSuccess == PK11_PubEncryptRaw(
      public_key_,
      &encrypted_key_source_[0],
      const_cast<unsigned char*>(&key_source[0]),
      key_source.size(),
      NULL);
}

