/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "public/platform/WebCryptoAlgorithm.h"

#include "public/platform/WebCryptoAlgorithmParams.h"
#include "wtf/Assertions.h"
#include "wtf/OwnPtr.h"
#include "wtf/StdLibExtras.h"
#include "wtf/ThreadSafeRefCounted.h"

namespace blink {

namespace {

// A mapping from the algorithm ID to information about the algorithm.
const WebCryptoAlgorithmInfo algorithmIdToInfo[] = {
    { // Index 0
        "AES-CBC", {
            WebCryptoAlgorithmParamsTypeAesCbcParams, // Encrypt
            WebCryptoAlgorithmParamsTypeAesCbcParams, // Decrypt
            WebCryptoAlgorithmInfo::Undefined, // Sign
            WebCryptoAlgorithmInfo::Undefined, // Verify
            WebCryptoAlgorithmInfo::Undefined, // Digest
            WebCryptoAlgorithmParamsTypeAesKeyGenParams, // GenerateKey
            WebCryptoAlgorithmParamsTypeNone, // ImportKey
            WebCryptoAlgorithmInfo::Undefined, // DeriveKey
            WebCryptoAlgorithmInfo::Undefined, // DeriveBits
            WebCryptoAlgorithmParamsTypeAesCbcParams, // WrapKey
            WebCryptoAlgorithmParamsTypeAesCbcParams // UnwrapKey
        }
    }, { // Index 1
        "HMAC", {
            WebCryptoAlgorithmInfo::Undefined, // Encrypt
            WebCryptoAlgorithmInfo::Undefined, // Decrypt
            WebCryptoAlgorithmParamsTypeNone, // Sign
            WebCryptoAlgorithmParamsTypeNone, // Verify
            WebCryptoAlgorithmInfo::Undefined, // Digest
            WebCryptoAlgorithmParamsTypeHmacKeyGenParams, // GenerateKey
            WebCryptoAlgorithmParamsTypeHmacImportParams, // ImportKey
            WebCryptoAlgorithmInfo::Undefined, // DeriveKey
            WebCryptoAlgorithmInfo::Undefined, // DeriveBits
            WebCryptoAlgorithmInfo::Undefined, // WrapKey
            WebCryptoAlgorithmInfo::Undefined // UnwrapKey
        }
    }, { // Index 2
        "RSASSA-PKCS1-v1_5", {
            WebCryptoAlgorithmInfo::Undefined, // Encrypt
            WebCryptoAlgorithmInfo::Undefined, // Decrypt
            WebCryptoAlgorithmParamsTypeNone, // Sign
            WebCryptoAlgorithmParamsTypeNone, // Verify
            WebCryptoAlgorithmInfo::Undefined, // Digest
            WebCryptoAlgorithmParamsTypeRsaHashedKeyGenParams, // GenerateKey
            WebCryptoAlgorithmParamsTypeRsaHashedImportParams, // ImportKey
            WebCryptoAlgorithmInfo::Undefined, // DeriveKey
            WebCryptoAlgorithmInfo::Undefined, // DeriveBits
            WebCryptoAlgorithmInfo::Undefined, // WrapKey
            WebCryptoAlgorithmInfo::Undefined // UnwrapKey
        }
    }, { // Index 3
        "SHA-1", {
            WebCryptoAlgorithmInfo::Undefined, // Encrypt
            WebCryptoAlgorithmInfo::Undefined, // Decrypt
            WebCryptoAlgorithmInfo::Undefined, // Sign
            WebCryptoAlgorithmInfo::Undefined, // Verify
            WebCryptoAlgorithmParamsTypeNone, // Digest
            WebCryptoAlgorithmInfo::Undefined, // GenerateKey
            WebCryptoAlgorithmInfo::Undefined, // ImportKey
            WebCryptoAlgorithmInfo::Undefined, // DeriveKey
            WebCryptoAlgorithmInfo::Undefined, // DeriveBits
            WebCryptoAlgorithmInfo::Undefined, // WrapKey
            WebCryptoAlgorithmInfo::Undefined // UnwrapKey
        }
    }, { // Index 4
        "SHA-256", {
            WebCryptoAlgorithmInfo::Undefined, // Encrypt
            WebCryptoAlgorithmInfo::Undefined, // Decrypt
            WebCryptoAlgorithmInfo::Undefined, // Sign
            WebCryptoAlgorithmInfo::Undefined, // Verify
            WebCryptoAlgorithmParamsTypeNone, // Digest
            WebCryptoAlgorithmInfo::Undefined, // GenerateKey
            WebCryptoAlgorithmInfo::Undefined, // ImportKey
            WebCryptoAlgorithmInfo::Undefined, // DeriveKey
            WebCryptoAlgorithmInfo::Undefined, // DeriveBits
            WebCryptoAlgorithmInfo::Undefined, // WrapKey
            WebCryptoAlgorithmInfo::Undefined // UnwrapKey
        }
    }, { // Index 5
        "SHA-384", {
            WebCryptoAlgorithmInfo::Undefined, // Encrypt
            WebCryptoAlgorithmInfo::Undefined, // Decrypt
            WebCryptoAlgorithmInfo::Undefined, // Sign
            WebCryptoAlgorithmInfo::Undefined, // Verify
            WebCryptoAlgorithmParamsTypeNone, // Digest
            WebCryptoAlgorithmInfo::Undefined, // GenerateKey
            WebCryptoAlgorithmInfo::Undefined, // ImportKey
            WebCryptoAlgorithmInfo::Undefined, // DeriveKey
            WebCryptoAlgorithmInfo::Undefined, // DeriveBits
            WebCryptoAlgorithmInfo::Undefined, // WrapKey
            WebCryptoAlgorithmInfo::Undefined // UnwrapKey
        }
    }, { // Index 6
        "SHA-512", {
            WebCryptoAlgorithmInfo::Undefined, // Encrypt
            WebCryptoAlgorithmInfo::Undefined, // Decrypt
            WebCryptoAlgorithmInfo::Undefined, // Sign
            WebCryptoAlgorithmInfo::Undefined, // Verify
            WebCryptoAlgorithmParamsTypeNone, // Digest
            WebCryptoAlgorithmInfo::Undefined, // GenerateKey
            WebCryptoAlgorithmInfo::Undefined, // ImportKey
            WebCryptoAlgorithmInfo::Undefined, // DeriveKey
            WebCryptoAlgorithmInfo::Undefined, // DeriveBits
            WebCryptoAlgorithmInfo::Undefined, // WrapKey
            WebCryptoAlgorithmInfo::Undefined // UnwrapKey
        }
    }, { // Index 7
        "AES-GCM", {
            WebCryptoAlgorithmParamsTypeAesGcmParams, // Encrypt
            WebCryptoAlgorithmParamsTypeAesGcmParams, // Decrypt
            WebCryptoAlgorithmInfo::Undefined, // Sign
            WebCryptoAlgorithmInfo::Undefined, // Verify
            WebCryptoAlgorithmInfo::Undefined, // Digest
            WebCryptoAlgorithmParamsTypeAesKeyGenParams, // GenerateKey
            WebCryptoAlgorithmParamsTypeNone, // ImportKey
            WebCryptoAlgorithmInfo::Undefined, // DeriveKey
            WebCryptoAlgorithmInfo::Undefined, // DeriveBits
            WebCryptoAlgorithmParamsTypeAesGcmParams, // WrapKey
            WebCryptoAlgorithmParamsTypeAesGcmParams // UnwrapKey
        }
    }, { // Index 8
        "RSA-OAEP", {
            WebCryptoAlgorithmParamsTypeRsaOaepParams, // Encrypt
            WebCryptoAlgorithmParamsTypeRsaOaepParams, // Decrypt
            WebCryptoAlgorithmInfo::Undefined, // Sign
            WebCryptoAlgorithmInfo::Undefined, // Verify
            WebCryptoAlgorithmInfo::Undefined, // Digest
            WebCryptoAlgorithmParamsTypeRsaHashedKeyGenParams, // GenerateKey
            WebCryptoAlgorithmParamsTypeRsaHashedImportParams, // ImportKey
            WebCryptoAlgorithmInfo::Undefined, // DeriveKey
            WebCryptoAlgorithmInfo::Undefined, // DeriveBits
            WebCryptoAlgorithmParamsTypeRsaOaepParams, // WrapKey
            WebCryptoAlgorithmParamsTypeRsaOaepParams // UnwrapKey
        }
    }, { // Index 9
        "AES-CTR", {
            WebCryptoAlgorithmParamsTypeAesCtrParams, // Encrypt
            WebCryptoAlgorithmParamsTypeAesCtrParams, // Decrypt
            WebCryptoAlgorithmInfo::Undefined, // Sign
            WebCryptoAlgorithmInfo::Undefined, // Verify
            WebCryptoAlgorithmInfo::Undefined, // Digest
            WebCryptoAlgorithmParamsTypeAesKeyGenParams, // GenerateKey
            WebCryptoAlgorithmParamsTypeNone, // ImportKey
            WebCryptoAlgorithmInfo::Undefined, // DeriveKey
            WebCryptoAlgorithmInfo::Undefined, // DeriveBits
            WebCryptoAlgorithmParamsTypeAesCtrParams, // WrapKey
            WebCryptoAlgorithmParamsTypeAesCtrParams // UnwrapKey
        }
    }, { // Index 10
        "AES-KW", {
            WebCryptoAlgorithmInfo::Undefined, // Encrypt
            WebCryptoAlgorithmInfo::Undefined, // Decrypt
            WebCryptoAlgorithmInfo::Undefined, // Sign
            WebCryptoAlgorithmInfo::Undefined, // Verify
            WebCryptoAlgorithmInfo::Undefined, // Digest
            WebCryptoAlgorithmParamsTypeAesKeyGenParams, // GenerateKey
            WebCryptoAlgorithmParamsTypeNone, // ImportKey
            WebCryptoAlgorithmInfo::Undefined, // DeriveKey
            WebCryptoAlgorithmInfo::Undefined, // DeriveBits
            WebCryptoAlgorithmParamsTypeNone, // WrapKey
            WebCryptoAlgorithmParamsTypeNone // UnwrapKey
        }
    },
};

// Initializing the algorithmIdToInfo table above depends on knowing the enum
// values for algorithm IDs. If those ever change, the table will need to be
// updated.
COMPILE_ASSERT(WebCryptoAlgorithmIdAesCbc == 0, AesCbc_idDoesntMatch);
COMPILE_ASSERT(WebCryptoAlgorithmIdHmac == 1, Hmac_idDoesntMatch);
COMPILE_ASSERT(WebCryptoAlgorithmIdRsaSsaPkcs1v1_5 == 2, RsaSsaPkcs1v1_5_idDoesntMatch);
COMPILE_ASSERT(WebCryptoAlgorithmIdSha1 == 3, Sha1_idDoesntMatch);
COMPILE_ASSERT(WebCryptoAlgorithmIdSha256 == 4, Sha256_idDoesntMatch);
COMPILE_ASSERT(WebCryptoAlgorithmIdSha384 == 5, Sha384_idDoesntMatch);
COMPILE_ASSERT(WebCryptoAlgorithmIdSha512 == 6, Sha512_idDoesntMatch);
COMPILE_ASSERT(WebCryptoAlgorithmIdAesGcm == 7, AesGcm_idDoesntMatch);
COMPILE_ASSERT(WebCryptoAlgorithmIdRsaOaep == 8, RsaOaep_idDoesntMatch);
COMPILE_ASSERT(WebCryptoAlgorithmIdAesCtr == 9, AesCtr_idDoesntMatch);
COMPILE_ASSERT(WebCryptoAlgorithmIdAesKw == 10, AesKw_idDoesntMatch);
COMPILE_ASSERT(WebCryptoAlgorithmIdLast == 10, Last_idDoesntMatch);
COMPILE_ASSERT(10 == WebCryptoOperationLast, UpdateParamsMapping);

} // namespace

class WebCryptoAlgorithmPrivate : public ThreadSafeRefCounted<WebCryptoAlgorithmPrivate> {
public:
    WebCryptoAlgorithmPrivate(WebCryptoAlgorithmId id, PassOwnPtr<WebCryptoAlgorithmParams> params)
        : id(id)
        , params(params)
    {
    }

    WebCryptoAlgorithmId id;
    OwnPtr<WebCryptoAlgorithmParams> params;
};

WebCryptoAlgorithm::WebCryptoAlgorithm(WebCryptoAlgorithmId id, PassOwnPtr<WebCryptoAlgorithmParams> params)
    : m_private(adoptRef(new WebCryptoAlgorithmPrivate(id, params)))
{
}

WebCryptoAlgorithm WebCryptoAlgorithm::createNull()
{
    return WebCryptoAlgorithm();
}

WebCryptoAlgorithm WebCryptoAlgorithm::adoptParamsAndCreate(WebCryptoAlgorithmId id, WebCryptoAlgorithmParams* params)
{
    return WebCryptoAlgorithm(id, adoptPtr(params));
}

const WebCryptoAlgorithmInfo* WebCryptoAlgorithm::lookupAlgorithmInfo(WebCryptoAlgorithmId id)
{
    if (id < 0 || id >= WTF_ARRAY_LENGTH(algorithmIdToInfo))
        return 0;
    return &algorithmIdToInfo[id];
}

bool WebCryptoAlgorithm::isNull() const
{
    return m_private.isNull();
}

WebCryptoAlgorithmId WebCryptoAlgorithm::id() const
{
    ASSERT(!isNull());
    return m_private->id;
}

WebCryptoAlgorithmParamsType WebCryptoAlgorithm::paramsType() const
{
    ASSERT(!isNull());
    if (!m_private->params)
        return WebCryptoAlgorithmParamsTypeNone;
    return m_private->params->type();
}

const WebCryptoAesCbcParams* WebCryptoAlgorithm::aesCbcParams() const
{
    ASSERT(!isNull());
    if (paramsType() == WebCryptoAlgorithmParamsTypeAesCbcParams)
        return static_cast<WebCryptoAesCbcParams*>(m_private->params.get());
    return 0;
}

const WebCryptoAesCtrParams* WebCryptoAlgorithm::aesCtrParams() const
{
    ASSERT(!isNull());
    if (paramsType() == WebCryptoAlgorithmParamsTypeAesCtrParams)
        return static_cast<WebCryptoAesCtrParams*>(m_private->params.get());
    return 0;
}

const WebCryptoAesKeyGenParams* WebCryptoAlgorithm::aesKeyGenParams() const
{
    ASSERT(!isNull());
    if (paramsType() == WebCryptoAlgorithmParamsTypeAesKeyGenParams)
        return static_cast<WebCryptoAesKeyGenParams*>(m_private->params.get());
    return 0;
}

const WebCryptoHmacImportParams* WebCryptoAlgorithm::hmacImportParams() const
{
    ASSERT(!isNull());
    if (paramsType() == WebCryptoAlgorithmParamsTypeHmacImportParams)
        return static_cast<WebCryptoHmacImportParams*>(m_private->params.get());
    return 0;
}

const WebCryptoHmacKeyGenParams* WebCryptoAlgorithm::hmacKeyGenParams() const
{
    ASSERT(!isNull());
    if (paramsType() == WebCryptoAlgorithmParamsTypeHmacKeyGenParams)
        return static_cast<WebCryptoHmacKeyGenParams*>(m_private->params.get());
    return 0;
}

const WebCryptoAesGcmParams* WebCryptoAlgorithm::aesGcmParams() const
{
    ASSERT(!isNull());
    if (paramsType() == WebCryptoAlgorithmParamsTypeAesGcmParams)
        return static_cast<WebCryptoAesGcmParams*>(m_private->params.get());
    return 0;
}

const WebCryptoRsaOaepParams* WebCryptoAlgorithm::rsaOaepParams() const
{
    ASSERT(!isNull());
    if (paramsType() == WebCryptoAlgorithmParamsTypeRsaOaepParams)
        return static_cast<WebCryptoRsaOaepParams*>(m_private->params.get());
    return 0;
}

const WebCryptoRsaHashedImportParams* WebCryptoAlgorithm::rsaHashedImportParams() const
{
    ASSERT(!isNull());
    if (paramsType() == WebCryptoAlgorithmParamsTypeRsaHashedImportParams)
        return static_cast<WebCryptoRsaHashedImportParams*>(m_private->params.get());
    return 0;
}

const WebCryptoRsaHashedKeyGenParams* WebCryptoAlgorithm::rsaHashedKeyGenParams() const
{
    ASSERT(!isNull());
    if (paramsType() == WebCryptoAlgorithmParamsTypeRsaHashedKeyGenParams)
        return static_cast<WebCryptoRsaHashedKeyGenParams*>(m_private->params.get());
    return 0;
}

bool WebCryptoAlgorithm::isHash(WebCryptoAlgorithmId id)
{
    switch (id) {
    case WebCryptoAlgorithmIdSha1:
    case WebCryptoAlgorithmIdSha256:
    case WebCryptoAlgorithmIdSha384:
    case WebCryptoAlgorithmIdSha512:
        return true;
    case WebCryptoAlgorithmIdAesCbc:
    case WebCryptoAlgorithmIdHmac:
    case WebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
    case WebCryptoAlgorithmIdAesGcm:
    case WebCryptoAlgorithmIdRsaOaep:
    case WebCryptoAlgorithmIdAesCtr:
    case WebCryptoAlgorithmIdAesKw:
        break;
    }
    return false;
}

void WebCryptoAlgorithm::assign(const WebCryptoAlgorithm& other)
{
    m_private = other.m_private;
}

void WebCryptoAlgorithm::reset()
{
    m_private.reset();
}

} // namespace blink
