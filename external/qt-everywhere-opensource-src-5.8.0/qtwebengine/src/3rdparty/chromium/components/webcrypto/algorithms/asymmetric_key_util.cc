// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/algorithms/asymmetric_key_util.h"

#include <openssl/bytestring.h>
#include <openssl/evp.h>
#include <stdint.h>
#include <utility>

#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/generate_key_result.h"
#include "components/webcrypto/status.h"
#include "crypto/auto_cbb.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_openssl_types.h"

namespace webcrypto {

namespace {

// Exports an EVP_PKEY public key to the SPKI format.
Status ExportPKeySpki(EVP_PKEY* key, std::vector<uint8_t>* buffer) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  uint8_t* der;
  size_t der_len;
  crypto::AutoCBB cbb;
  if (!CBB_init(cbb.get(), 0) || !EVP_marshal_public_key(cbb.get(), key) ||
      !CBB_finish(cbb.get(), &der, &der_len)) {
    return Status::ErrorUnexpected();
  }
  buffer->assign(der, der + der_len);
  OPENSSL_free(der);
  return Status::Success();
}

// Exports an EVP_PKEY private key to the PKCS8 format.
Status ExportPKeyPkcs8(EVP_PKEY* key, std::vector<uint8_t>* buffer) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  // TODO(eroman): Use the OID specified by webcrypto spec.
  //               http://crbug.com/373545
  uint8_t* der;
  size_t der_len;
  crypto::AutoCBB cbb;
  if (!CBB_init(cbb.get(), 0) || !EVP_marshal_private_key(cbb.get(), key) ||
      !CBB_finish(cbb.get(), &der, &der_len)) {
    return Status::ErrorUnexpected();
  }
  buffer->assign(der, der + der_len);
  OPENSSL_free(der);
  return Status::Success();
}

}  // namespace

Status CreateWebCryptoPublicKey(crypto::ScopedEVP_PKEY public_key,
                                const blink::WebCryptoKeyAlgorithm& algorithm,
                                bool extractable,
                                blink::WebCryptoKeyUsageMask usages,
                                blink::WebCryptoKey* key) {
  // Serialize the key at creation time so that if structured cloning is
  // requested it can be done synchronously from the Blink thread.
  std::vector<uint8_t> spki_data;
  Status status = ExportPKeySpki(public_key.get(), &spki_data);
  if (status.IsError())
    return status;

  *key = blink::WebCryptoKey::create(
      CreateAsymmetricKeyHandle(std::move(public_key), spki_data),
      blink::WebCryptoKeyTypePublic, extractable, algorithm, usages);
  return Status::Success();
}

Status CreateWebCryptoPrivateKey(crypto::ScopedEVP_PKEY private_key,
                                 const blink::WebCryptoKeyAlgorithm& algorithm,
                                 bool extractable,
                                 blink::WebCryptoKeyUsageMask usages,
                                 blink::WebCryptoKey* key) {
  // Serialize the key at creation time so that if structured cloning is
  // requested it can be done synchronously from the Blink thread.
  std::vector<uint8_t> pkcs8_data;
  Status status = ExportPKeyPkcs8(private_key.get(), &pkcs8_data);
  if (status.IsError())
    return status;

  *key = blink::WebCryptoKey::create(
      CreateAsymmetricKeyHandle(std::move(private_key), pkcs8_data),
      blink::WebCryptoKeyTypePrivate, extractable, algorithm, usages);
  return Status::Success();
}

Status CheckPrivateKeyCreationUsages(
    blink::WebCryptoKeyUsageMask all_possible_usages,
    blink::WebCryptoKeyUsageMask actual_usages) {
  return CheckKeyCreationUsages(all_possible_usages, actual_usages,
                                EmptyUsagePolicy::REJECT_EMPTY);
}

Status CheckPublicKeyCreationUsages(
    blink::WebCryptoKeyUsageMask all_possible_usages,
    blink::WebCryptoKeyUsageMask actual_usages) {
  return CheckKeyCreationUsages(all_possible_usages, actual_usages,
                                EmptyUsagePolicy::ALLOW_EMPTY);
}

Status ImportUnverifiedPkeyFromSpki(const CryptoData& key_data,
                                    int expected_pkey_id,
                                    crypto::ScopedEVP_PKEY* out_pkey) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  CBS cbs;
  CBS_init(&cbs, key_data.bytes(), key_data.byte_length());
  crypto::ScopedEVP_PKEY pkey(EVP_parse_public_key(&cbs));
  if (!pkey || CBS_len(&cbs) != 0)
    return Status::DataError();

  if (EVP_PKEY_id(pkey.get()) != expected_pkey_id)
    return Status::DataError();  // Data did not define expected key type.

  *out_pkey = std::move(pkey);
  return Status::Success();
}

Status ImportUnverifiedPkeyFromPkcs8(const CryptoData& key_data,
                                     int expected_pkey_id,
                                     crypto::ScopedEVP_PKEY* out_pkey) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  CBS cbs;
  CBS_init(&cbs, key_data.bytes(), key_data.byte_length());
  crypto::ScopedEVP_PKEY pkey(EVP_parse_private_key(&cbs));
  if (!pkey || CBS_len(&cbs) != 0)
    return Status::DataError();

  if (EVP_PKEY_id(pkey.get()) != expected_pkey_id)
    return Status::DataError();  // Data did not define expected key type.

  *out_pkey = std::move(pkey);
  return Status::Success();
}

Status VerifyUsagesBeforeImportAsymmetricKey(
    blink::WebCryptoKeyFormat format,
    blink::WebCryptoKeyUsageMask all_public_key_usages,
    blink::WebCryptoKeyUsageMask all_private_key_usages,
    blink::WebCryptoKeyUsageMask usages) {
  switch (format) {
    case blink::WebCryptoKeyFormatSpki:
      return CheckPublicKeyCreationUsages(all_public_key_usages, usages);
    case blink::WebCryptoKeyFormatPkcs8:
      return CheckPrivateKeyCreationUsages(all_private_key_usages, usages);
    case blink::WebCryptoKeyFormatJwk: {
      // The JWK could represent either a public key or private key. The usages
      // must make sense for one of the two. The usages will be checked again by
      // ImportKeyJwk() once the key type has been determined.
      if (CheckPublicKeyCreationUsages(all_public_key_usages, usages)
              .IsError() &&
          CheckPrivateKeyCreationUsages(all_private_key_usages, usages)
              .IsError()) {
        return Status::ErrorCreateKeyBadUsages();
      }
      return Status::Success();
    }
    default:
      return Status::ErrorUnsupportedImportKeyFormat();
  }
}

Status GetUsagesForGenerateAsymmetricKey(
    blink::WebCryptoKeyUsageMask combined_usages,
    blink::WebCryptoKeyUsageMask all_public_usages,
    blink::WebCryptoKeyUsageMask all_private_usages,
    blink::WebCryptoKeyUsageMask* public_usages,
    blink::WebCryptoKeyUsageMask* private_usages) {
  // Ensure that the combined usages is a subset of the total possible usages.
  Status status =
      CheckKeyCreationUsages(all_public_usages | all_private_usages,
                             combined_usages, EmptyUsagePolicy::ALLOW_EMPTY);
  if (status.IsError())
    return status;

  *public_usages = combined_usages & all_public_usages;
  *private_usages = combined_usages & all_private_usages;

  // Ensure that the private key has non-empty usages.
  return CheckPrivateKeyCreationUsages(all_private_usages, *private_usages);
}

}  // namespace webcrypto
