// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <openssl/bn.h>
#include <openssl/dsa.h>
#include <openssl/ecdsa.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "crypto/auto_cbb.h"
#include "crypto/openssl_util.h"
#include "net/android/keystore.h"
#include "net/android/keystore_openssl.h"
#include "net/ssl/scoped_openssl_types.h"
#include "net/test/jni/AndroidKeyStoreTestUtil_jni.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

// Technical note:
//
// This source file not only checks that signing with
// RawSignDigestWithPrivateKey() works correctly, it also verifies that
// the generated signature matches 100% of what OpenSSL generates when
// calling RSA_sign(NID_md5_sha1,...), DSA_sign(0, ...) or
// ECDSA_sign(0, ...).
//
// That's crucial to ensure that this function can later be used to
// implement client certificate support. More specifically, that it is
// possible to create a custom EVP_PKEY that uses
// RawSignDigestWithPrivateKey() internally to perform RSA/DSA/ECDSA
// signing, as invoked by the OpenSSL code at
// openssl/ssl/s3_clnt.c:ssl3_send_client_verify().
//
// For more details, read the comments in AndroidKeyStore.java.
//
// Finally, it also checks that using the EVP_PKEY generated with
// GetOpenSSLPrivateKeyWrapper() works correctly.

namespace net {
namespace android {

namespace {

typedef base::android::ScopedJavaLocalRef<jobject> ScopedJava;

JNIEnv* InitEnv() {
  JNIEnv* env = base::android::AttachCurrentThread();
  static bool inited = false;
  if (!inited) {
    RegisterNativesImpl(env);
    inited = true;
  }
  return env;
}

// Returns true if running on an Android version older than 4.2
bool IsOnAndroidOlderThan_4_2(void) {
  const int kAndroid42ApiLevel = 17;
  int level = base::android::BuildInfo::GetInstance()->sdk_int();
  return level < kAndroid42ApiLevel;
}

// Implements the callback expected by ERR_print_errors_cb().
// used by GetOpenSSLErrorString below.
int openssl_print_error_callback(const char* msg, size_t msglen, void* u) {
  std::string* result = reinterpret_cast<std::string*>(u);
  result->append(msg, msglen);
  return 1;
}

// Retrieves the OpenSSL error as a string
std::string GetOpenSSLErrorString(void) {
  std::string result;
  ERR_print_errors_cb(openssl_print_error_callback, &result);
  return result;
}

// Resize a string to |size| bytes of data, then return its data buffer
// address cast as an 'unsigned char*', as expected by OpenSSL functions.
// |str| the target string.
// |size| the number of bytes to write into the string.
// Return the string's new buffer in memory, as an 'unsigned char*'
// pointer.
unsigned char* OpenSSLWriteInto(std::string* str, size_t size) {
  return reinterpret_cast<unsigned char*>(base::WriteInto(str, size + 1));
}

// Load a given private key file into an EVP_PKEY.
// |filename| is the key file path.
// Returns a new EVP_PKEY on success, NULL on failure.
EVP_PKEY* ImportPrivateKeyFile(const char* filename) {
  // Load file in memory.
  base::FilePath certs_dir = GetTestCertsDirectory();
  base::FilePath file_path = certs_dir.AppendASCII(filename);
  base::ScopedFILE handle(base::OpenFile(file_path, "rb"));
  if (!handle.get()) {
    LOG(ERROR) << "Could not open private key file: " << filename;
    return NULL;
  }
  // Assume it is PEM_encoded. Load it as an EVP_PKEY.
  EVP_PKEY* pkey = PEM_read_PrivateKey(handle.get(), NULL, NULL, NULL);
  if (!pkey) {
    LOG(ERROR) << "Could not load public key file: " << filename
               << ", " << GetOpenSSLErrorString();
    return NULL;
  }
  return pkey;
}

// Convert a private key into its PKCS#8 encoded representation.
// |pkey| is the EVP_PKEY handle for the private key.
// |pkcs8| will receive the PKCS#8 bytes.
// Returns true on success, false otherwise.
bool GetPrivateKeyPkcs8Bytes(const crypto::ScopedEVP_PKEY& pkey,
                             std::string* pkcs8) {
  uint8_t* der;
  size_t der_len;
  crypto::AutoCBB cbb;
  if (!CBB_init(cbb.get(), 0) ||
      !EVP_marshal_private_key(cbb.get(), pkey.get()) ||
      !CBB_finish(cbb.get(), &der, &der_len)) {
    return false;
  }
  pkcs8->assign(reinterpret_cast<const char*>(der), der_len);
  OPENSSL_free(der);
  return true;
}

bool ImportPrivateKeyFileAsPkcs8(const char* filename,
                                 std::string* pkcs8) {
  crypto::ScopedEVP_PKEY pkey(ImportPrivateKeyFile(filename));
  if (!pkey.get())
    return false;
  return GetPrivateKeyPkcs8Bytes(pkey, pkcs8);
}

// Same as ImportPrivateKey, but for public ones.
EVP_PKEY* ImportPublicKeyFile(const char* filename) {
  // Load file as PEM data.
  base::FilePath certs_dir = GetTestCertsDirectory();
  base::FilePath file_path = certs_dir.AppendASCII(filename);
  base::ScopedFILE handle(base::OpenFile(file_path, "rb"));
  if (!handle.get()) {
    LOG(ERROR) << "Could not open public key file: " << filename;
    return NULL;
  }
  EVP_PKEY* pkey = PEM_read_PUBKEY(handle.get(), NULL, NULL, NULL);
  if (!pkey) {
    LOG(ERROR) << "Could not load public key file: " << filename
               << ", " << GetOpenSSLErrorString();
    return NULL;
  }
  return pkey;
}

// Retrieve a JNI local ref from encoded PKCS#8 data.
ScopedJava GetPKCS8PrivateKeyJava(PrivateKeyType key_type,
                                  const std::string& pkcs8_key) {
  JNIEnv* env = InitEnv();
  base::android::ScopedJavaLocalRef<jbyteArray> bytes(
      base::android::ToJavaByteArray(
          env, reinterpret_cast<const uint8_t*>(pkcs8_key.data()),
          pkcs8_key.size()));

  ScopedJava key(
      Java_AndroidKeyStoreTestUtil_createPrivateKeyFromPKCS8(
          env, key_type, bytes.obj()));

  return key;
}

const char kTestRsaKeyFile[] = "android-test-key-rsa.pem";

// The RSA test hash must be 36 bytes exactly.
const char kTestRsaHash[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

// Retrieve a JNI local ref for our test RSA key.
ScopedJava GetRSATestKeyJava() {
  std::string key;
  if (!ImportPrivateKeyFileAsPkcs8(kTestRsaKeyFile, &key))
    return ScopedJava();
  return GetPKCS8PrivateKeyJava(PRIVATE_KEY_TYPE_RSA, key);
}

const char kTestEcdsaKeyFile[] = "android-test-key-ecdsa.pem";
const char kTestEcdsaPublicKeyFile[] = "android-test-key-ecdsa-public.pem";

// The test hash for ECDSA keys must be 20 bytes exactly.
const char kTestEcdsaHash[] = "0123456789ABCDEFGHIJ";

// Retrieve a JNI local ref for our test ECDSA key.
ScopedJava GetECDSATestKeyJava() {
  std::string key;
  if (!ImportPrivateKeyFileAsPkcs8(kTestEcdsaKeyFile, &key))
    return ScopedJava();
  return GetPKCS8PrivateKeyJava(PRIVATE_KEY_TYPE_ECDSA, key);
}

// Call this function to verify that one message signed with our
// test ECDSA private key is correct. Since ECDSA signing introduces
// random elements in the signature, it is not possible to compare
// signature bits directly. However, one can use the public key
// to do the check.
bool VerifyTestECDSASignature(const base::StringPiece& message,
                              const base::StringPiece& signature) {
  crypto::ScopedEVP_PKEY pkey(ImportPublicKeyFile(kTestEcdsaPublicKeyFile));
  if (!pkey.get())
    return false;
  crypto::ScopedEC_KEY pub_key(EVP_PKEY_get1_EC_KEY(pkey.get()));
  if (!pub_key.get()) {
    LOG(ERROR) << "Could not get ECDSA public key: "
               << GetOpenSSLErrorString();
    return false;
  }

  const unsigned char* digest =
      reinterpret_cast<const unsigned char*>(message.data());
  int digest_len = static_cast<int>(message.size());
  const unsigned char* sigbuf =
      reinterpret_cast<const unsigned char*>(signature.data());
  int siglen = static_cast<int>(signature.size());

  int ret = ECDSA_verify(
      0, digest, digest_len, sigbuf, siglen, pub_key.get());
  if (ret != 1) {
    LOG(ERROR) << "ECDSA_verify() failed: " << GetOpenSSLErrorString();
    return false;
  }
  return true;
}

// Sign a message with OpenSSL, return the result as a string.
// |message| is the message to be signed.
// |openssl_key| is an OpenSSL EVP_PKEY to use.
// |result| receives the result.
// Returns true on success, false otherwise.
bool SignWithOpenSSL(const base::StringPiece& message,
                     EVP_PKEY* openssl_key,
                     std::string* result) {
  const unsigned char* digest =
      reinterpret_cast<const unsigned char*>(message.data());
  unsigned int digest_len = static_cast<unsigned int>(message.size());
  std::string signature;
  size_t signature_size;
  size_t max_signature_size;
  int key_type = EVP_PKEY_id(openssl_key);
  switch (key_type) {
    case EVP_PKEY_RSA:
    {
      crypto::ScopedRSA rsa(EVP_PKEY_get1_RSA(openssl_key));
      if (!rsa.get()) {
        LOG(ERROR) << "Could not get RSA from EVP_PKEY: "
                   << GetOpenSSLErrorString();
        return false;
      }
      // With RSA, the signature will always be RSA_size() bytes.
      max_signature_size = static_cast<size_t>(RSA_size(rsa.get()));
      unsigned char* p = OpenSSLWriteInto(&signature,
                                          max_signature_size);
      unsigned int p_len = 0;
      int ret = RSA_sign(
          NID_md5_sha1, digest, digest_len, p, &p_len, rsa.get());
      if (ret != 1) {
        LOG(ERROR) << "RSA_sign() failed: " << GetOpenSSLErrorString();
        return false;
      }
      signature_size = static_cast<size_t>(p_len);
      break;
    }
    case EVP_PKEY_EC:
    {
      crypto::ScopedEC_KEY ecdsa(EVP_PKEY_get1_EC_KEY(openssl_key));
      if (!ecdsa.get()) {
        LOG(ERROR) << "Could not get EC_KEY from EVP_PKEY: "
                   << GetOpenSSLErrorString();
        return false;
      }
      // Note, the actual signature can be smaller than ECDSA_size()
      max_signature_size = ECDSA_size(ecdsa.get());
      unsigned char* p = OpenSSLWriteInto(&signature,
                                          max_signature_size);
      unsigned int p_len = 0;
      // Note: first parameter is ignored by function.
      int ret = ECDSA_sign(
          0, digest, digest_len, p, &p_len, ecdsa.get());
      if (ret != 1) {
        LOG(ERROR) << "ECDSA_sign() fialed: " << GetOpenSSLErrorString();
        return false;
      }
      signature_size = static_cast<size_t>(p_len);
      break;
    }
    default:
      LOG(WARNING) << "Invalid OpenSSL key type: " << key_type;
      return false;
  }

  if (signature_size == 0) {
    LOG(ERROR) << "Signature is empty!";
    return false;
  }
  if (signature_size > max_signature_size) {
    LOG(ERROR) << "Signature size mismatch, actual " << signature_size
                << ", expected <= " << max_signature_size;
    return false;
  }
  signature.resize(signature_size);
  result->swap(signature);
  return true;
}

// Check that a generated signature for a given message matches
// OpenSSL output byte-by-byte.
// |message| is the input message.
// |signature| is the generated signature for the message.
// |openssl_key| is a raw EVP_PKEY for the same private key than the
// one which was used to generate the signature.
// Returns true on success, false otherwise.
bool CompareSignatureWithOpenSSL(const base::StringPiece& message,
                                 const base::StringPiece& signature,
                                 EVP_PKEY* openssl_key) {
  std::string openssl_signature;
  SignWithOpenSSL(message, openssl_key, &openssl_signature);

  if (signature.size() != openssl_signature.size()) {
    LOG(ERROR) << "Signature size mismatch, actual "
               << signature.size() << ", expected "
               << openssl_signature.size();
    return false;
  }
  for (size_t n = 0; n < signature.size(); ++n) {
    if (openssl_signature[n] != signature[n]) {
      LOG(ERROR) << "Signature byte mismatch at index " << n
                 << "actual " << signature[n] << ", expected "
                 << openssl_signature[n];
      LOG(ERROR) << "Actual signature  : "
                 << base::HexEncode(signature.data(), signature.size());
      LOG(ERROR) << "Expected signature: "
                 << base::HexEncode(openssl_signature.data(),
                                    openssl_signature.size());
      return false;
    }
  }
  return true;
}

// Sign a message with our platform API.
//
// |android_key| is a JNI reference to the platform PrivateKey object.
// |openssl_key| is a pointer to an OpenSSL key object for the exact
// same key content.
// |message| is a message.
// |result| will receive the result.
void DoKeySigning(jobject android_key,
                  EVP_PKEY* openssl_key,
                  const base::StringPiece& message,
                  std::string* result) {
  // First, get the platform signature.
  std::vector<uint8_t> android_signature;
  ASSERT_TRUE(
      RawSignDigestWithPrivateKey(android_key,
                                  message,
                                  &android_signature));

  result->assign(
      reinterpret_cast<const char*>(&android_signature[0]),
      android_signature.size());
}

// Sign a message with our OpenSSL EVP_PKEY wrapper around platform
// APIS.
//
// |android_key| is a JNI reference to the platform PrivateKey object.
// |openssl_key| is a pointer to an OpenSSL key object for the exact
// same key content.
// |message| is a message.
// |result| will receive the result.
void DoKeySigningWithWrapper(EVP_PKEY* wrapper_key,
                             EVP_PKEY* openssl_key,
                             const base::StringPiece& message,
                             std::string* result) {
  // First, get the platform signature.
  std::string wrapper_signature;
  SignWithOpenSSL(message, wrapper_key, &wrapper_signature);
  ASSERT_NE(0U, wrapper_signature.size());

  result->assign(
      reinterpret_cast<const char*>(&wrapper_signature[0]),
      wrapper_signature.size());
}

}  // namespace

TEST(AndroidKeyStore, GetRSAKeyModulus) {
  crypto::OpenSSLErrStackTracer err_trace(FROM_HERE);
  InitEnv();

  // Load the test RSA key.
  crypto::ScopedEVP_PKEY pkey(ImportPrivateKeyFile(kTestRsaKeyFile));
  ASSERT_TRUE(pkey.get());

  // Convert it to encoded PKCS#8 bytes.
  std::string pkcs8_data;
  ASSERT_TRUE(GetPrivateKeyPkcs8Bytes(pkey, &pkcs8_data));

  // Create platform PrivateKey object from it.
  ScopedJava key_java = GetPKCS8PrivateKeyJava(PRIVATE_KEY_TYPE_RSA,
                                                pkcs8_data);
  ASSERT_FALSE(key_java.is_null());

  // Retrieve the corresponding modulus through JNI
  std::vector<uint8_t> modulus_java;
  ASSERT_TRUE(GetRSAKeyModulus(key_java.obj(), &modulus_java));

  // Create an OpenSSL BIGNUM from it.
  crypto::ScopedBIGNUM bn(
      BN_bin2bn(reinterpret_cast<const unsigned char*>(&modulus_java[0]),
                static_cast<int>(modulus_java.size()),
                NULL));
  ASSERT_TRUE(bn.get());

  // Compare it to the one in the RSA key, they must be identical.
  crypto::ScopedRSA rsa(EVP_PKEY_get1_RSA(pkey.get()));
  ASSERT_TRUE(rsa.get()) << GetOpenSSLErrorString();

  ASSERT_EQ(0, BN_cmp(bn.get(), rsa.get()->n));
}

TEST(AndroidKeyStore,GetPrivateKeyTypeRSA) {
  crypto::OpenSSLErrStackTracer err_trace(FROM_HERE);

  ScopedJava rsa_key = GetRSATestKeyJava();
  ASSERT_FALSE(rsa_key.is_null());
  EXPECT_EQ(PRIVATE_KEY_TYPE_RSA,
            GetPrivateKeyType(rsa_key.obj()));
}

TEST(AndroidKeyStore,SignWithPrivateKeyRSA) {
  ScopedJava rsa_key = GetRSATestKeyJava();
  ASSERT_FALSE(rsa_key.is_null());

  if (IsOnAndroidOlderThan_4_2()) {
    LOG(INFO) << "This test can't run on Android < 4.2";
    return;
  }

  crypto::ScopedEVP_PKEY openssl_key(ImportPrivateKeyFile(kTestRsaKeyFile));
  ASSERT_TRUE(openssl_key.get());

  std::string message = kTestRsaHash;
  ASSERT_EQ(36U, message.size());

  std::string signature;
  DoKeySigning(rsa_key.obj(), openssl_key.get(), message, &signature);
  ASSERT_TRUE(
      CompareSignatureWithOpenSSL(message, signature, openssl_key.get()));
  // All good.
}

TEST(AndroidKeyStore,SignWithWrapperKeyRSA) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  ScopedJava rsa_key = GetRSATestKeyJava();
  ASSERT_FALSE(rsa_key.is_null());

  crypto::ScopedEVP_PKEY wrapper_key(
      GetOpenSSLPrivateKeyWrapper(rsa_key.obj()));
  ASSERT_TRUE(wrapper_key.get() != NULL);

  crypto::ScopedEVP_PKEY openssl_key(ImportPrivateKeyFile(kTestRsaKeyFile));
  ASSERT_TRUE(openssl_key.get());

  // Check that RSA_size() works properly on the wrapper key.
  EXPECT_EQ(EVP_PKEY_size(openssl_key.get()),
            EVP_PKEY_size(wrapper_key.get()));

  // Message size must be 36 for RSA_sign(NID_md5_sha1,...) to return
  // without an error.
  std::string message = kTestRsaHash;
  ASSERT_EQ(36U, message.size());

  std::string signature;
  DoKeySigningWithWrapper(wrapper_key.get(),
                          openssl_key.get(),
                          message,
                          &signature);
  ASSERT_TRUE(
      CompareSignatureWithOpenSSL(message, signature, openssl_key.get()));
}

TEST(AndroidKeyStore,GetPrivateKeyTypeECDSA) {
  crypto::OpenSSLErrStackTracer err_trace(FROM_HERE);

  ScopedJava ecdsa_key = GetECDSATestKeyJava();
  ASSERT_FALSE(ecdsa_key.is_null());
  EXPECT_EQ(PRIVATE_KEY_TYPE_ECDSA,
            GetPrivateKeyType(ecdsa_key.obj()));
}

TEST(AndroidKeyStore,SignWithPrivateKeyECDSA) {
  ScopedJava ecdsa_key = GetECDSATestKeyJava();
  ASSERT_FALSE(ecdsa_key.is_null());

  crypto::ScopedEVP_PKEY openssl_key(ImportPrivateKeyFile(kTestEcdsaKeyFile));
  ASSERT_TRUE(openssl_key.get());

  std::string message = kTestEcdsaHash;
  std::string signature;
  DoKeySigning(ecdsa_key.obj(), openssl_key.get(), message, &signature);
  ASSERT_TRUE(VerifyTestECDSASignature(message, signature));
}

TEST(AndroidKeyStore, SignWithWrapperKeyECDSA) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  ScopedJava ecdsa_key = GetECDSATestKeyJava();
  ASSERT_FALSE(ecdsa_key.is_null());

  crypto::ScopedEVP_PKEY wrapper_key(
      GetOpenSSLPrivateKeyWrapper(ecdsa_key.obj()));
  ASSERT_TRUE(wrapper_key.get());

  crypto::ScopedEVP_PKEY openssl_key(ImportPrivateKeyFile(kTestEcdsaKeyFile));
  ASSERT_TRUE(openssl_key.get());

  // Check that ECDSA size works correctly on the wrapper.
  EXPECT_EQ(EVP_PKEY_size(openssl_key.get()),
            EVP_PKEY_size(wrapper_key.get()));

  std::string message = kTestEcdsaHash;
  std::string signature;
  DoKeySigningWithWrapper(wrapper_key.get(),
                          openssl_key.get(),
                          message,
                          &signature);
  ASSERT_TRUE(VerifyTestECDSASignature(message, signature));
}

}  // namespace android
}  // namespace net
