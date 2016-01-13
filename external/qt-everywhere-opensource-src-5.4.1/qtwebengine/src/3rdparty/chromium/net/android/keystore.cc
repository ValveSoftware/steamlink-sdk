// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/keystore.h"

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/logging.h"
#include "jni/AndroidKeyStore_jni.h"
#include "net/android/android_private_key.h"

using base::android::AttachCurrentThread;
using base::android::HasException;
using base::android::JavaByteArrayToByteVector;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;
using base::android::JavaArrayOfByteArrayToStringVector;

namespace net {
namespace android {

bool GetRSAKeyModulus(
    jobject private_key_ref,
    std::vector<uint8>* result) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jbyteArray> modulus_ref =
      Java_AndroidKeyStore_getRSAKeyModulus(env,
                                            GetKeyStore(private_key_ref).obj(),
                                            private_key_ref);
  if (modulus_ref.is_null())
    return false;

  JavaByteArrayToByteVector(env, modulus_ref.obj(), result);
  return true;
}

bool GetDSAKeyParamQ(jobject private_key_ref,
                     std::vector<uint8>* result) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jbyteArray> q_ref =
      Java_AndroidKeyStore_getDSAKeyParamQ(
          env,
          GetKeyStore(private_key_ref).obj(),
          private_key_ref);
  if (q_ref.is_null())
    return false;

  JavaByteArrayToByteVector(env, q_ref.obj(), result);
  return true;
}

bool GetECKeyOrder(jobject private_key_ref,
                   std::vector<uint8>* result) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jbyteArray> order_ref =
      Java_AndroidKeyStore_getECKeyOrder(
          env,
          GetKeyStore(private_key_ref).obj(),
          private_key_ref);

  if (order_ref.is_null())
    return false;

  JavaByteArrayToByteVector(env, order_ref.obj(), result);
  return true;
}

bool GetPrivateKeyEncodedBytes(jobject private_key_ref,
                               std::vector<uint8>* result) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jbyteArray> encoded_ref =
      Java_AndroidKeyStore_getPrivateKeyEncodedBytes(
          env,
          GetKeyStore(private_key_ref).obj(),
          private_key_ref);
  if (encoded_ref.is_null())
    return false;

  JavaByteArrayToByteVector(env, encoded_ref.obj(), result);
  return true;
}

bool RawSignDigestWithPrivateKey(
    jobject private_key_ref,
    const base::StringPiece& digest,
    std::vector<uint8>* signature) {
  JNIEnv* env = AttachCurrentThread();

  // Convert message to byte[] array.
  ScopedJavaLocalRef<jbyteArray> digest_ref =
      ToJavaByteArray(env,
                      reinterpret_cast<const uint8*>(digest.data()),
                      digest.length());
  DCHECK(!digest_ref.is_null());

  // Invoke platform API
  ScopedJavaLocalRef<jbyteArray> signature_ref =
      Java_AndroidKeyStore_rawSignDigestWithPrivateKey(
          env,
          GetKeyStore(private_key_ref).obj(),
          private_key_ref,
          digest_ref.obj());
  if (HasException(env) || signature_ref.is_null())
    return false;

  // Write signature to string.
  JavaByteArrayToByteVector(env, signature_ref.obj(), signature);
  return true;
}

PrivateKeyType GetPrivateKeyType(jobject private_key_ref) {
  JNIEnv* env = AttachCurrentThread();
  int type = Java_AndroidKeyStore_getPrivateKeyType(
      env,
      GetKeyStore(private_key_ref).obj(),
      private_key_ref);
  return static_cast<PrivateKeyType>(type);
}

EVP_PKEY* GetOpenSSLSystemHandleForPrivateKey(jobject private_key_ref) {
  JNIEnv* env = AttachCurrentThread();
  // Note: the pointer is passed as a jint here because that's how it
  // is stored in the Java object. Java doesn't have a primitive type
  // like intptr_t that matches the size of pointers on the host
  // machine, and Android only runs on 32-bit CPUs.
  //
  // Given that this routine shall only be called on Android < 4.2,
  // this won't be a problem in the far future (e.g. when Android gets
  // ported to 64-bit environments, if ever).
  long pkey = Java_AndroidKeyStore_getOpenSSLHandleForPrivateKey(
      env,
      GetKeyStore(private_key_ref).obj(),
      private_key_ref);
  return reinterpret_cast<EVP_PKEY*>(pkey);
}

void ReleaseKey(jobject private_key_ref) {
  JNIEnv* env = AttachCurrentThread();
  Java_AndroidKeyStore_releaseKey(env,
                                  GetKeyStore(private_key_ref).obj(),
                                  private_key_ref);
  env->DeleteGlobalRef(private_key_ref);
}

bool RegisterKeyStore(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace net
