// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/keystore_openssl.h"

#include <jni.h>
#include <openssl/bn.h>
// This include is required to get the ECDSA_METHOD structure definition
// which isn't currently part of the OpenSSL official ABI. This should
// not be a concern for Chromium which always links against its own
// version of the library on Android.
#include <openssl/crypto/ecdsa/ecs_locl.h>
// And this one is needed for the EC_GROUP definition.
#include <openssl/crypto/ec/ec_lcl.h>
#include <openssl/dsa.h>
#include <openssl/ec.h>
#include <openssl/engine.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "crypto/openssl_util.h"
#include "net/android/keystore.h"
#include "net/ssl/ssl_client_cert_type.h"

// IMPORTANT NOTE: The following code will currently only work when used
// to implement client certificate support with OpenSSL. That's because
// only the signing operations used in this use case are implemented here.
//
// Generally speaking, OpenSSL provides many different ways to sign
// digests. This code doesn't support all these cases, only the ones that
// are required to sign the digest during the OpenSSL handshake for TLS.
//
// The OpenSSL EVP_PKEY type is a generic wrapper around key pairs.
// Internally, it can hold a pointer to a RSA, DSA or ECDSA structure,
// which model keypair implementations of each respective crypto
// algorithm.
//
// The RSA type has a 'method' field pointer to a vtable-like structure
// called a RSA_METHOD. This contains several function pointers that
// correspond to operations on RSA keys (e.g. decode/encode with public
// key, decode/encode with private key, signing, validation), as well as
// a few flags.
//
// For example, the RSA_sign() function will call "method->rsa_sign()" if
// method->rsa_sign is not NULL, otherwise, it will perform a regular
// signing operation using the other fields in the RSA structure (which
// are used to hold the typical modulus / exponent / parameters for the
// key pair).
//
// This source file thus defines a custom RSA_METHOD structure whose
// fields point to static methods used to implement the corresponding
// RSA operation using platform Android APIs.
//
// However, the platform APIs require a jobject JNI reference to work.
// It must be stored in the RSA instance, or made accessible when the
// custom RSA methods are called. This is done by using RSA_set_app_data()
// and RSA_get_app_data().
//
// One can thus _directly_ create a new EVP_PKEY that uses a custom RSA
// object with the following:
//
//    RSA* rsa = RSA_new()
//    RSA_set_method(&custom_rsa_method);
//    RSA_set_app_data(rsa, jni_private_key);
//
//    EVP_PKEY* pkey = EVP_PKEY_new();
//    EVP_PKEY_assign_RSA(pkey, rsa);
//
// Note that because EVP_PKEY_assign_RSA() is used, instead of
// EVP_PKEY_set1_RSA(), the new EVP_PKEY now owns the RSA object, and
// will destroy it when it is itself destroyed.
//
// Unfortunately, such objects cannot be used with RSA_size(), which
// totally ignores the RSA_METHOD pointers. Instead, it is necessary
// to manually setup the modulus field (n) in the RSA object, with a
// value that matches the wrapped PrivateKey object. See GetRsaPkeyWrapper
// for full details.
//
// Similarly, custom DSA_METHOD and ECDSA_METHOD are defined by this source
// file, and appropriate field setups are performed to ensure that
// DSA_size() and ECDSA_size() work properly with the wrapper EVP_PKEY.
//
// Note that there is no need to define an OpenSSL ENGINE here. These
// are objects that can be used to expose custom methods (i.e. either
// RSA_METHOD, DSA_METHOD, ECDSA_METHOD, and a large number of other ones
// for types not related to this source file), and make them used by
// default for a lot of operations. Very fortunately, this is not needed
// here, which saves a lot of complexity.

using base::android::ScopedJavaGlobalRef;

namespace net {
namespace android {

namespace {

typedef crypto::ScopedOpenSSL<EVP_PKEY, EVP_PKEY_free> ScopedEVP_PKEY;
typedef crypto::ScopedOpenSSL<RSA, RSA_free> ScopedRSA;
typedef crypto::ScopedOpenSSL<DSA, DSA_free> ScopedDSA;
typedef crypto::ScopedOpenSSL<EC_KEY, EC_KEY_free> ScopedEC_KEY;
typedef crypto::ScopedOpenSSL<EC_GROUP, EC_GROUP_free> ScopedEC_GROUP;

// Custom RSA_METHOD that uses the platform APIs.
// Note that for now, only signing through RSA_sign() is really supported.
// all other method pointers are either stubs returning errors, or no-ops.
// See <openssl/rsa.h> for exact declaration of RSA_METHOD.

int RsaMethodPubEnc(int flen,
                    const unsigned char* from,
                    unsigned char* to,
                    RSA* rsa,
                    int padding) {
  NOTIMPLEMENTED();
  RSAerr(RSA_F_RSA_PUBLIC_ENCRYPT, RSA_R_RSA_OPERATIONS_NOT_SUPPORTED);
  return -1;
}

int RsaMethodPubDec(int flen,
                    const unsigned char* from,
                    unsigned char* to,
                    RSA* rsa,
                    int padding) {
  NOTIMPLEMENTED();
  RSAerr(RSA_F_RSA_PUBLIC_DECRYPT, RSA_R_RSA_OPERATIONS_NOT_SUPPORTED);
  return -1;
}

// See RSA_eay_private_encrypt in
// third_party/openssl/openssl/crypto/rsa/rsa_eay.c for the default
// implementation of this function.
int RsaMethodPrivEnc(int flen,
                     const unsigned char *from,
                     unsigned char *to,
                     RSA *rsa,
                     int padding) {
  DCHECK_EQ(RSA_PKCS1_PADDING, padding);
  if (padding != RSA_PKCS1_PADDING) {
    // TODO(davidben): If we need to, we can implement RSA_NO_PADDING
    // by using javax.crypto.Cipher and picking either the
    // "RSA/ECB/NoPadding" or "RSA/ECB/PKCS1Padding" transformation as
    // appropriate. I believe support for both of these was added in
    // the same Android version as the "NONEwithRSA"
    // java.security.Signature algorithm, so the same version checks
    // for GetRsaLegacyKey should work.
    RSAerr(RSA_F_RSA_PRIVATE_ENCRYPT, RSA_R_UNKNOWN_PADDING_TYPE);
    return -1;
  }

  // Retrieve private key JNI reference.
  jobject private_key = reinterpret_cast<jobject>(RSA_get_app_data(rsa));
  if (!private_key) {
    LOG(WARNING) << "Null JNI reference passed to RsaMethodPrivEnc!";
    RSAerr(RSA_F_RSA_PRIVATE_ENCRYPT, ERR_R_INTERNAL_ERROR);
    return -1;
  }

  base::StringPiece from_piece(reinterpret_cast<const char*>(from), flen);
  std::vector<uint8> result;
  // For RSA keys, this function behaves as RSA_private_encrypt with
  // PKCS#1 padding.
  if (!RawSignDigestWithPrivateKey(private_key, from_piece, &result)) {
    LOG(WARNING) << "Could not sign message in RsaMethodPrivEnc!";
    RSAerr(RSA_F_RSA_PRIVATE_ENCRYPT, ERR_R_INTERNAL_ERROR);
    return -1;
  }

  size_t expected_size = static_cast<size_t>(RSA_size(rsa));
  if (result.size() > expected_size) {
    LOG(ERROR) << "RSA Signature size mismatch, actual: "
               <<  result.size() << ", expected <= " << expected_size;
    RSAerr(RSA_F_RSA_PRIVATE_ENCRYPT, ERR_R_INTERNAL_ERROR);
    return -1;
  }

  // Copy result to OpenSSL-provided buffer. RawSignDigestWithPrivateKey
  // should pad with leading 0s, but if it doesn't, pad the result.
  size_t zero_pad = expected_size - result.size();
  memset(to, 0, zero_pad);
  memcpy(to + zero_pad, &result[0], result.size());

  return expected_size;
}

int RsaMethodPrivDec(int flen,
                     const unsigned char* from,
                     unsigned char* to,
                     RSA* rsa,
                     int padding) {
  NOTIMPLEMENTED();
  RSAerr(RSA_F_RSA_PRIVATE_DECRYPT, RSA_R_RSA_OPERATIONS_NOT_SUPPORTED);
  return -1;
}

int RsaMethodInit(RSA* rsa) {
  return 0;
}

int RsaMethodFinish(RSA* rsa) {
  // Ensure the global JNI reference created with this wrapper is
  // properly destroyed with it.
  jobject key = reinterpret_cast<jobject>(RSA_get_app_data(rsa));
  if (key != NULL) {
    RSA_set_app_data(rsa, NULL);
    ReleaseKey(key);
  }
  // Actual return value is ignored by OpenSSL. There are no docs
  // explaining what this is supposed to be.
  return 0;
}

const RSA_METHOD android_rsa_method = {
  /* .name = */ "Android signing-only RSA method",
  /* .rsa_pub_enc = */ RsaMethodPubEnc,
  /* .rsa_pub_dec = */ RsaMethodPubDec,
  /* .rsa_priv_enc = */ RsaMethodPrivEnc,
  /* .rsa_priv_dec = */ RsaMethodPrivDec,
  /* .rsa_mod_exp = */ NULL,
  /* .bn_mod_exp = */ NULL,
  /* .init = */ RsaMethodInit,
  /* .finish = */ RsaMethodFinish,
  // This flag is necessary to tell OpenSSL to avoid checking the content
  // (i.e. internal fields) of the private key. Otherwise, it will complain
  // it's not valid for the certificate.
  /* .flags = */ RSA_METHOD_FLAG_NO_CHECK,
  /* .app_data = */ NULL,
  /* .rsa_sign = */ NULL,
  /* .rsa_verify = */ NULL,
  /* .rsa_keygen = */ NULL,
};

// Copy the contents of an encoded big integer into an existing BIGNUM.
// This function modifies |*num| in-place.
// |new_bytes| is the byte encoding of the new value.
// |num| points to the BIGNUM which will be assigned with the new value.
// Returns true on success, false otherwise. On failure, |*num| is
// not modified.
bool CopyBigNumFromBytes(const std::vector<uint8>& new_bytes,
                         BIGNUM* num) {
  BIGNUM* ret = BN_bin2bn(
      reinterpret_cast<const unsigned char*>(&new_bytes[0]),
      static_cast<int>(new_bytes.size()),
      num);
  return (ret != NULL);
}

// Decode the contents of an encoded big integer and either create a new
// BIGNUM object (if |*num_ptr| is NULL on input) or copy it (if
// |*num_ptr| is not NULL).
// |new_bytes| is the byte encoding of the new value.
// |num_ptr| is the address of a BIGNUM pointer. |*num_ptr| can be NULL.
// Returns true on success, false otherwise. On failure, |*num_ptr| is
// not modified. On success, |*num_ptr| will always be non-NULL and
// point to a valid BIGNUM object.
bool SwapBigNumPtrFromBytes(const std::vector<uint8>& new_bytes,
                            BIGNUM** num_ptr) {
  BIGNUM* old_num = *num_ptr;
  BIGNUM* new_num = BN_bin2bn(
      reinterpret_cast<const unsigned char*>(&new_bytes[0]),
      static_cast<int>(new_bytes.size()),
      old_num);
  if (new_num == NULL)
    return false;

  if (old_num == NULL)
    *num_ptr = new_num;
  return true;
}

// Setup an EVP_PKEY to wrap an existing platform RSA PrivateKey object.
// |private_key| is the JNI reference (local or global) to the object.
// |pkey| is the EVP_PKEY to setup as a wrapper.
// Returns true on success, false otherwise.
// On success, this creates a new global JNI reference to the object
// that is owned by and destroyed with the EVP_PKEY. I.e. caller can
// free |private_key| after the call.
// IMPORTANT: The EVP_PKEY will *only* work on Android >= 4.2. For older
// platforms, use GetRsaLegacyKey() instead.
bool GetRsaPkeyWrapper(jobject private_key, EVP_PKEY* pkey) {
  ScopedRSA rsa(RSA_new());
  RSA_set_method(rsa.get(), &android_rsa_method);

  // HACK: RSA_size() doesn't work with custom RSA_METHODs. To ensure that
  // it will return the right value, set the 'n' field of the RSA object
  // to match the private key's modulus.
  std::vector<uint8> modulus;
  if (!GetRSAKeyModulus(private_key, &modulus)) {
    LOG(ERROR) << "Failed to get private key modulus";
    return false;
  }
  if (!SwapBigNumPtrFromBytes(modulus, &rsa.get()->n)) {
    LOG(ERROR) << "Failed to decode private key modulus";
    return false;
  }

  ScopedJavaGlobalRef<jobject> global_key;
  global_key.Reset(NULL, private_key);
  if (global_key.is_null()) {
    LOG(ERROR) << "Could not create global JNI reference";
    return false;
  }
  RSA_set_app_data(rsa.get(), global_key.Release());
  EVP_PKEY_assign_RSA(pkey, rsa.release());
  return true;
}

// On Android < 4.2, the libkeystore.so ENGINE uses CRYPTO_EX_DATA and is not
// added to the global engine list. If all references to it are dropped, OpenSSL
// will dlclose the module, leaving a dangling function pointer in the RSA
// CRYPTO_EX_DATA class. To work around this, leak an extra reference to the
// ENGINE we extract in GetRsaLegacyKey.
//
// In 4.2, this change avoids the problem:
// https://android.googlesource.com/platform/libcore/+/106a8928fb4249f2f3d4dba1dddbe73ca5cb3d61
//
// https://crbug.com/381465
class KeystoreEngineWorkaround {
 public:
  KeystoreEngineWorkaround() : leaked_engine_(false) {}

  void LeakRsaEngine(EVP_PKEY* pkey) {
    if (leaked_engine_)
      return;
    ScopedRSA rsa(EVP_PKEY_get1_RSA(pkey));
    if (!rsa.get() ||
        !rsa.get()->engine ||
        strcmp(ENGINE_get_id(rsa.get()->engine), "keystore") ||
        !ENGINE_init(rsa.get()->engine)) {
      NOTREACHED();
      return;
    }
    leaked_engine_ = true;
  }

 private:
  bool leaked_engine_;
};

void LeakRsaEngine(EVP_PKEY* pkey) {
  static base::LazyInstance<KeystoreEngineWorkaround>::Leaky s_instance =
      LAZY_INSTANCE_INITIALIZER;
  s_instance.Get().LeakRsaEngine(pkey);
}

// Setup an EVP_PKEY to wrap an existing platform RSA PrivateKey object
// for Android 4.0 to 4.1.x. Must only be used on Android < 4.2.
// |private_key| is a JNI reference (local or global) to the object.
// |pkey| is the EVP_PKEY to setup as a wrapper.
// Returns true on success, false otherwise.
EVP_PKEY* GetRsaLegacyKey(jobject private_key) {
  EVP_PKEY* sys_pkey =
      GetOpenSSLSystemHandleForPrivateKey(private_key);
  if (sys_pkey != NULL) {
    CRYPTO_add(&sys_pkey->references, 1, CRYPTO_LOCK_EVP_PKEY);
    LeakRsaEngine(sys_pkey);
  } else {
    // GetOpenSSLSystemHandleForPrivateKey() will fail on Android
    // 4.0.3 and earlier. However, it is possible to get the key
    // content with PrivateKey.getEncoded() on these platforms.
    // Note that this method may return NULL on 4.0.4 and later.
    std::vector<uint8> encoded;
    if (!GetPrivateKeyEncodedBytes(private_key, &encoded)) {
      LOG(ERROR) << "Can't get private key data!";
      return NULL;
    }
    const unsigned char* p =
        reinterpret_cast<const unsigned char*>(&encoded[0]);
    int len = static_cast<int>(encoded.size());
    sys_pkey = d2i_AutoPrivateKey(NULL, &p, len);
    if (sys_pkey == NULL) {
      LOG(ERROR) << "Can't convert private key data!";
      return NULL;
    }
  }
  return sys_pkey;
}

// Custom DSA_METHOD that uses the platform APIs.
// Note that for now, only signing through DSA_sign() is really supported.
// all other method pointers are either stubs returning errors, or no-ops.
// See <openssl/dsa.h> for exact declaration of DSA_METHOD.
//
// Note: There is no DSA_set_app_data() and DSA_get_app_data() functions,
//       but RSA_set_app_data() is defined as a simple macro that calls
//       RSA_set_ex_data() with a hard-coded index of 0, so this code
//       does the same thing here.

DSA_SIG* DsaMethodDoSign(const unsigned char* dgst,
                         int dlen,
                         DSA* dsa) {
  // Extract the JNI reference to the PrivateKey object.
  jobject private_key = reinterpret_cast<jobject>(DSA_get_ex_data(dsa, 0));
  if (private_key == NULL)
    return NULL;

  // Sign the message with it, calling platform APIs.
  std::vector<uint8> signature;
  if (!RawSignDigestWithPrivateKey(
          private_key,
          base::StringPiece(
              reinterpret_cast<const char*>(dgst),
              static_cast<size_t>(dlen)),
          &signature)) {
    return NULL;
  }

  // Note: With DSA, the actual signature might be smaller than DSA_size().
  size_t max_expected_size = static_cast<size_t>(DSA_size(dsa));
  if (signature.size() > max_expected_size) {
    LOG(ERROR) << "DSA Signature size mismatch, actual: "
               << signature.size() << ", expected <= "
               << max_expected_size;
    return NULL;
  }

  // Convert the signature into a DSA_SIG object.
  const unsigned char* sigbuf =
      reinterpret_cast<const unsigned char*>(&signature[0]);
  int siglen = static_cast<size_t>(signature.size());
  DSA_SIG* dsa_sig = d2i_DSA_SIG(NULL, &sigbuf, siglen);
  return dsa_sig;
}

int DsaMethodSignSetup(DSA* dsa,
                       BN_CTX* ctx_in,
                       BIGNUM** kinvp,
                       BIGNUM** rp) {
  NOTIMPLEMENTED();
  DSAerr(DSA_F_DSA_SIGN_SETUP, DSA_R_INVALID_DIGEST_TYPE);
  return -1;
}

int DsaMethodDoVerify(const unsigned char* dgst,
                      int dgst_len,
                      DSA_SIG* sig,
                      DSA* dsa) {
  NOTIMPLEMENTED();
  DSAerr(DSA_F_DSA_DO_VERIFY, DSA_R_INVALID_DIGEST_TYPE);
  return -1;
}

int DsaMethodFinish(DSA* dsa) {
  // Free the global JNI reference that was created with this
  // wrapper key.
  jobject key = reinterpret_cast<jobject>(DSA_get_ex_data(dsa,0));
  if (key != NULL) {
    DSA_set_ex_data(dsa, 0, NULL);
    ReleaseKey(key);
  }
  // Actual return value is ignored by OpenSSL. There are no docs
  // explaining what this is supposed to be.
  return 0;
}

const DSA_METHOD android_dsa_method = {
  /* .name = */ "Android signing-only DSA method",
  /* .dsa_do_sign = */ DsaMethodDoSign,
  /* .dsa_sign_setup = */ DsaMethodSignSetup,
  /* .dsa_do_verify = */ DsaMethodDoVerify,
  /* .dsa_mod_exp = */ NULL,
  /* .bn_mod_exp = */ NULL,
  /* .init = */ NULL,  // nothing to do here.
  /* .finish = */ DsaMethodFinish,
  /* .flags = */ 0,
  /* .app_data = */ NULL,
  /* .dsa_paramgem = */ NULL,
  /* .dsa_keygen = */ NULL
};

// Setup an EVP_PKEY to wrap an existing DSA platform PrivateKey object.
// |private_key| is a JNI reference (local or global) to the object.
// |pkey| is the EVP_PKEY to setup as a wrapper.
// Returns true on success, false otherwise.
// On success, this creates a global JNI reference to the same object
// that will be owned by and destroyed with the EVP_PKEY.
bool GetDsaPkeyWrapper(jobject private_key, EVP_PKEY* pkey) {
  ScopedDSA dsa(DSA_new());
  DSA_set_method(dsa.get(), &android_dsa_method);

  // DSA_size() doesn't work with custom DSA_METHODs. To ensure it
  // returns the right value, set the 'q' field in the DSA object to
  // match the parameter from the platform key.
  std::vector<uint8> q;
  if (!GetDSAKeyParamQ(private_key, &q)) {
    LOG(ERROR) << "Can't extract Q parameter from DSA private key";
    return false;
  }
  if (!SwapBigNumPtrFromBytes(q, &dsa.get()->q)) {
    LOG(ERROR) << "Can't decode Q parameter from DSA private key";
    return false;
  }

  ScopedJavaGlobalRef<jobject> global_key;
  global_key.Reset(NULL, private_key);
  if (global_key.is_null()) {
    LOG(ERROR) << "Could not create global JNI reference";
    return false;
  }
  DSA_set_ex_data(dsa.get(), 0, global_key.Release());
  EVP_PKEY_assign_DSA(pkey, dsa.release());
  return true;
}

// Custom ECDSA_METHOD that uses the platform APIs.
// Note that for now, only signing through ECDSA_sign() is really supported.
// all other method pointers are either stubs returning errors, or no-ops.
//
// Note: The ECDSA_METHOD structure doesn't have init/finish
//       methods. As such, the only way to to ensure the global
//       JNI reference is properly released when the EVP_PKEY is
//       destroyed is to use a custom EX_DATA type.

// Used to ensure that the global JNI reference associated with a custom
// EC_KEY + ECDSA_METHOD wrapper is released when its EX_DATA is destroyed
// (this function is called when EVP_PKEY_free() is called on the wrapper).
void ExDataFree(void* parent,
                void* ptr,
                CRYPTO_EX_DATA* ad,
                int idx,
                long argl,
                void* argp) {
  jobject private_key = reinterpret_cast<jobject>(ptr);
  if (private_key == NULL)
    return;

  CRYPTO_set_ex_data(ad, idx, NULL);
  ReleaseKey(private_key);
}

int ExDataDup(CRYPTO_EX_DATA* to,
              CRYPTO_EX_DATA* from,
              void* from_d,
              int idx,
              long argl,
              void* argp) {
  // This callback shall never be called with the current OpenSSL
  // implementation (the library only ever duplicates EX_DATA items
  // for SSL and BIO objects). But provide this to catch regressions
  // in the future.
  CHECK(false) << "ExDataDup was called for ECDSA custom key !?";
  // Return value is currently ignored by OpenSSL.
  return 0;
}

class EcdsaExDataIndex {
public:
  int ex_data_index() { return ex_data_index_; }

  EcdsaExDataIndex() {
    ex_data_index_ = ECDSA_get_ex_new_index(0,           // argl
                                            NULL,        // argp
                                            NULL,        // new_func
                                            ExDataDup,   // dup_func
                                            ExDataFree); // free_func
  }

private:
  int ex_data_index_;
};

// Returns the index of the custom EX_DATA used to store the JNI reference.
int EcdsaGetExDataIndex(void) {
  // Use a LazyInstance to perform thread-safe lazy initialization.
  // Use a leaky one, since OpenSSL doesn't provide a way to release
  // allocated EX_DATA indices.
  static base::LazyInstance<EcdsaExDataIndex>::Leaky s_instance =
      LAZY_INSTANCE_INITIALIZER;
  return s_instance.Get().ex_data_index();
}

ECDSA_SIG* EcdsaMethodDoSign(const unsigned char* dgst,
                             int dgst_len,
                             const BIGNUM* inv,
                             const BIGNUM* rp,
                             EC_KEY* eckey) {
  // Retrieve private key JNI reference.
  jobject private_key = reinterpret_cast<jobject>(
      ECDSA_get_ex_data(eckey, EcdsaGetExDataIndex()));
  if (!private_key) {
    LOG(WARNING) << "Null JNI reference passed to EcdsaMethodDoSign!";
    return NULL;
  }
  // Sign message with it through JNI.
  std::vector<uint8> signature;
  base::StringPiece digest(
      reinterpret_cast<const char*>(dgst),
      static_cast<size_t>(dgst_len));
  if (!RawSignDigestWithPrivateKey(
          private_key, digest, &signature)) {
    LOG(WARNING) << "Could not sign message in EcdsaMethodDoSign!";
    return NULL;
  }

  // Note: With ECDSA, the actual signature may be smaller than
  // ECDSA_size().
  size_t max_expected_size = static_cast<size_t>(ECDSA_size(eckey));
  if (signature.size() > max_expected_size) {
    LOG(ERROR) << "ECDSA Signature size mismatch, actual: "
               <<  signature.size() << ", expected <= "
               << max_expected_size;
    return NULL;
  }

  // Convert signature to ECDSA_SIG object
  const unsigned char* sigbuf =
      reinterpret_cast<const unsigned char*>(&signature[0]);
  long siglen = static_cast<long>(signature.size());
  return d2i_ECDSA_SIG(NULL, &sigbuf, siglen);
}

int EcdsaMethodSignSetup(EC_KEY* eckey,
                         BN_CTX* ctx,
                         BIGNUM** kinv,
                         BIGNUM** r) {
  NOTIMPLEMENTED();
  ECDSAerr(ECDSA_F_ECDSA_SIGN_SETUP, ECDSA_R_ERR_EC_LIB);
  return -1;
}

int EcdsaMethodDoVerify(const unsigned char* dgst,
                        int dgst_len,
                        const ECDSA_SIG* sig,
                        EC_KEY* eckey) {
  NOTIMPLEMENTED();
  ECDSAerr(ECDSA_F_ECDSA_DO_VERIFY, ECDSA_R_ERR_EC_LIB);
  return -1;
}

const ECDSA_METHOD android_ecdsa_method = {
  /* .name = */ "Android signing-only ECDSA method",
  /* .ecdsa_do_sign = */ EcdsaMethodDoSign,
  /* .ecdsa_sign_setup = */ EcdsaMethodSignSetup,
  /* .ecdsa_do_verify = */ EcdsaMethodDoVerify,
  /* .flags = */ 0,
  /* .app_data = */ NULL,
};

// Setup an EVP_PKEY to wrap an existing platform PrivateKey object.
// |private_key| is the JNI reference (local or global) to the object.
// |pkey| is the EVP_PKEY to setup as a wrapper.
// Returns true on success, false otherwise.
// On success, this creates a global JNI reference to the object that
// is owned by and destroyed with the EVP_PKEY. I.e. the caller shall
// always free |private_key| after the call.
bool GetEcdsaPkeyWrapper(jobject private_key, EVP_PKEY* pkey) {
  ScopedEC_KEY eckey(EC_KEY_new());
  ECDSA_set_method(eckey.get(), &android_ecdsa_method);

  // To ensure that ECDSA_size() works properly, craft a custom EC_GROUP
  // that has the same order than the private key.
  std::vector<uint8> order;
  if (!GetECKeyOrder(private_key, &order)) {
    LOG(ERROR) << "Can't extract order parameter from EC private key";
    return false;
  }
  ScopedEC_GROUP group(EC_GROUP_new(EC_GFp_nist_method()));
  if (!group.get()) {
    LOG(ERROR) << "Can't create new EC_GROUP";
    return false;
  }
  if (!CopyBigNumFromBytes(order, &group.get()->order)) {
    LOG(ERROR) << "Can't decode order from PrivateKey";
    return false;
  }
  EC_KEY_set_group(eckey.get(), group.release());

  ScopedJavaGlobalRef<jobject> global_key;
  global_key.Reset(NULL, private_key);
  if (global_key.is_null()) {
    LOG(ERROR) << "Can't create global JNI reference";
    return false;
  }
  ECDSA_set_ex_data(eckey.get(),
                    EcdsaGetExDataIndex(),
                    global_key.Release());

  EVP_PKEY_assign_EC_KEY(pkey, eckey.release());
  return true;
}

}  // namespace

EVP_PKEY* GetOpenSSLPrivateKeyWrapper(jobject private_key) {
  // Create new empty EVP_PKEY instance.
  ScopedEVP_PKEY pkey(EVP_PKEY_new());
  if (!pkey.get())
    return NULL;

  // Create sub key type, depending on private key's algorithm type.
  PrivateKeyType key_type = GetPrivateKeyType(private_key);
  switch (key_type) {
    case PRIVATE_KEY_TYPE_RSA:
      {
        // Route around platform bug: if Android < 4.2, then
        // base::android::RawSignDigestWithPrivateKey() cannot work, so
        // instead, obtain a raw EVP_PKEY* to the system object
        // backing this PrivateKey object.
        const int kAndroid42ApiLevel = 17;
        if (base::android::BuildInfo::GetInstance()->sdk_int() <
            kAndroid42ApiLevel) {
          EVP_PKEY* legacy_key = GetRsaLegacyKey(private_key);
          if (legacy_key == NULL)
            return NULL;
          pkey.reset(legacy_key);
        } else {
          // Running on Android 4.2.
          if (!GetRsaPkeyWrapper(private_key, pkey.get()))
            return NULL;
        }
      }
      break;
    case PRIVATE_KEY_TYPE_DSA:
      if (!GetDsaPkeyWrapper(private_key, pkey.get()))
        return NULL;
      break;
    case PRIVATE_KEY_TYPE_ECDSA:
      if (!GetEcdsaPkeyWrapper(private_key, pkey.get()))
        return NULL;
      break;
    default:
      LOG(WARNING)
          << "GetOpenSSLPrivateKeyWrapper() called with invalid key type";
      return NULL;
  }
  return pkey.release();
}

}  // namespace android
}  // namespace net
