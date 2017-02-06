// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_ANDROID_KEYSTORE_OPENSSL_H
#define NET_ANDROID_KEYSTORE_OPENSSL_H

#include <jni.h>
#include <openssl/evp.h>

#include "crypto/scoped_openssl_types.h"
#include "net/base/net_export.h"

// The features provided here are highly implementation specific and are
// segregated from net/android/keystore.h because the latter only provides
// simply JNI stubs to call Java code which only uses platform APIs.

namespace net {
namespace android {

// Create a custom OpenSSL EVP_PKEY instance that wraps a platform
// java.security.PrivateKey object, and will call the platform APIs
// through JNI to implement signing (and only signing).
//
// This method can be called from any thread. It shall only be used
// to implement client certificate handling though.
//
// |private_key| is a JNI local (or global) reference to the Java
// PrivateKey object.
//
// Returns a new EVP_PKEY* object with the following features:
//
// - Only contains a private key.
//
// - Owns its own _global_ JNI reference to the object. This means the
//   caller can free |private_key| safely after the call, and that the
//   the returned EVP_PKEY instance can be used from any thread.
//
// - Uses a custom method to implement the minimum functions required to
//   *sign* the digest that is part of the "Verify Certificate" message
//   during the OpenSSL handshake. Anything else will result in undefined
//   behaviour.
NET_EXPORT crypto::ScopedEVP_PKEY GetOpenSSLPrivateKeyWrapper(
    jobject private_key);

}  // namespace android
}  // namespace net

#endif  // NET_ANDROID_KEYSTORE_OPENSSL_H
