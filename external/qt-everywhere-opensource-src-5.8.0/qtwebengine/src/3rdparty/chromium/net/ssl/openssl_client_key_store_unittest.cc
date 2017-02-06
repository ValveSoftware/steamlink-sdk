// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/openssl_client_key_store.h"

#include "base/memory/ref_counted.h"
#include "crypto/scoped_openssl_types.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Return the internal reference count of a given EVP_PKEY.
int EVP_PKEY_get_refcount(EVP_PKEY* pkey) {
  return pkey->references;
}

// A common test class to ensure that the store is flushed after
// each test.
class OpenSSLClientKeyStoreTest : public ::testing::Test {
 public:
  OpenSSLClientKeyStoreTest()
    : store_(OpenSSLClientKeyStore::GetInstance()) {
  }

  ~OpenSSLClientKeyStoreTest() override {
    if (store_)
      store_->Flush();
  }

 protected:
  OpenSSLClientKeyStore* store_;
};

// Check that GetInstance() returns non-null
TEST_F(OpenSSLClientKeyStoreTest, GetInstance) {
  ASSERT_TRUE(store_);
}

// Check that Flush() works correctly.
TEST_F(OpenSSLClientKeyStoreTest, Flush) {
  ASSERT_TRUE(store_);

  scoped_refptr<X509Certificate> cert_1(
      ImportCertFromFile(GetTestCertsDirectory(), "client_1.pem"));
  ASSERT_TRUE(cert_1.get());

  crypto::ScopedEVP_PKEY priv_key(EVP_PKEY_new());
  ASSERT_TRUE(priv_key.get());

  ASSERT_TRUE(store_->RecordClientCertPrivateKey(cert_1.get(),
                                                 priv_key.get()));

  store_->Flush();

  // Retrieve the private key. This should fail because the store
  // was flushed.
  crypto::ScopedEVP_PKEY pkey = store_->FetchClientCertPrivateKey(cert_1.get());
  ASSERT_FALSE(pkey.get());
}

// Check that trying to retrieve the private key of an unknown certificate
// simply fails by returning null.
TEST_F(OpenSSLClientKeyStoreTest, FetchEmptyPrivateKey) {
  ASSERT_TRUE(store_);

  scoped_refptr<X509Certificate> cert_1(
      ImportCertFromFile(GetTestCertsDirectory(), "client_1.pem"));
  ASSERT_TRUE(cert_1.get());

  // Retrieve the private key now. This should fail because it was
  // never recorded in the store.
  crypto::ScopedEVP_PKEY pkey = store_->FetchClientCertPrivateKey(cert_1.get());
  ASSERT_FALSE(pkey.get());
}

// Check that any private key recorded through RecordClientCertPrivateKey
// can be retrieved with FetchClientCertPrivateKey.
TEST_F(OpenSSLClientKeyStoreTest, RecordAndFetchPrivateKey) {
  ASSERT_TRUE(store_);

  // Any certificate / key pair will do, the store is not supposed to
  // check that the private and certificate public keys match. This is
  // by design since the private EVP_PKEY could be a wrapper around a
  // JNI reference, with no way to access the real private key bits.
  scoped_refptr<X509Certificate> cert_1(
      ImportCertFromFile(GetTestCertsDirectory(), "client_1.pem"));
  ASSERT_TRUE(cert_1.get());

  crypto::ScopedEVP_PKEY priv_key(EVP_PKEY_new());
  ASSERT_TRUE(priv_key.get());
  ASSERT_EQ(1, EVP_PKEY_get_refcount(priv_key.get()));

  // Add the key a first time, this should increment its reference count.
  ASSERT_TRUE(store_->RecordClientCertPrivateKey(cert_1.get(),
                                                 priv_key.get()));
  ASSERT_EQ(2, EVP_PKEY_get_refcount(priv_key.get()));

  // Two successive calls with the same certificate / private key shall
  // also succeed, but the key's reference count should not be incremented.
  ASSERT_TRUE(store_->RecordClientCertPrivateKey(cert_1.get(),
                                                 priv_key.get()));
  ASSERT_EQ(2, EVP_PKEY_get_refcount(priv_key.get()));

  // Retrieve the private key. This should increment the private key's
  // reference count.
  crypto::ScopedEVP_PKEY pkey2 =
      store_->FetchClientCertPrivateKey(cert_1.get());
  ASSERT_EQ(pkey2.get(), priv_key.get());
  ASSERT_EQ(3, EVP_PKEY_get_refcount(priv_key.get()));

  // Flush the store explicitely, this should decrement the private
  // key's reference count.
  store_->Flush();
  ASSERT_EQ(2, EVP_PKEY_get_refcount(priv_key.get()));
}

// Same test, but with two certificates / private keys.
TEST_F(OpenSSLClientKeyStoreTest, RecordAndFetchTwoPrivateKeys) {
  scoped_refptr<X509Certificate> cert_1(
      ImportCertFromFile(GetTestCertsDirectory(), "client_1.pem"));
  ASSERT_TRUE(cert_1.get());

  scoped_refptr<X509Certificate> cert_2(
      ImportCertFromFile(GetTestCertsDirectory(), "client_2.pem"));
  ASSERT_TRUE(cert_2.get());

  crypto::ScopedEVP_PKEY priv_key1(EVP_PKEY_new());
  ASSERT_TRUE(priv_key1.get());
  ASSERT_EQ(1, EVP_PKEY_get_refcount(priv_key1.get()));

  crypto::ScopedEVP_PKEY priv_key2(EVP_PKEY_new());
  ASSERT_TRUE(priv_key2.get());
  ASSERT_EQ(1, EVP_PKEY_get_refcount(priv_key2.get()));

  ASSERT_NE(priv_key1.get(), priv_key2.get());

  // Add the key a first time, this shall succeed, and increment the
  // reference count.
  EXPECT_TRUE(store_->RecordClientCertPrivateKey(cert_1.get(),
                                                 priv_key1.get()));
  EXPECT_TRUE(store_->RecordClientCertPrivateKey(cert_2.get(),
                                                 priv_key2.get()));
  EXPECT_EQ(2, EVP_PKEY_get_refcount(priv_key1.get()));
  EXPECT_EQ(2, EVP_PKEY_get_refcount(priv_key2.get()));

  // Retrieve the private key now. This shall succeed and increment
  // the private key's reference count.
  crypto::ScopedEVP_PKEY fetch_key1 =
      store_->FetchClientCertPrivateKey(cert_1.get());
  crypto::ScopedEVP_PKEY fetch_key2 =
      store_->FetchClientCertPrivateKey(cert_2.get());

  EXPECT_TRUE(fetch_key1.get());
  EXPECT_TRUE(fetch_key2.get());

  EXPECT_EQ(fetch_key1.get(), priv_key1.get());
  EXPECT_EQ(fetch_key2.get(), priv_key2.get());

  EXPECT_EQ(3, EVP_PKEY_get_refcount(priv_key1.get()));
  EXPECT_EQ(3, EVP_PKEY_get_refcount(priv_key2.get()));
}

}  // namespace
}  // namespace net
