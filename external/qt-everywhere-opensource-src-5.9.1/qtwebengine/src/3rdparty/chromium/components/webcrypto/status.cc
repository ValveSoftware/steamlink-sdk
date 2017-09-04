// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/status.h"

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"

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

Status Status::ErrorJwkMemberMissing(const std::string& member_name) {
  return Status(blink::WebCryptoErrorTypeData,
                "The required JWK member \"" + member_name + "\" was missing");
}

Status Status::ErrorJwkMemberWrongType(const std::string& member_name,
                                       const std::string& expected_type) {
  return Status(
      blink::WebCryptoErrorTypeData,
      "The JWK member \"" + member_name + "\" must be a " + expected_type);
}

Status Status::ErrorJwkBase64Decode(const std::string& member_name) {
  return Status(blink::WebCryptoErrorTypeData,
                "The JWK member \"" + member_name +
                    "\" could not be base64url decoded or contained padding");
}

Status Status::ErrorJwkExtInconsistent() {
  return Status(
      blink::WebCryptoErrorTypeData,
      "The \"ext\" member of the JWK dictionary is inconsistent what that "
      "specified by the Web Crypto call");
}

Status Status::ErrorJwkAlgorithmInconsistent() {
  return Status(blink::WebCryptoErrorTypeData,
                "The JWK \"alg\" member was inconsistent with that specified "
                "by the Web Crypto call");
}

Status Status::ErrorJwkUnrecognizedUse() {
  return Status(blink::WebCryptoErrorTypeData,
                "The JWK \"use\" member could not be parsed");
}

Status Status::ErrorJwkUnrecognizedKeyop() {
  return Status(blink::WebCryptoErrorTypeData,
                "The JWK \"key_ops\" member could not be parsed");
}

Status Status::ErrorJwkUseInconsistent() {
  return Status(blink::WebCryptoErrorTypeData,
                "The JWK \"use\" member was inconsistent with that specified "
                "by the Web Crypto call. The JWK usage must be a superset of "
                "those requested");
}

Status Status::ErrorJwkKeyopsInconsistent() {
  return Status(blink::WebCryptoErrorTypeData,
                "The JWK \"key_ops\" member was inconsistent with that "
                "specified by the Web Crypto call. The JWK usage must be a "
                "superset of those requested");
}

Status Status::ErrorJwkUseAndKeyopsInconsistent() {
  return Status(blink::WebCryptoErrorTypeData,
                "The JWK \"use\" and \"key_ops\" properties were both found "
                "but are inconsistent with each other.");
}

Status Status::ErrorJwkUnexpectedKty(const std::string& expected) {
  return Status(blink::WebCryptoErrorTypeData,
                "The JWK \"kty\" member was not \"" + expected + "\"");
}

Status Status::ErrorJwkIncorrectKeyLength() {
  return Status(blink::WebCryptoErrorTypeData,
                "The JWK \"k\" member did not include the right length "
                "of key data for the given algorithm.");
}

Status Status::ErrorJwkEmptyBigInteger(const std::string& member_name) {
  return Status(blink::WebCryptoErrorTypeData,
                "The JWK \"" + member_name + "\" member was empty.");
}

Status Status::ErrorJwkBigIntegerHasLeadingZero(
    const std::string& member_name) {
  return Status(
      blink::WebCryptoErrorTypeData,
      "The JWK \"" + member_name + "\" member contained a leading zero.");
}

Status Status::ErrorJwkDuplicateKeyOps() {
  return Status(blink::WebCryptoErrorTypeData,
                "The \"key_ops\" member of the JWK dictionary contains "
                "duplicate usages.");
}

Status Status::ErrorUnsupportedImportKeyFormat() {
  return Status(blink::WebCryptoErrorTypeNotSupported,
                "Unsupported import key format for algorithm");
}

Status Status::ErrorUnsupportedExportKeyFormat() {
  return Status(blink::WebCryptoErrorTypeNotSupported,
                "Unsupported export key format for algorithm");
}

Status Status::ErrorImportAesKeyLength() {
  return Status(blink::WebCryptoErrorTypeData,
                "AES key data must be 128 or 256 bits");
}

Status Status::ErrorGetAesKeyLength() {
  return Status(blink::WebCryptoErrorTypeOperation,
                "AES key length must be 128 or 256 bits");
}

Status Status::ErrorGenerateAesKeyLength() {
  return Status(blink::WebCryptoErrorTypeOperation,
                "AES key length must be 128 or 256 bits");
}

Status Status::ErrorAes192BitUnsupported() {
  return Status(blink::WebCryptoErrorTypeOperation,
                "192-bit AES keys are not supported");
}

Status Status::ErrorUnexpectedKeyType() {
  return Status(blink::WebCryptoErrorTypeInvalidAccess,
                "The key is not of the expected type");
}

Status Status::ErrorIncorrectSizeAesCbcIv() {
  return Status(blink::WebCryptoErrorTypeOperation,
                "The \"iv\" has an unexpected length -- must be 16 bytes");
}

Status Status::ErrorIncorrectSizeAesCtrCounter() {
  return Status(blink::WebCryptoErrorTypeOperation,
                "The \"counter\" has an unexpected length -- must be 16 bytes");
}

Status Status::ErrorInvalidAesCtrCounterLength() {
  return Status(blink::WebCryptoErrorTypeOperation,
                "The \"length\" member must be >= 1 and <= 128");
}

Status Status::ErrorAesCtrInputTooLongCounterRepeated() {
  return Status(blink::WebCryptoErrorTypeData,
                "The input is too large for the counter length.");
}

Status Status::ErrorDataTooLarge() {
  return Status(blink::WebCryptoErrorTypeOperation,
                "The provided data is too large");
}

Status Status::ErrorDataTooSmall() {
  return Status(blink::WebCryptoErrorTypeOperation,
                "The provided data is too small");
}

Status Status::ErrorUnsupported() {
  return ErrorUnsupported("The requested operation is unsupported");
}

Status Status::ErrorUnsupported(const std::string& message) {
  return Status(blink::WebCryptoErrorTypeNotSupported, message);
}

Status Status::ErrorUnexpected() {
  return Status(blink::WebCryptoErrorTypeOperation,
                "Something unexpected happened...");
}

Status Status::ErrorInvalidAesGcmTagLength() {
  return Status(
      blink::WebCryptoErrorTypeOperation,
      "The tag length is invalid: Must be 32, 64, 96, 104, 112, 120, or 128 "
      "bits");
}

Status Status::ErrorInvalidAesKwDataLength() {
  return Status(blink::WebCryptoErrorTypeData,
                "The AES-KW input data length is invalid: not a multiple of 8 "
                "bytes");
}

Status Status::ErrorGenerateKeyPublicExponent() {
  return Status(blink::WebCryptoErrorTypeOperation,
                "The \"publicExponent\" must be either 3 or 65537");
}

Status Status::ErrorImportRsaEmptyModulus() {
  return Status(blink::WebCryptoErrorTypeData, "The modulus is empty");
}

Status Status::ErrorGenerateRsaUnsupportedModulus() {
  return Status(blink::WebCryptoErrorTypeOperation,
                "The modulus length must be a multiple of 8 bits and >= 256 "
                "and <= 16384");
}

Status Status::ErrorImportRsaEmptyExponent() {
  return Status(blink::WebCryptoErrorTypeData,
                "No bytes for the exponent were provided");
}

Status Status::ErrorKeyNotExtractable() {
  return Status(blink::WebCryptoErrorTypeInvalidAccess,
                "They key is not extractable");
}

Status Status::ErrorGenerateHmacKeyLengthZero() {
  return Status(blink::WebCryptoErrorTypeOperation,
                "HMAC key length must not be zero");
}

Status Status::ErrorHmacImportEmptyKey() {
  return Status(blink::WebCryptoErrorTypeData,
                "HMAC key data must not be empty");
}

Status Status::ErrorGetHmacKeyLengthZero() {
  return Status(blink::WebCryptoErrorTypeType,
                "HMAC key length must not be zero");
}

Status Status::ErrorHmacImportBadLength() {
  return Status(
      blink::WebCryptoErrorTypeData,
      "The optional HMAC key length must be shorter than the key data, and by "
      "no more than 7 bits.");
}

Status Status::ErrorCreateKeyBadUsages() {
  return Status(blink::WebCryptoErrorTypeSyntax,
                "Cannot create a key using the specified key usages.");
}

Status Status::ErrorCreateKeyEmptyUsages() {
  return Status(blink::WebCryptoErrorTypeSyntax,
                "Usages cannot be empty when creating a key.");
}

Status Status::ErrorImportedEcKeyIncorrectCurve() {
  return Status(
      blink::WebCryptoErrorTypeData,
      "The imported EC key specifies a different curve than requested");
}

Status Status::ErrorJwkIncorrectCrv() {
  return Status(
      blink::WebCryptoErrorTypeData,
      "The JWK's \"crv\" member specifies a different curve than requested");
}

Status Status::ErrorEcKeyInvalid() {
  return Status(blink::WebCryptoErrorTypeData,
                "The imported EC key is invalid");
}

Status Status::JwkOctetStringWrongLength(const std::string& member_name,
                                         size_t expected_length,
                                         size_t actual_length) {
  return Status(
      blink::WebCryptoErrorTypeData,
      base::StringPrintf(
          "The JWK's \"%s\" member defines an octet string of length %" PRIuS
          " bytes but should be %" PRIuS,
          member_name.c_str(), actual_length, expected_length));
}

Status Status::ErrorEcdhPublicKeyWrongType() {
  return Status(
      blink::WebCryptoErrorTypeInvalidAccess,
      "The public parameter for ECDH key derivation is not a public EC key");
}

Status Status::ErrorEcdhPublicKeyWrongAlgorithm() {
  return Status(
      blink::WebCryptoErrorTypeInvalidAccess,
      "The public parameter for ECDH key derivation must be for ECDH");
}

Status Status::ErrorEcdhCurveMismatch() {
  return Status(blink::WebCryptoErrorTypeInvalidAccess,
                "The public parameter for ECDH key derivation is for a "
                "different named curve");
}

Status Status::ErrorEcdhLengthTooBig(unsigned int max_length_bits) {
  return Status(blink::WebCryptoErrorTypeOperation,
                base::StringPrintf(
                    "Length specified for ECDH key derivation is too large. "
                    "Maximum allowed is %u bits",
                    max_length_bits));
}

Status Status::ErrorHkdfLengthTooLong() {
  return Status(blink::WebCryptoErrorTypeOperation,
                "The length provided for HKDF is too large.");
}

Status Status::ErrorHkdfDeriveBitsLengthNotSpecified() {
  // TODO(nharper): The spec might change so that an OperationError should be
  // thrown here instead of a TypeError.
  // (https://www.w3.org/Bugs/Public/show_bug.cgi?id=27771)
  return Status(blink::WebCryptoErrorTypeType,
                "No length was specified for the HKDF Derive Bits operation.");
}

Status Status::ErrorPbkdf2InvalidLength() {
  return Status(
      blink::WebCryptoErrorTypeOperation,
      "Length for PBKDF2 key derivation must be a multiple of 8 bits.");
}

Status Status::ErrorPbkdf2DeriveBitsLengthNotSpecified() {
  return Status(
      blink::WebCryptoErrorTypeOperation,
      "No length was specified for the PBKDF2 Derive Bits operation.");
}

Status Status::ErrorPbkdf2DeriveBitsLengthZero() {
  return Status(
      blink::WebCryptoErrorTypeOperation,
      "A length of 0 was specified for PBKDF2's Derive Bits operation.");
}

Status Status::ErrorPbkdf2Iterations0() {
  return Status(blink::WebCryptoErrorTypeOperation,
                "PBKDF2 requires iterations > 0");
}

Status Status::ErrorImportExtractableKdfKey() {
  return Status(blink::WebCryptoErrorTypeSyntax,
                "KDF keys must set extractable=false");
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
