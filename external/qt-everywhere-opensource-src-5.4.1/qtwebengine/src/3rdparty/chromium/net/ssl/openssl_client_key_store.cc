// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/openssl_client_key_store.h"

#include <openssl/evp.h>
#include <openssl/x509.h>

#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "net/cert/x509_certificate.h"

namespace net {

namespace {

typedef OpenSSLClientKeyStore::ScopedEVP_PKEY ScopedEVP_PKEY;

// Increment the reference count of a given EVP_PKEY. This function
// is similar to EVP_PKEY_dup which is not available from the OpenSSL
// version used by Chromium at the moment. Its name is distinct to
// avoid compiler warnings about ambiguous function calls at caller
// sites.
EVP_PKEY* CopyEVP_PKEY(EVP_PKEY* key) {
  if (key)
    CRYPTO_add(&key->references, 1, CRYPTO_LOCK_EVP_PKEY);
  return key;
}

// Return the EVP_PKEY holding the public key of a given certificate.
// |cert| is a certificate.
// Returns a scoped EVP_PKEY for it.
ScopedEVP_PKEY GetOpenSSLPublicKey(const X509Certificate* cert) {
  // X509_PUBKEY_get() increments the reference count of its result.
  // Unlike X509_get_X509_PUBKEY() which simply returns a direct pointer.
  EVP_PKEY* pkey =
      X509_PUBKEY_get(X509_get_X509_PUBKEY(cert->os_cert_handle()));
  if (!pkey)
    LOG(ERROR) << "Can't extract private key from certificate!";
  return ScopedEVP_PKEY(pkey);
}

}  // namespace

OpenSSLClientKeyStore::OpenSSLClientKeyStore() {
}

OpenSSLClientKeyStore::~OpenSSLClientKeyStore() {
}

OpenSSLClientKeyStore::KeyPair::KeyPair(EVP_PKEY* pub_key,
                                        EVP_PKEY* priv_key) {
  public_key = CopyEVP_PKEY(pub_key);
  private_key = CopyEVP_PKEY(priv_key);
}

OpenSSLClientKeyStore::KeyPair::~KeyPair() {
  EVP_PKEY_free(public_key);
  EVP_PKEY_free(private_key);
}

OpenSSLClientKeyStore::KeyPair::KeyPair(const KeyPair& other) {
  public_key = CopyEVP_PKEY(other.public_key);
  private_key = CopyEVP_PKEY(other.private_key);
}

void OpenSSLClientKeyStore::KeyPair::operator=(const KeyPair& other) {
  EVP_PKEY* old_public_key = public_key;
  EVP_PKEY* old_private_key = private_key;
  public_key = CopyEVP_PKEY(other.public_key);
  private_key = CopyEVP_PKEY(other.private_key);
  EVP_PKEY_free(old_private_key);
  EVP_PKEY_free(old_public_key);
}

int OpenSSLClientKeyStore::FindKeyPairIndex(EVP_PKEY* public_key) {
  if (!public_key)
    return -1;
  for (size_t n = 0; n < pairs_.size(); ++n) {
    if (EVP_PKEY_cmp(pairs_[n].public_key, public_key) == 1)
      return static_cast<int>(n);
  }
  return -1;
}

void OpenSSLClientKeyStore::AddKeyPair(EVP_PKEY* pub_key,
                                       EVP_PKEY* private_key) {
  int index = FindKeyPairIndex(pub_key);
  if (index < 0)
    pairs_.push_back(KeyPair(pub_key, private_key));
}

// Common code for OpenSSLClientKeyStore. Shared by all OpenSSL-based
// builds.
bool OpenSSLClientKeyStore::RecordClientCertPrivateKey(
    const X509Certificate* client_cert,
    EVP_PKEY* private_key) {
  // Sanity check.
  if (!client_cert || !private_key)
    return false;

  // Get public key from certificate.
  ScopedEVP_PKEY pub_key(GetOpenSSLPublicKey(client_cert));
  if (!pub_key.get())
    return false;

  AddKeyPair(pub_key.get(), private_key);
  return true;
}

bool OpenSSLClientKeyStore::FetchClientCertPrivateKey(
    const X509Certificate* client_cert,
    ScopedEVP_PKEY* private_key) {
  if (!client_cert)
    return false;

  ScopedEVP_PKEY pub_key(GetOpenSSLPublicKey(client_cert));
  if (!pub_key.get())
    return false;

  int index = FindKeyPairIndex(pub_key.get());
  if (index < 0)
    return false;

  private_key->reset(CopyEVP_PKEY(pairs_[index].private_key));
  return true;
}

void OpenSSLClientKeyStore::Flush() {
  pairs_.clear();
}

OpenSSLClientKeyStore* OpenSSLClientKeyStore::GetInstance() {
  return Singleton<OpenSSLClientKeyStore>::get();
}

}  // namespace net


