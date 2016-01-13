// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jww) The original Blink-style header guard for this file conflicts with
// the header guard in Source/modules/crypto/Crypto.h, so this is a
// Chromium-style header guard instead. There is now a bug
// (https://crbug.com/360121) to track a proposal to change all header guards
// to a similar style. Thus, whenever that is resolved, this header guard
// should be changed to whatever style is agreed upon.
#ifndef SOURCE_PLATFORM_CRYPTO_H_
#define SOURCE_PLATFORM_CRYPTO_H_

#include "platform/PlatformExport.h"
#include "public/platform/WebCrypto.h"
#include "wtf/HashSet.h"
#include "wtf/StringHasher.h"
#include "wtf/Vector.h"

namespace WebCore {

static const size_t kMaxDigestSize = 64;
typedef Vector<uint8_t, kMaxDigestSize> DigestValue;

const size_t sha1HashSize = 20;
enum HashAlgorithm {
    HashAlgorithmSha1,
    HashAlgorithmSha256,
    HashAlgorithmSha384,
    HashAlgorithmSha512
};

PLATFORM_EXPORT bool computeDigest(HashAlgorithm, const char* digestable, size_t length, DigestValue& digestResult);
PLATFORM_EXPORT PassOwnPtr<blink::WebCryptoDigestor> createDigestor(HashAlgorithm);
PLATFORM_EXPORT void finishDigestor(blink::WebCryptoDigestor*, DigestValue& digestResult);

} // namespace WebCore

namespace WTF {

struct DigestValueHash {
    static unsigned hash(const WebCore::DigestValue& v)
    {
        return StringHasher::computeHash(v.data(), v.size());
    }
    static bool equal(const WebCore::DigestValue& a, const WebCore::DigestValue& b)
    {
        return a == b;
    };
    static const bool safeToCompareToEmptyOrDeleted = true;
};
template <>
struct DefaultHash<WebCore::DigestValue> {
    typedef DigestValueHash Hash;
};

template <>
struct DefaultHash<WebCore::HashAlgorithm> {
    typedef IntHash<WebCore::HashAlgorithm> Hash;
};
template <>
struct HashTraits<WebCore::HashAlgorithm> : UnsignedWithZeroKeyHashTraits<WebCore::HashAlgorithm> {
};

} // namespace WTF
#endif // SOURCE_PLATFORM_CRYPTO_H_
