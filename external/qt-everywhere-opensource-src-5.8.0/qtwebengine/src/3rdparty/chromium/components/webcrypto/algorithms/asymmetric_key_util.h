// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_ALGORITHMS_ASYMMETRIC_KEY_UTIL_
#define COMPONENTS_WEBCRYPTO_ALGORITHMS_ASYMMETRIC_KEY_UTIL_

#include "crypto/scoped_openssl_types.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoKey.h"

// This file contains functions shared by multiple asymmetric key algorithms.

namespace webcrypto {

class CryptoData;
class Status;

// Creates a WebCrypto public key given an EVP_PKEY. This step includes
// exporting the key to SPKI format, for use by serialization later.
Status CreateWebCryptoPublicKey(crypto::ScopedEVP_PKEY public_key,
                                const blink::WebCryptoKeyAlgorithm& algorithm,
                                bool extractable,
                                blink::WebCryptoKeyUsageMask usages,
                                blink::WebCryptoKey* key);

// Creates a WebCrypto private key given an EVP_PKEY. This step includes
// exporting the key to PKCS8 format, for use by serialization later.
Status CreateWebCryptoPrivateKey(crypto::ScopedEVP_PKEY private_key,
                                 const blink::WebCryptoKeyAlgorithm& algorithm,
                                 bool extractable,
                                 blink::WebCryptoKeyUsageMask usages,
                                 blink::WebCryptoKey* key);

// Checks that a private key can be created using |actual_usages|, where
// |all_possible_usages| is the full set of allowed private key usages. Note
// that private keys are not allowed to have empty usages.
Status CheckPrivateKeyCreationUsages(
    blink::WebCryptoKeyUsageMask all_possible_usages,
    blink::WebCryptoKeyUsageMask actual_usages);

// Checks that a public key can be created using |actual_usages|, where
// |all_possible_usages| is the full set of allowed public key usages
Status CheckPublicKeyCreationUsages(
    blink::WebCryptoKeyUsageMask all_possible_usages,
    blink::WebCryptoKeyUsageMask actual_usages);

// Imports SPKI bytes to an EVP_PKEY for a public key. The resulting asymmetric
// key may be invalid, and should be verified using something like
// RSA_check_key(). The only validation performed by this function is to ensure
// the key type matched |expected_pkey_id|.
Status ImportUnverifiedPkeyFromSpki(const CryptoData& key_data,
                                    int expected_pkey_id,
                                    crypto::ScopedEVP_PKEY* pkey);

// Imports PKCS8 bytes to an EVP_PKEY for a private key. The resulting
// asymmetric key may be invalid, and should be verified using something like
// RSA_check_key(). The only validation performed by this function is to ensure
// the key type matched |expected_pkey_id|.
Status ImportUnverifiedPkeyFromPkcs8(const CryptoData& key_data,
                                     int expected_pkey_id,
                                     crypto::ScopedEVP_PKEY* pkey);

// Verifies that |usages| is valid when importing a key of the given format.
Status VerifyUsagesBeforeImportAsymmetricKey(
    blink::WebCryptoKeyFormat format,
    blink::WebCryptoKeyUsageMask all_public_key_usages,
    blink::WebCryptoKeyUsageMask all_private_key_usages,
    blink::WebCryptoKeyUsageMask usages);

// Splits the combined usages given to GenerateKey() into the respective usages
// for the public key and private key. Returns an error if the usages are
// invalid.
Status GetUsagesForGenerateAsymmetricKey(
    blink::WebCryptoKeyUsageMask combined_usages,
    blink::WebCryptoKeyUsageMask all_public_usages,
    blink::WebCryptoKeyUsageMask all_private_usages,
    blink::WebCryptoKeyUsageMask* public_usages,
    blink::WebCryptoKeyUsageMask* private_usages);

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_ALGORITHMS_ASYMMETRIC_KEY_UTIL_
