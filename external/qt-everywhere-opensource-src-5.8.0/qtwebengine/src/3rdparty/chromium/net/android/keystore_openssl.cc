// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/keystore_openssl.h"

#include <jni.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <stdint.h>

#include <memory>

#include "base/android/build_info.h"
#include "base/android/scoped_java_ref.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "crypto/openssl_util.h"
#include "net/android/keystore.h"
#include "net/android/legacy_openssl.h"
#include "net/ssl/scoped_openssl_types.h"
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
// Internally, it can hold a pointer to a RSA or ECDSA structure, which model
// keypair implementations of each respective crypto algorithm.
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
// However, the platform APIs require a jobject JNI reference to work. It must
// be stored in the RSA instance, or made accessible when the custom RSA
// methods are called. This is done by storing it in a |KeyExData| structure
// that's referenced by the key using |EX_DATA|.

using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace net {
namespace android {

namespace {

extern const RSA_METHOD android_rsa_method;
extern const ECDSA_METHOD android_ecdsa_method;

// KeyExData contains the data that is contained in the EX_DATA of the RSA and
// EC_KEY objects that are created to wrap Android system keys.
struct KeyExData {
  // private_key contains a reference to a Java, private-key object.
  ScopedJavaGlobalRef<jobject> private_key;
  // legacy_rsa, if not NULL, points to an RSA* in the system's OpenSSL (which
  // might not be ABI compatible with Chromium).
  AndroidRSA* legacy_rsa;
  // cached_size contains the "size" of the key. This is the size of the
  // modulus (in bytes) for RSA, or the group order size for ECDSA. This
  // avoids calling into Java to calculate the size.
  size_t cached_size;
};

// ExDataDup is called when one of the RSA or EC_KEY objects is duplicated. We
// don't support this and it should never happen.
int ExDataDup(CRYPTO_EX_DATA* to,
              const CRYPTO_EX_DATA* from,
              void** from_d,
              int index,
              long argl,
              void* argp) {
  CHECK_EQ((void*)NULL, *from_d);
  return 0;
}

// ExDataFree is called when one of the RSA or EC_KEY objects is freed.
void ExDataFree(void* parent,
                void* ptr,
                CRYPTO_EX_DATA* ad,
                int index,
                long argl,
                void* argp) {
  // Ensure the global JNI reference created with this wrapper is
  // properly destroyed with it.
  KeyExData* ex_data = reinterpret_cast<KeyExData*>(ptr);
  delete ex_data;
}

// BoringSSLEngine is a BoringSSL ENGINE that implements RSA and ECDSA by
// forwarding the requested operations to the Java libraries.
class BoringSSLEngine {
 public:
  BoringSSLEngine()
      : rsa_index_(RSA_get_ex_new_index(0 /* argl */,
                                        NULL /* argp */,
                                        NULL /* new_func */,
                                        ExDataDup,
                                        ExDataFree)),
        ec_key_index_(EC_KEY_get_ex_new_index(0 /* argl */,
                                              NULL /* argp */,
                                              NULL /* new_func */,
                                              ExDataDup,
                                              ExDataFree)),
        engine_(ENGINE_new()) {
    ENGINE_set_RSA_method(engine_, &android_rsa_method,
                          sizeof(android_rsa_method));
    ENGINE_set_ECDSA_method(engine_, &android_ecdsa_method,
                            sizeof(android_ecdsa_method));
  }

  int rsa_ex_index() const { return rsa_index_; }
  int ec_key_ex_index() const { return ec_key_index_; }

  const ENGINE* engine() const { return engine_; }

 private:
  const int rsa_index_;
  const int ec_key_index_;
  ENGINE* const engine_;
};

base::LazyInstance<BoringSSLEngine>::Leaky global_boringssl_engine =
    LAZY_INSTANCE_INITIALIZER;

// VectorBignumSize returns the number of bytes needed to represent the bignum
// given in |v|, i.e. the length of |v| less any leading zero bytes.
size_t VectorBignumSize(const std::vector<uint8_t>& v) {
  size_t size = v.size();
  // Ignore any leading zero bytes.
  for (size_t i = 0; i < v.size() && v[i] == 0; i++) {
    size--;
  }
  return size;
}

KeyExData* RsaGetExData(const RSA* rsa) {
  return reinterpret_cast<KeyExData*>(
      RSA_get_ex_data(rsa, global_boringssl_engine.Get().rsa_ex_index()));
}

size_t RsaMethodSize(const RSA* rsa) {
  const KeyExData* ex_data = RsaGetExData(rsa);
  return ex_data->cached_size;
}

int RsaMethodEncrypt(RSA* rsa,
                     size_t* out_len,
                     uint8_t* out,
                     size_t max_out,
                     const uint8_t* in,
                     size_t in_len,
                     int padding) {
  NOTIMPLEMENTED();
  OPENSSL_PUT_ERROR(RSA, RSA_R_UNKNOWN_ALGORITHM_TYPE);
  return 0;
}

int RsaMethodSignRaw(RSA* rsa,
                     size_t* out_len,
                     uint8_t* out,
                     size_t max_out,
                     const uint8_t* in,
                     size_t in_len,
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
    OPENSSL_PUT_ERROR(RSA, RSA_R_UNKNOWN_PADDING_TYPE);
    return 0;
  }

  // Retrieve private key JNI reference.
  const KeyExData* ex_data = RsaGetExData(rsa);
  if (!ex_data || !ex_data->private_key.obj()) {
    LOG(WARNING) << "Null JNI reference passed to RsaMethodSignRaw!";
    OPENSSL_PUT_ERROR(RSA, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  // Pre-4.2 legacy codepath.
  if (ex_data->legacy_rsa) {
    int ret = ex_data->legacy_rsa->meth->rsa_priv_enc(
        in_len, in, out, ex_data->legacy_rsa, ANDROID_RSA_PKCS1_PADDING);
    if (ret < 0) {
      LOG(WARNING) << "Could not sign message in RsaMethodSignRaw!";
      // System OpenSSL will use a separate error queue, so it is still
      // necessary to push a new error.
      //
      // TODO(davidben): It would be good to also clear the system error queue
      // if there were some way to convince Java to do it. (Without going
      // through Java, it's difficult to get a handle on a system OpenSSL
      // function; dlopen loads a second copy.)
      OPENSSL_PUT_ERROR(RSA, ERR_R_INTERNAL_ERROR);
      return 0;
    }
    *out_len = ret;
    return 1;
  }

  base::StringPiece from_piece(reinterpret_cast<const char*>(in), in_len);
  std::vector<uint8_t> result;
  // For RSA keys, this function behaves as RSA_private_encrypt with
  // PKCS#1 padding.
  if (!RawSignDigestWithPrivateKey(ex_data->private_key.obj(), from_piece,
                                   &result)) {
    LOG(WARNING) << "Could not sign message in RsaMethodSignRaw!";
    OPENSSL_PUT_ERROR(RSA, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  size_t expected_size = static_cast<size_t>(RSA_size(rsa));
  if (result.size() > expected_size) {
    LOG(ERROR) << "RSA Signature size mismatch, actual: " << result.size()
               << ", expected <= " << expected_size;
    OPENSSL_PUT_ERROR(RSA, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  if (max_out < expected_size) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_DATA_TOO_LARGE);
    return 0;
  }

  // Copy result to OpenSSL-provided buffer. RawSignDigestWithPrivateKey
  // should pad with leading 0s, but if it doesn't, pad the result.
  size_t zero_pad = expected_size - result.size();
  memset(out, 0, zero_pad);
  memcpy(out + zero_pad, &result[0], result.size());
  *out_len = expected_size;

  return 1;
}

int RsaMethodDecrypt(RSA* rsa,
                     size_t* out_len,
                     uint8_t* out,
                     size_t max_out,
                     const uint8_t* in,
                     size_t in_len,
                     int padding) {
  NOTIMPLEMENTED();
  OPENSSL_PUT_ERROR(RSA, RSA_R_UNKNOWN_ALGORITHM_TYPE);
  return 0;
}

int RsaMethodVerifyRaw(RSA* rsa,
                       size_t* out_len,
                       uint8_t* out,
                       size_t max_out,
                       const uint8_t* in,
                       size_t in_len,
                       int padding) {
  NOTIMPLEMENTED();
  OPENSSL_PUT_ERROR(RSA, RSA_R_UNKNOWN_ALGORITHM_TYPE);
  return 0;
}

const RSA_METHOD android_rsa_method = {
    {
        0 /* references */, 1 /* is_static */
    } /* common */,
    nullptr /* app_data */,

    nullptr /* init */,
    nullptr /* finish */,
    RsaMethodSize,
    nullptr /* sign */,
    nullptr /* verify */,
    RsaMethodEncrypt,
    RsaMethodSignRaw,
    RsaMethodDecrypt,
    RsaMethodVerifyRaw,
    nullptr /* private_transform */,
    nullptr /* mod_exp */,
    nullptr /* bn_mod_exp */,
    RSA_FLAG_OPAQUE,
    nullptr /* keygen */,
    nullptr /* multi_prime_keygen */,
    nullptr /* supports_digest */,
};

// Setup an EVP_PKEY to wrap an existing platform RSA PrivateKey object.
// |private_key| is the JNI reference (local or global) to the object.
// |legacy_rsa|, if non-NULL, is a pointer to the system OpenSSL RSA object
// backing |private_key|. This parameter is only used for Android < 4.2 to
// implement key operations not exposed by the platform.
// Returns a new EVP_PKEY on success, NULL otherwise.
// On success, this creates a new global JNI reference to the object
// that is owned by and destroyed with the EVP_PKEY. I.e. caller can
// free |private_key| after the call.
crypto::ScopedEVP_PKEY CreateRsaPkeyWrapper(
    jobject private_key,
    AndroidRSA* legacy_rsa,
    const crypto::OpenSSLErrStackTracer& tracer) {
  crypto::ScopedRSA rsa(RSA_new_method(global_boringssl_engine.Get().engine()));

  std::vector<uint8_t> modulus;
  if (!GetRSAKeyModulus(private_key, &modulus)) {
    LOG(ERROR) << "Failed to get private key modulus";
    return nullptr;
  }

  std::unique_ptr<KeyExData> ex_data(new KeyExData);
  ex_data->private_key.Reset(nullptr, private_key);
  if (ex_data->private_key.is_null()) {
    LOG(ERROR) << "Could not create global JNI reference";
    return nullptr;
  }
  ex_data->legacy_rsa = legacy_rsa;
  ex_data->cached_size = VectorBignumSize(modulus);

  RSA_set_ex_data(rsa.get(), global_boringssl_engine.Get().rsa_ex_index(),
                  ex_data.release());

  crypto::ScopedEVP_PKEY pkey(EVP_PKEY_new());
  if (!pkey || !EVP_PKEY_set1_RSA(pkey.get(), rsa.get()))
    return nullptr;
  return pkey;
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
  KeystoreEngineWorkaround() {}

  void LeakEngine(jobject private_key) {
    if (!engine_.is_null())
      return;
    ScopedJavaLocalRef<jobject> engine =
        GetOpenSSLEngineForPrivateKey(private_key);
    if (engine.is_null()) {
      NOTREACHED();
      return;
    }
    engine_.Reset(engine);
  }

 private:
  ScopedJavaGlobalRef<jobject> engine_;
};

void LeakEngine(jobject private_key) {
  static base::LazyInstance<KeystoreEngineWorkaround>::Leaky s_instance =
      LAZY_INSTANCE_INITIALIZER;
  s_instance.Get().LeakEngine(private_key);
}

// Creates an EVP_PKEY wrapper corresponding to the RSA key
// |private_key|. Returns nullptr on failure.
crypto::ScopedEVP_PKEY GetRsaPkeyWrapper(jobject private_key) {
  const int kAndroid42ApiLevel = 17;
  crypto::OpenSSLErrStackTracer tracer(FROM_HERE);

  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      kAndroid42ApiLevel) {
    return CreateRsaPkeyWrapper(private_key, nullptr, tracer);
  }

  // Route around platform limitation: if Android < 4.2, then
  // base::android::RawSignDigestWithPrivateKey() cannot work, so try to get the
  // system OpenSSL's EVP_PKEY backing this PrivateKey object.
  AndroidEVP_PKEY* sys_pkey = GetOpenSSLSystemHandleForPrivateKey(private_key);
  if (sys_pkey == nullptr)
    return nullptr;

  if (sys_pkey->type != ANDROID_EVP_PKEY_RSA) {
    LOG(ERROR) << "Private key has wrong type!";
    return nullptr;
  }

  AndroidRSA* sys_rsa = sys_pkey->pkey.rsa;
  if (sys_rsa->engine) {
    // |private_key| may not have an engine if the PrivateKey did not come
    // from the key store, such as in unit tests.
    if (strcmp(sys_rsa->engine->id, "keystore") == 0) {
      LeakEngine(private_key);
    } else {
      NOTREACHED();
    }
  }

  return CreateRsaPkeyWrapper(private_key, sys_rsa, tracer);
}

// Custom ECDSA_METHOD that uses the platform APIs.
// Note that for now, only signing through ECDSA_sign() is really supported.
// all other method pointers are either stubs returning errors, or no-ops.

jobject EcKeyGetKey(const EC_KEY* ec_key) {
  KeyExData* ex_data = reinterpret_cast<KeyExData*>(EC_KEY_get_ex_data(
      ec_key, global_boringssl_engine.Get().ec_key_ex_index()));
  return ex_data->private_key.obj();
}

size_t EcdsaMethodGroupOrderSize(const EC_KEY* ec_key) {
  KeyExData* ex_data = reinterpret_cast<KeyExData*>(EC_KEY_get_ex_data(
      ec_key, global_boringssl_engine.Get().ec_key_ex_index()));
  return ex_data->cached_size;
}

int EcdsaMethodSign(const uint8_t* digest,
                    size_t digest_len,
                    uint8_t* sig,
                    unsigned int* sig_len,
                    EC_KEY* ec_key) {
  // Retrieve private key JNI reference.
  jobject private_key = EcKeyGetKey(ec_key);
  if (!private_key) {
    LOG(WARNING) << "Null JNI reference passed to EcdsaMethodSign!";
    return 0;
  }
  // Sign message with it through JNI.
  std::vector<uint8_t> signature;
  base::StringPiece digest_sp(reinterpret_cast<const char*>(digest),
                              digest_len);
  if (!RawSignDigestWithPrivateKey(private_key, digest_sp, &signature)) {
    LOG(WARNING) << "Could not sign message in EcdsaMethodSign!";
    return 0;
  }

  // Note: With ECDSA, the actual signature may be smaller than
  // ECDSA_size().
  size_t max_expected_size = ECDSA_size(ec_key);
  if (signature.size() > max_expected_size) {
    LOG(ERROR) << "ECDSA Signature size mismatch, actual: " << signature.size()
               << ", expected <= " << max_expected_size;
    return 0;
  }

  memcpy(sig, &signature[0], signature.size());
  *sig_len = signature.size();
  return 1;
}

int EcdsaMethodVerify(const uint8_t* digest,
                      size_t digest_len,
                      const uint8_t* sig,
                      size_t sig_len,
                      EC_KEY* ec_key) {
  NOTIMPLEMENTED();
  OPENSSL_PUT_ERROR(ECDSA, ECDSA_R_NOT_IMPLEMENTED);
  return 0;
}

// Setup an EVP_PKEY to wrap an existing platform PrivateKey object.
// |private_key| is the JNI reference (local or global) to the object.
// Returns a new EVP_PKEY on success, NULL otherwise.
// On success, this creates a global JNI reference to the object that
// is owned by and destroyed with the EVP_PKEY. I.e. the caller shall
// always free |private_key| after the call.
crypto::ScopedEVP_PKEY GetEcdsaPkeyWrapper(jobject private_key) {
  crypto::OpenSSLErrStackTracer tracer(FROM_HERE);
  crypto::ScopedEC_KEY ec_key(
      EC_KEY_new_method(global_boringssl_engine.Get().engine()));

  std::vector<uint8_t> order;
  if (!GetECKeyOrder(private_key, &order)) {
    LOG(ERROR) << "Can't extract order parameter from EC private key";
    return nullptr;
  }

  std::unique_ptr<KeyExData> ex_data(new KeyExData);
  ex_data->private_key.Reset(nullptr, private_key);
  if (ex_data->private_key.is_null()) {
    LOG(ERROR) << "Can't create global JNI reference";
    return nullptr;
  }
  ex_data->legacy_rsa = nullptr;
  ex_data->cached_size = VectorBignumSize(order);

  EC_KEY_set_ex_data(ec_key.get(),
                     global_boringssl_engine.Get().ec_key_ex_index(),
                     ex_data.release());

  crypto::ScopedEVP_PKEY pkey(EVP_PKEY_new());
  if (!pkey || !EVP_PKEY_set1_EC_KEY(pkey.get(), ec_key.get()))
    return nullptr;
  return pkey;
}

const ECDSA_METHOD android_ecdsa_method = {
    {
        0 /* references */, 1 /* is_static */
    } /* common */,
    NULL /* app_data */,

    NULL /* init */,
    NULL /* finish */,
    EcdsaMethodGroupOrderSize,
    EcdsaMethodSign,
    EcdsaMethodVerify,
    ECDSA_FLAG_OPAQUE,
};

}  // namespace

crypto::ScopedEVP_PKEY GetOpenSSLPrivateKeyWrapper(jobject private_key) {
  // Create sub key type, depending on private key's algorithm type.
  PrivateKeyType key_type = GetPrivateKeyType(private_key);
  switch (key_type) {
    case PRIVATE_KEY_TYPE_RSA:
      return GetRsaPkeyWrapper(private_key);
    case PRIVATE_KEY_TYPE_ECDSA:
      return GetEcdsaPkeyWrapper(private_key);
    default:
      LOG(WARNING)
          << "GetOpenSSLPrivateKeyWrapper() called with invalid key type";
      return nullptr;
  }
}

}  // namespace android
}  // namespace net
