// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/status.h"

namespace content {

namespace webcrypto {

bool Status::IsError() const {
  return type_ == TYPE_ERROR;
}

bool Status::IsSuccess() const {
  return type_ == TYPE_SUCCESS;
}

Status Status::Success() {
  return Status(TYPE_SUCCESS);
}

Status Status::OperationError() {
  return Status(blink::WebCryptoErrorTypeOperation, "");
}

Status Status::DataError() {
  return Status(blink::WebCryptoErrorTypeData, "");
}

Status Status::ErrorJwkNotDictionary() {
  return Status(blink::WebCryptoErrorTypeData,
                "JWK input could not be parsed to a JSON dictionary");
}

Status Status::ErrorJwkPropertyMissing(const std::string& property) {
  return Status(blink::WebCryptoErrorTypeData,
                "The required JWK property \"" + property + "\" was missing");
}

Status Status::ErrorJwkPropertyWrongType(const std::string& property,
                                         const std::string& expected_type) {
  return Status(
      blink::WebCryptoErrorTypeData,
      "The JWK property \"" + property + "\" must be a " + expected_type);
}

Status Status::ErrorJwkBase64Decode(const std::string& property) {
  return Status(
      blink::WebCryptoErrorTypeData,
      "The JWK property \"" + property + "\" could not be base64 decoded");
}

Status Status::ErrorJwkExtInconsistent() {
  return Status(
      blink::WebCryptoErrorTypeData,
      "The \"ext\" property of the JWK dictionary is inconsistent what that "
      "specified by the Web Crypto call");
}

Status Status::ErrorJwkUnrecognizedAlgorithm() {
  return Status(blink::WebCryptoErrorTypeData,
                "The JWK \"alg\" property was not recognized");
}

Status Status::ErrorJwkAlgorithmInconsistent() {
  return Status(blink::WebCryptoErrorTypeData,
                "The JWK \"alg\" property was inconsistent with that specified "
                "by the Web Crypto call");
}

Status Status::ErrorJwkUnrecognizedUse() {
  return Status(blink::WebCryptoErrorTypeData,
                "The JWK \"use\" property could not be parsed");
}

Status Status::ErrorJwkUnrecognizedKeyop() {
  return Status(blink::WebCryptoErrorTypeData,
                "The JWK \"key_ops\" property could not be parsed");
}

Status Status::ErrorJwkUseInconsistent() {
  return Status(blink::WebCryptoErrorTypeData,
                "The JWK \"use\" property was inconsistent with that specified "
                "by the Web Crypto call. The JWK usage must be a superset of "
                "those requested");
}

Status Status::ErrorJwkKeyopsInconsistent() {
  return Status(blink::WebCryptoErrorTypeData,
                "The JWK \"key_ops\" property was inconsistent with that "
                "specified by the Web Crypto call. The JWK usage must be a "
                "superset of those requested");
}

Status Status::ErrorJwkUseAndKeyopsInconsistent() {
  return Status(blink::WebCryptoErrorTypeData,
                "The JWK \"use\" and \"key_ops\" properties were both found "
                "but are inconsistent with each other.");
}

Status Status::ErrorJwkUnrecognizedKty() {
  return Status(blink::WebCryptoErrorTypeData,
                "The JWK \"kty\" property was unrecognized");
}

Status Status::ErrorJwkIncorrectKeyLength() {
  return Status(blink::WebCryptoErrorTypeData,
                "The JWK \"k\" property did not include the right length "
                "of key data for the given algorithm.");
}

Status Status::ErrorJwkIncompleteOptionalRsaPrivateKey() {
  return Status(blink::WebCryptoErrorTypeData,
                "The optional JWK properties p, q, dp, dq, qi must either all "
                "be provided, or none provided");
}

Status Status::ErrorImportEmptyKeyData() {
  return Status(blink::WebCryptoErrorTypeData, "No key data was provided");
}

Status Status::ErrorImportAesKeyLength() {
  return Status(blink::WebCryptoErrorTypeData,
                "AES key data must be 128, 192 or 256 bits");
}

Status Status::ErrorAes192BitUnsupported() {
  return Status(blink::WebCryptoErrorTypeNotSupported,
                "192-bit AES keys are not supported");
}

Status Status::ErrorUnexpectedKeyType() {
  return Status(blink::WebCryptoErrorTypeInvalidAccess,
                "The key is not of the expected type");
}

Status Status::ErrorIncorrectSizeAesCbcIv() {
  return Status(blink::WebCryptoErrorTypeData,
                "The \"iv\" has an unexpected length -- must be 16 bytes");
}

Status Status::ErrorDataTooLarge() {
  return Status(blink::WebCryptoErrorTypeData,
                "The provided data is too large");
}

Status Status::ErrorDataTooSmall() {
  return Status(blink::WebCryptoErrorTypeData,
                "The provided data is too small");
}

Status Status::ErrorUnsupported() {
  return ErrorUnsupported("The requested operation is unsupported");
}

Status Status::ErrorUnsupported(const std::string& message) {
  return Status(blink::WebCryptoErrorTypeNotSupported, message);
}

Status Status::ErrorUnexpected() {
  return Status(blink::WebCryptoErrorTypeUnknown,
                "Something unexpected happened...");
}

Status Status::ErrorInvalidAesGcmTagLength() {
  return Status(
      blink::WebCryptoErrorTypeData,
      "The tag length is invalid: Must be 32, 64, 96, 104, 112, 120, or 128 "
      "bits");
}

Status Status::ErrorInvalidAesKwDataLength() {
  return Status(blink::WebCryptoErrorTypeData,
                "The AES-KW input data length is invalid: not a multiple of 8 "
                "bytes");
}

Status Status::ErrorGenerateKeyPublicExponent() {
  return Status(blink::WebCryptoErrorTypeData,
                "The \"publicExponent\" must be either 3 or 65537");
}

Status Status::ErrorImportRsaEmptyModulus() {
  return Status(blink::WebCryptoErrorTypeData, "The modulus is empty");
}

Status Status::ErrorGenerateRsaZeroModulus() {
  return Status(blink::WebCryptoErrorTypeData,
                "The modulus bit length cannot be zero");
}

Status Status::ErrorImportRsaEmptyExponent() {
  return Status(blink::WebCryptoErrorTypeData,
                "No bytes for the exponent were provided");
}

Status Status::ErrorKeyNotExtractable() {
  return Status(blink::WebCryptoErrorTypeInvalidAccess,
                "They key is not extractable");
}

Status Status::ErrorGenerateKeyLength() {
  return Status(blink::WebCryptoErrorTypeData,
                "Invalid key length: it is either zero or not a multiple of 8 "
                "bits");
}

Status Status::ErrorCreateKeyBadUsages() {
  return Status(blink::WebCryptoErrorTypeData,
                "Cannot create a key using the specified key usages.");
}

Status::Status(blink::WebCryptoErrorType error_type,
               const std::string& error_details_utf8)
    : type_(TYPE_ERROR),
      error_type_(error_type),
      error_details_(error_details_utf8) {
}

Status::Status(Type type) : type_(type) {
}

}  // namespace webcrypto

}  // namespace content
