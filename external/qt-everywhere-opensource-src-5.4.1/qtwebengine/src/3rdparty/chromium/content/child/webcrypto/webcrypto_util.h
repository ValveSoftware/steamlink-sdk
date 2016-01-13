// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBCRYPTO_WEBCRYPTO_UTIL_H_
#define CONTENT_CHILD_WEBCRYPTO_WEBCRYPTO_UTIL_H_

#include <string>
#include <vector>
#include "base/basictypes.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoKey.h"

namespace content {

namespace webcrypto {

class Status;

// Returns a pointer to the start of |data|, or NULL if it is empty. This is a
// convenience function for getting the pointer, and should not be used beyond
// the expected lifetime of |data|.
CONTENT_EXPORT const uint8* Uint8VectorStart(const std::vector<uint8>& data);
CONTENT_EXPORT uint8* Uint8VectorStart(std::vector<uint8>* data);

// This function decodes unpadded 'base64url' encoded data, as described in
// RFC4648 (http://www.ietf.org/rfc/rfc4648.txt) Section 5.
// In Web Crypto, this type of encoding is only used inside JWK.
CONTENT_EXPORT bool Base64DecodeUrlSafe(const std::string& input,
                                        std::string* output);

// Returns an unpadded 'base64url' encoding of the input data, the opposite of
// Base64DecodeUrlSafe() above.
CONTENT_EXPORT std::string Base64EncodeUrlSafe(const base::StringPiece& input);
CONTENT_EXPORT std::string Base64EncodeUrlSafe(const std::vector<uint8>& input);

// Composes a Web Crypto usage mask from an array of JWK key_ops values.
CONTENT_EXPORT Status GetWebCryptoUsagesFromJwkKeyOps(
    const base::ListValue* jwk_key_ops_value,
    blink::WebCryptoKeyUsageMask* jwk_key_ops_mask);

// Composes a JWK key_ops array from a Web Crypto usage mask.
base::ListValue* CreateJwkKeyOpsFromWebCryptoUsages(
    blink::WebCryptoKeyUsageMask usage_mask);

// Returns the "hash" param for an algorithm if it exists, otherwise returns
// a null algorithm.
blink::WebCryptoAlgorithm GetInnerHashAlgorithm(
    const blink::WebCryptoAlgorithm& algorithm);

// Creates a WebCryptoAlgorithm without any parameters.
CONTENT_EXPORT blink::WebCryptoAlgorithm CreateAlgorithm(
    blink::WebCryptoAlgorithmId id);

// Creates an HMAC import algorithm whose inner hash algorithm is determined by
// the specified algorithm ID. It is an error to call this method with a hash
// algorithm that is not SHA*.
CONTENT_EXPORT blink::WebCryptoAlgorithm CreateHmacImportAlgorithm(
    blink::WebCryptoAlgorithmId hash_id);

// Creates an import algorithm for RSA algorithms that take a hash.
// It is an error to call this with a hash_id that is not a SHA*.
CONTENT_EXPORT blink::WebCryptoAlgorithm CreateRsaHashedImportAlgorithm(
    blink::WebCryptoAlgorithmId id,
    blink::WebCryptoAlgorithmId hash_id);

bool CreateSecretKeyAlgorithm(const blink::WebCryptoAlgorithm& algorithm,
                              unsigned int keylen_bytes,
                              blink::WebCryptoKeyAlgorithm* key_algorithm);

// Returns true if the set bits in b make up a subset of the set bits in a.
bool ContainsKeyUsages(blink::WebCryptoKeyUsageMask a,
                       blink::WebCryptoKeyUsageMask b);

bool IsAlgorithmRsa(blink::WebCryptoAlgorithmId alg_id);
bool IsAlgorithmAsymmetric(blink::WebCryptoAlgorithmId alg_id);

}  // namespace webcrypto

}  // namespace content

#endif  // CONTENT_CHILD_WEBCRYPTO_WEBCRYPTO_UTIL_H_
