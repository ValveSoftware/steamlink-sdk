// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBCRYPTO_STATUS_H_
#define CONTENT_CHILD_WEBCRYPTO_STATUS_H_

#include <string>
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebCrypto.h"

namespace content {

namespace webcrypto {

// Status indicates whether an operation completed successfully, or with an
// error. The error is used for verification in unit-tests, as well as for
// display to the user.
//
// As such, it is important that errors DO NOT reveal any sensitive material
// (like key bytes).
class CONTENT_EXPORT Status {
 public:
  Status() : type_(TYPE_ERROR) {}

  // Returns true if the Status represents an error (any one of them).
  bool IsError() const;

  // Returns true if the Status represent success.
  bool IsSuccess() const;

  // Returns a UTF-8 error message (non-localized) describing the error.
  const std::string& error_details() const { return error_details_; }

  blink::WebCryptoErrorType error_type() const { return error_type_; }

  // Constructs a status representing success.
  static Status Success();

  // Constructs a status representing a generic operation error. It contains no
  // extra details.
  static Status OperationError();

  // Constructs a status representing a generic data error. It contains no
  // extra details.
  static Status DataError();

  // ------------------------------------
  // Errors when importing a JWK formatted key
  // ------------------------------------

  // The key bytes could not parsed as JSON dictionary. This either
  // means there was a parsing error, or the JSON object was not
  // convertable to a dictionary.
  static Status ErrorJwkNotDictionary();

  // The required property |property| was missing.
  static Status ErrorJwkPropertyMissing(const std::string& property);

  // The property |property| was not of type |expected_type|.
  static Status ErrorJwkPropertyWrongType(const std::string& property,
                                          const std::string& expected_type);

  // The property |property| was a string, however could not be successfully
  // base64 decoded.
  static Status ErrorJwkBase64Decode(const std::string& property);

  // The "ext" parameter was specified but was
  // incompatible with the value requested by the Web Crypto call.
  static Status ErrorJwkExtInconsistent();

  // The "alg" parameter could not be converted to an equivalent
  // WebCryptoAlgorithm. Either it was malformed or unrecognized.
  static Status ErrorJwkUnrecognizedAlgorithm();

  // The "alg" parameter is incompatible with the (optional) Algorithm
  // specified by the Web Crypto import operation.
  static Status ErrorJwkAlgorithmInconsistent();

  // The "use" parameter was specified, however it couldn't be converted to an
  // equivalent Web Crypto usage.
  static Status ErrorJwkUnrecognizedUse();

  // The "key_ops" parameter was specified, however one of the values in the
  // array couldn't be converted to an equivalent Web Crypto usage.
  static Status ErrorJwkUnrecognizedKeyop();

  // The "use" parameter was specified, however it is incompatible with that
  // specified by the Web Crypto import operation.
  static Status ErrorJwkUseInconsistent();

  // The "key_ops" parameter was specified, however it is incompatible with that
  // specified by the Web Crypto import operation.
  static Status ErrorJwkKeyopsInconsistent();

  // Both the "key_ops" and the "use" parameters were specified, however they
  // are incompatible with each other.
  static Status ErrorJwkUseAndKeyopsInconsistent();

  // The "kty" parameter was given and was a string, however it was
  // unrecognized.
  static Status ErrorJwkUnrecognizedKty();

  // The amount of key data provided was incompatible with the selected
  // algorithm. For instance if the algorith name was A128CBC then EXACTLY
  // 128-bits of key data must have been provided. If 192-bits of key data were
  // given that is an error.
  static Status ErrorJwkIncorrectKeyLength();

  // The JWK was for an RSA private key but only partially provided the optional
  // parameters (p, q, dq, dq, qi).
  static Status ErrorJwkIncompleteOptionalRsaPrivateKey();

  // ------------------------------------
  // Other errors
  // ------------------------------------

  // No key data was provided when importing an spki, pkcs8, or jwk formatted
  // key. This does not apply to raw format, since it is possible to have empty
  // key data there.
  static Status ErrorImportEmptyKeyData();

  // The key data buffer provided for importKey() is an incorrect length for
  // AES.
  static Status ErrorImportAesKeyLength();

  // 192-bit AES keys are valid, however unsupported.
  static Status ErrorAes192BitUnsupported();

  // The wrong key was used for the operation. For instance, a public key was
  // used to verify a RsaSsaPkcs1v1_5 signature, or tried exporting a private
  // key using spki format.
  static Status ErrorUnexpectedKeyType();

  // When doing an AES-CBC encryption/decryption, the "iv" parameter was not 16
  // bytes.
  static Status ErrorIncorrectSizeAesCbcIv();

  // The data provided to an encrypt/decrypt/sign/verify operation was too
  // large. This can either represent an internal limitation (for instance
  // representing buffer lengths as uints).
  static Status ErrorDataTooLarge();

  // The data provided to an encrypt/decrypt/sign/verify operation was too
  // small. This usually represents an algorithm restriction (for instance
  // AES-KW requires a minimum of 24 bytes input data).
  static Status ErrorDataTooSmall();

  // Something was unsupported or unimplemented. This can mean the algorithm in
  // question was unsupported, some parameter combination was unsupported, or
  // something has not yet been implemented.
  static Status ErrorUnsupported();
  static Status ErrorUnsupported(const std::string& message);

  // Something unexpected happened in the code, which implies there is a
  // source-level bug. These should not happen, but safer to fail than simply
  // DCHECK.
  static Status ErrorUnexpected();

  // The authentication tag length specified for AES-GCM encrypt/decrypt was
  // not 32, 64, 96, 104, 112, 120, or 128.
  static Status ErrorInvalidAesGcmTagLength();

  // The input data given to an AES-KW encrypt/decrypt operation was not a
  // multiple of 8 bytes, as required by RFC 3394.
  static Status ErrorInvalidAesKwDataLength();

  // The "publicExponent" used to generate a key was invalid or unsupported.
  // Only values of 3 and 65537 are allowed.
  static Status ErrorGenerateKeyPublicExponent();

  // The modulus bytes were empty when importing an RSA public key.
  static Status ErrorImportRsaEmptyModulus();

  // The the modulus length was zero bits when generating an RSA public key.
  static Status ErrorGenerateRsaZeroModulus();

  // The exponent bytes were empty when importing an RSA public key.
  static Status ErrorImportRsaEmptyExponent();

  // An unextractable key was used by an operation which exports the key data.
  static Status ErrorKeyNotExtractable();

  // The key length specified when generating a key was invalid. Either it was
  // zero, or it was not a multiple of 8 bits.
  static Status ErrorGenerateKeyLength();

  // Attempted to create a key (either by importKey(), generateKey(), or
  // unwrapKey()) however the key usages were not applicable for the key type
  // and algorithm.
  static Status ErrorCreateKeyBadUsages();

 private:
  enum Type { TYPE_ERROR, TYPE_SUCCESS };

  // Constructs an error with the specified error type and message.
  Status(blink::WebCryptoErrorType error_type,
         const std::string& error_details_utf8);

  // Constructs a success or error without any details.
  explicit Status(Type type);

  Type type_;
  blink::WebCryptoErrorType error_type_;
  std::string error_details_;
};

}  // namespace webcrypto

}  // namespace content

#endif  // CONTENT_CHILD_WEBCRYPTO_STATUS_H_
