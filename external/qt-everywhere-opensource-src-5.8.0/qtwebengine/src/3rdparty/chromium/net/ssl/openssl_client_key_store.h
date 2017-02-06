// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_OPENSSL_CLIENT_KEY_STORE_H_
#define NET_SSL_OPENSSL_CLIENT_KEY_STORE_H_

#include <openssl/evp.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_openssl_types.h"
#include "net/base/net_export.h"

namespace net {

class X509Certificate;

// OpenSSLClientKeyStore implements an in-memory store for client
// certificate private keys, because the platforms where OpenSSL is
// used do not provide a way to retrieve the private key of a known
// certificate.
//
// This class is not thread-safe and should only be used from the network
// thread.
class NET_EXPORT OpenSSLClientKeyStore {
 public:
  // Platforms must define this factory function as appropriate.
  static OpenSSLClientKeyStore* GetInstance();

  // Record the association between a certificate and its
  // private key. This method should be called _before_
  // FetchClientCertPrivateKey to ensure that the private key is returned
  // when it is called later. The association is recorded in memory
  // exclusively.
  // |cert| is a handle to a certificate object.
  // |private_key| is an OpenSSL EVP_PKEY that corresponds to the
  // certificate's private key.
  // Returns false if an error occured.
  // This function does not take ownership of the private_key, but may
  // increment its internal reference count.
  bool RecordClientCertPrivateKey(const X509Certificate* cert,
                                  EVP_PKEY* private_key);

  // Given a certificate's |public_key|, return the corresponding private
  // key that has been recorded previously by RecordClientCertPrivateKey().
  // |cert| is a client certificate.
  // Returns its matching private key on success, NULL otherwise.
  crypto::ScopedEVP_PKEY FetchClientCertPrivateKey(const X509Certificate* cert);

  // Flush all recorded keys.
  void Flush();

 protected:
  OpenSSLClientKeyStore();

  ~OpenSSLClientKeyStore();

  // Adds a given public/private key pair.
  // |pub_key| and |private_key| can point to the same object.
  // This increments the reference count on both objects, caller
  // must still call EVP_PKEY_free on them.
  void AddKeyPair(EVP_PKEY* pub_key, EVP_PKEY* private_key);

 private:
  // KeyPair is an internal class used to hold a pair of private / public
  // EVP_PKEY objects, with appropriate ownership.
  class KeyPair {
   public:
    explicit KeyPair(EVP_PKEY* pub_key, EVP_PKEY* priv_key);
    KeyPair(const KeyPair& other);
    // Intentionally pass by value, in order to use the copy-and-swap idiom.
    void operator=(KeyPair other);
    void swap(KeyPair& other);
    ~KeyPair();

    crypto::ScopedEVP_PKEY public_key;
    crypto::ScopedEVP_PKEY private_key;

   private:
    KeyPair();  // intentionally not implemented.
  };

  // Returns the index of the keypair for |public_key|. or -1 if not found.
  int FindKeyPairIndex(EVP_PKEY* public_key);

  std::vector<KeyPair> pairs_;

  friend struct base::DefaultSingletonTraits<OpenSSLClientKeyStore>;

  DISALLOW_COPY_AND_ASSIGN(OpenSSLClientKeyStore);
};

}  // namespace net

#endif  // NET_SSL_OPENSSL_CLIENT_KEY_STORE_H_
