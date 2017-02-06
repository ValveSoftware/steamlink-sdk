// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/openssl_client_key_store.h"

#include <openssl/evp.h>
#include <openssl/x509.h>

#include <algorithm>
#include <memory>

#include "base/memory/singleton.h"
#include "net/cert/x509_certificate.h"

namespace net {

namespace {

// Return the EVP_PKEY holding the public key of a given certificate.
// |cert| is a certificate.
// Returns a scoped EVP_PKEY for it.
crypto::ScopedEVP_PKEY GetOpenSSLPublicKey(const X509Certificate* cert) {
  // X509_PUBKEY_get() increments the reference count of its result.
  // Unlike X509_get_X509_PUBKEY() which simply returns a direct pointer.
  EVP_PKEY* pkey =
      X509_PUBKEY_get(X509_get_X509_PUBKEY(cert->os_cert_handle()));
  if (!pkey)
    LOG(ERROR) << "Can't extract private key from certificate!";
  return crypto::ScopedEVP_PKEY(pkey);
}

}  // namespace

OpenSSLClientKeyStore::OpenSSLClientKeyStore() {
}

OpenSSLClientKeyStore::~OpenSSLClientKeyStore() {
}

OpenSSLClientKeyStore::KeyPair::KeyPair(EVP_PKEY* pub_key, EVP_PKEY* priv_key)
    : public_key(EVP_PKEY_up_ref(pub_key)),
      private_key(EVP_PKEY_up_ref(priv_key)) {
}

OpenSSLClientKeyStore::KeyPair::~KeyPair() {
}

OpenSSLClientKeyStore::KeyPair::KeyPair(const KeyPair& other)
    : public_key(EVP_PKEY_up_ref(other.public_key.get())),
      private_key(EVP_PKEY_up_ref(other.private_key.get())) {
}

void OpenSSLClientKeyStore::KeyPair::operator=(KeyPair other) {
  swap(other);
}

void OpenSSLClientKeyStore::KeyPair::swap(KeyPair& other) {
  using std::swap;
  swap(public_key, other.public_key);
  swap(private_key, other.private_key);
}

int OpenSSLClientKeyStore::FindKeyPairIndex(EVP_PKEY* public_key) {
  if (!public_key)
    return -1;
  for (size_t n = 0; n < pairs_.size(); ++n) {
    if (EVP_PKEY_cmp(pairs_[n].public_key.get(), public_key) == 1)
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
  crypto::ScopedEVP_PKEY pub_key(GetOpenSSLPublicKey(client_cert));
  if (!pub_key.get())
    return false;

  AddKeyPair(pub_key.get(), private_key);
  return true;
}

crypto::ScopedEVP_PKEY OpenSSLClientKeyStore::FetchClientCertPrivateKey(
    const X509Certificate* client_cert) {
  if (!client_cert)
    return crypto::ScopedEVP_PKEY();

  crypto::ScopedEVP_PKEY pub_key(GetOpenSSLPublicKey(client_cert));
  if (!pub_key.get())
    return crypto::ScopedEVP_PKEY();

  int index = FindKeyPairIndex(pub_key.get());
  if (index < 0)
    return crypto::ScopedEVP_PKEY();

  return crypto::ScopedEVP_PKEY(
      EVP_PKEY_up_ref(pairs_[index].private_key.get()));
}

void OpenSSLClientKeyStore::Flush() {
  pairs_.clear();
}

OpenSSLClientKeyStore* OpenSSLClientKeyStore::GetInstance() {
  return base::Singleton<OpenSSLClientKeyStore>::get();
}

}  // namespace net


