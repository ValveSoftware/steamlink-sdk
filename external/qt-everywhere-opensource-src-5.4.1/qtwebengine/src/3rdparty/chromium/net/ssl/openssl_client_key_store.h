// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_OPENSSL_CLIENT_KEY_STORE_H_
#define NET_SSL_OPENSSL_CLIENT_KEY_STORE_H_

#include <openssl/evp.h>

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "crypto/openssl_util.h"
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

  struct EVP_PKEY_Deleter {
    inline void operator()(EVP_PKEY* ptr) const {
      EVP_PKEY_free(ptr);
    }
  };

  typedef scoped_ptr<EVP_PKEY, EVP_PKEY_Deleter> ScopedEVP_PKEY;

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
  NET_EXPORT bool RecordClientCertPrivateKey(const X509Certificate* cert,
                                             EVP_PKEY* private_key);

  // Given a certificate's |public_key|, return the corresponding private
  // key that has been recorded previously by RecordClientCertPrivateKey().
  // |cert| is a client certificate.
  // |*private_key| will be reset to its matching private key on success.
  // Returns true on success, false otherwise. This increments the reference
  // count of the private key on success.
  bool FetchClientCertPrivateKey(const X509Certificate* cert,
                                 ScopedEVP_PKEY* private_key);

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
    void operator=(const KeyPair& other);
    ~KeyPair();

    EVP_PKEY* public_key;
    EVP_PKEY* private_key;

   private:
    KeyPair();  // intentionally not implemented.
  };

  // Returns the index of the keypair for |public_key|. or -1 if not found.
  int FindKeyPairIndex(EVP_PKEY* public_key);

  std::vector<KeyPair> pairs_;

  friend struct DefaultSingletonTraits<OpenSSLClientKeyStore>;

  DISALLOW_COPY_AND_ASSIGN(OpenSSLClientKeyStore);
};

}  // namespace net

#endif  // NET_SSL_OPENSSL_CLIENT_KEY_STORE_H_
