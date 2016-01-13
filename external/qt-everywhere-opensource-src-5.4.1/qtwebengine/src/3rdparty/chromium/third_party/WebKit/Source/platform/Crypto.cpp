// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/Crypto.h"

#include "public/platform/Platform.h"
#include "public/platform/WebArrayBuffer.h"
#include "public/platform/WebCrypto.h"
#include "public/platform/WebCryptoAlgorithm.h"

namespace WebCore {

static blink::WebCryptoAlgorithmId toWebCryptoAlgorithmId(HashAlgorithm algorithm)
{
    switch (algorithm) {
    case HashAlgorithmSha1:
        return blink::WebCryptoAlgorithmIdSha1;
    case HashAlgorithmSha256:
        return blink::WebCryptoAlgorithmIdSha256;
    case HashAlgorithmSha384:
        return blink::WebCryptoAlgorithmIdSha384;
    case HashAlgorithmSha512:
        return blink::WebCryptoAlgorithmIdSha512;
    };

    ASSERT_NOT_REACHED();
    return blink::WebCryptoAlgorithmIdSha256;
}

bool computeDigest(HashAlgorithm algorithm, const char* digestable, size_t length, DigestValue& digestResult)
{
    blink::WebCryptoAlgorithmId algorithmId = toWebCryptoAlgorithmId(algorithm);
    blink::WebCrypto* crypto = blink::Platform::current()->crypto();
    unsigned char* result;
    unsigned resultSize;

    ASSERT(crypto);

    OwnPtr<blink::WebCryptoDigestor> digestor = adoptPtr(crypto->createDigestor(algorithmId));
    if (!digestor.get() || !digestor->consume(reinterpret_cast<const unsigned char*>(digestable), length) || !digestor->finish(result, resultSize))
        return false;

    digestResult.append(static_cast<uint8_t*>(result), resultSize);
    return true;
}

PassOwnPtr<blink::WebCryptoDigestor> createDigestor(HashAlgorithm algorithm)
{
    return adoptPtr(blink::Platform::current()->crypto()->createDigestor(toWebCryptoAlgorithmId(algorithm)));
}

void finishDigestor(blink::WebCryptoDigestor* digestor, DigestValue& digestResult)
{
    unsigned char* result = 0;
    unsigned resultSize = 0;

    if (!digestor->finish(result, resultSize))
        return;

    ASSERT(result);

    digestResult.append(static_cast<uint8_t*>(result), resultSize);
}

} // namespace WebCore
