// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/shared_crypto.h"

#include "base/logging.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/jwk.h"
#include "content/child/webcrypto/platform_crypto.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "crypto/secure_util.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKey.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace content {

namespace webcrypto {

// ------------
// Threading:
// ------------
//
// All functions in this file are called from the webcrypto worker pool except
// for:
//
//   * SerializeKeyForClone()
//   * DeserializeKeyForClone()
//   * ImportKey() // TODO(eroman): Change this.

namespace {

// TODO(eroman): Move this helper to WebCryptoKey.
bool KeyUsageAllows(const blink::WebCryptoKey& key,
                    const blink::WebCryptoKeyUsage usage) {
  return ((key.usages() & usage) != 0);
}

bool IsValidAesKeyLengthBits(unsigned int length_bits) {
  // 192-bit AES is disallowed.
  return length_bits == 128 || length_bits == 256;
}

bool IsValidAesKeyLengthBytes(unsigned int length_bytes) {
  // 192-bit AES is disallowed.
  return length_bytes == 16 || length_bytes == 32;
}

const size_t kAesBlockSizeBytes = 16;

Status EncryptDecryptAesCbc(EncryptOrDecrypt mode,
                            const blink::WebCryptoAlgorithm& algorithm,
                            const blink::WebCryptoKey& key,
                            const CryptoData& data,
                            std::vector<uint8>* buffer) {
  platform::SymKey* sym_key;
  Status status = ToPlatformSymKey(key, &sym_key);
  if (status.IsError())
    return status;

  const blink::WebCryptoAesCbcParams* params = algorithm.aesCbcParams();
  if (!params)
    return Status::ErrorUnexpected();

  CryptoData iv(params->iv().data(), params->iv().size());
  if (iv.byte_length() != kAesBlockSizeBytes)
    return Status::ErrorIncorrectSizeAesCbcIv();

  return platform::EncryptDecryptAesCbc(mode, sym_key, data, iv, buffer);
}

Status EncryptDecryptAesGcm(EncryptOrDecrypt mode,
                            const blink::WebCryptoAlgorithm& algorithm,
                            const blink::WebCryptoKey& key,
                            const CryptoData& data,
                            std::vector<uint8>* buffer) {
  platform::SymKey* sym_key;
  Status status = ToPlatformSymKey(key, &sym_key);
  if (status.IsError())
    return status;

  const blink::WebCryptoAesGcmParams* params = algorithm.aesGcmParams();
  if (!params)
    return Status::ErrorUnexpected();

  unsigned int tag_length_bits = 128;
  if (params->hasTagLengthBits())
    tag_length_bits = params->optionalTagLengthBits();

  if (tag_length_bits != 32 && tag_length_bits != 64 && tag_length_bits != 96 &&
      tag_length_bits != 104 && tag_length_bits != 112 &&
      tag_length_bits != 120 && tag_length_bits != 128)
    return Status::ErrorInvalidAesGcmTagLength();

  return platform::EncryptDecryptAesGcm(
      mode,
      sym_key,
      data,
      CryptoData(params->iv()),
      CryptoData(params->optionalAdditionalData()),
      tag_length_bits,
      buffer);
}

Status EncryptRsaOaep(const blink::WebCryptoAlgorithm& algorithm,
                      const blink::WebCryptoKey& key,
                      const CryptoData& data,
                      std::vector<uint8>* buffer) {
  platform::PublicKey* public_key;
  Status status = ToPlatformPublicKey(key, &public_key);
  if (status.IsError())
    return status;

  const blink::WebCryptoRsaOaepParams* params = algorithm.rsaOaepParams();
  if (!params)
    return Status::ErrorUnexpected();

  return platform::EncryptRsaOaep(public_key,
                                  key.algorithm().rsaHashedParams()->hash(),
                                  CryptoData(params->optionalLabel()),
                                  data,
                                  buffer);
}

Status DecryptRsaOaep(const blink::WebCryptoAlgorithm& algorithm,
                      const blink::WebCryptoKey& key,
                      const CryptoData& data,
                      std::vector<uint8>* buffer) {
  platform::PrivateKey* private_key;
  Status status = ToPlatformPrivateKey(key, &private_key);
  if (status.IsError())
    return status;

  const blink::WebCryptoRsaOaepParams* params = algorithm.rsaOaepParams();
  if (!params)
    return Status::ErrorUnexpected();

  return platform::DecryptRsaOaep(private_key,
                                  key.algorithm().rsaHashedParams()->hash(),
                                  CryptoData(params->optionalLabel()),
                                  data,
                                  buffer);
}

Status SignHmac(const blink::WebCryptoAlgorithm& algorithm,
                const blink::WebCryptoKey& key,
                const CryptoData& data,
                std::vector<uint8>* buffer) {
  platform::SymKey* sym_key;
  Status status = ToPlatformSymKey(key, &sym_key);
  if (status.IsError())
    return status;

  return platform::SignHmac(
      sym_key, key.algorithm().hmacParams()->hash(), data, buffer);
}

Status VerifyHmac(const blink::WebCryptoAlgorithm& algorithm,
                  const blink::WebCryptoKey& key,
                  const CryptoData& signature,
                  const CryptoData& data,
                  bool* signature_match) {
  std::vector<uint8> result;
  Status status = SignHmac(algorithm, key, data, &result);
  if (status.IsError())
    return status;

  // Do not allow verification of truncated MACs.
  *signature_match =
      result.size() == signature.byte_length() &&
      crypto::SecureMemEqual(
          Uint8VectorStart(result), signature.bytes(), signature.byte_length());

  return Status::Success();
}

Status SignRsaSsaPkcs1v1_5(const blink::WebCryptoAlgorithm& algorithm,
                           const blink::WebCryptoKey& key,
                           const CryptoData& data,
                           std::vector<uint8>* buffer) {
  platform::PrivateKey* private_key;
  Status status = ToPlatformPrivateKey(key, &private_key);
  if (status.IsError())
    return status;

  return platform::SignRsaSsaPkcs1v1_5(
      private_key, key.algorithm().rsaHashedParams()->hash(), data, buffer);
}

Status VerifyRsaSsaPkcs1v1_5(const blink::WebCryptoAlgorithm& algorithm,
                             const blink::WebCryptoKey& key,
                             const CryptoData& signature,
                             const CryptoData& data,
                             bool* signature_match) {
  platform::PublicKey* public_key;
  Status status = ToPlatformPublicKey(key, &public_key);
  if (status.IsError())
    return status;

  return platform::VerifyRsaSsaPkcs1v1_5(
      public_key,
      key.algorithm().rsaHashedParams()->hash(),
      signature,
      data,
      signature_match);
}

// Note that this function may be called from the target Blink thread.
Status ImportKeyRaw(const CryptoData& key_data,
                    const blink::WebCryptoAlgorithm& algorithm,
                    bool extractable,
                    blink::WebCryptoKeyUsageMask usage_mask,
                    blink::WebCryptoKey* key) {
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdAesCtr:
    case blink::WebCryptoAlgorithmIdAesCbc:
    case blink::WebCryptoAlgorithmIdAesGcm:
    case blink::WebCryptoAlgorithmIdAesKw:
      if (!IsValidAesKeyLengthBytes(key_data.byte_length())) {
        return key_data.byte_length() == 24
                   ? Status::ErrorAes192BitUnsupported()
                   : Status::ErrorImportAesKeyLength();
      }
    // Fallthrough intentional!
    case blink::WebCryptoAlgorithmIdHmac:
      return platform::ImportKeyRaw(
          algorithm, key_data, extractable, usage_mask, key);
    default:
      return Status::ErrorUnsupported();
  }
}

// Returns the key format to use for structured cloning.
blink::WebCryptoKeyFormat GetCloneFormatForKeyType(
    blink::WebCryptoKeyType type) {
  switch (type) {
    case blink::WebCryptoKeyTypeSecret:
      return blink::WebCryptoKeyFormatRaw;
    case blink::WebCryptoKeyTypePublic:
      return blink::WebCryptoKeyFormatSpki;
    case blink::WebCryptoKeyTypePrivate:
      return blink::WebCryptoKeyFormatPkcs8;
  }

  NOTREACHED();
  return blink::WebCryptoKeyFormatRaw;
}

// Converts a KeyAlgorithm into an equivalent Algorithm for import.
blink::WebCryptoAlgorithm KeyAlgorithmToImportAlgorithm(
    const blink::WebCryptoKeyAlgorithm& algorithm) {
  switch (algorithm.paramsType()) {
    case blink::WebCryptoKeyAlgorithmParamsTypeAes:
      return CreateAlgorithm(algorithm.id());
    case blink::WebCryptoKeyAlgorithmParamsTypeHmac:
      return CreateHmacImportAlgorithm(algorithm.hmacParams()->hash().id());
    case blink::WebCryptoKeyAlgorithmParamsTypeRsaHashed:
      return CreateRsaHashedImportAlgorithm(
          algorithm.id(), algorithm.rsaHashedParams()->hash().id());
    case blink::WebCryptoKeyAlgorithmParamsTypeNone:
      break;
    default:
      break;
  }
  return blink::WebCryptoAlgorithm::createNull();
}

// There is some duplicated information in the serialized format used by
// structured clone (since the KeyAlgorithm is serialized separately from the
// key data). Use this extra information to further validate what was
// deserialized from the key data.
//
// A failure here implies either a bug in the code, or that the serialized data
// was corrupted.
bool ValidateDeserializedKey(const blink::WebCryptoKey& key,
                             const blink::WebCryptoKeyAlgorithm& algorithm,
                             blink::WebCryptoKeyType type) {
  if (algorithm.id() != key.algorithm().id())
    return false;

  if (key.type() != type)
    return false;

  switch (algorithm.paramsType()) {
    case blink::WebCryptoKeyAlgorithmParamsTypeAes:
      if (algorithm.aesParams()->lengthBits() !=
          key.algorithm().aesParams()->lengthBits())
        return false;
      break;
    case blink::WebCryptoKeyAlgorithmParamsTypeRsaHashed:
      if (algorithm.rsaHashedParams()->modulusLengthBits() !=
          key.algorithm().rsaHashedParams()->modulusLengthBits())
        return false;
      if (algorithm.rsaHashedParams()->publicExponent().size() !=
          key.algorithm().rsaHashedParams()->publicExponent().size())
        return false;
      if (memcmp(algorithm.rsaHashedParams()->publicExponent().data(),
                 key.algorithm().rsaHashedParams()->publicExponent().data(),
                 key.algorithm().rsaHashedParams()->publicExponent().size()) !=
          0)
        return false;
      break;
    case blink::WebCryptoKeyAlgorithmParamsTypeNone:
    case blink::WebCryptoKeyAlgorithmParamsTypeHmac:
      break;
    default:
      return false;
  }

  return true;
}

Status EncryptDecryptAesKw(EncryptOrDecrypt mode,
                           const blink::WebCryptoAlgorithm& algorithm,
                           const blink::WebCryptoKey& key,
                           const CryptoData& data,
                           std::vector<uint8>* buffer) {
  platform::SymKey* sym_key;
  Status status = ToPlatformSymKey(key, &sym_key);
  if (status.IsError())
    return status;

  unsigned int min_length = mode == ENCRYPT ? 16 : 24;

  if (data.byte_length() < min_length)
    return Status::ErrorDataTooSmall();
  if (data.byte_length() % 8)
    return Status::ErrorInvalidAesKwDataLength();

  if (status.IsError())
    return status;
  return platform::EncryptDecryptAesKw(mode, sym_key, data, buffer);
}

Status DecryptDontCheckKeyUsage(const blink::WebCryptoAlgorithm& algorithm,
                                const blink::WebCryptoKey& key,
                                const CryptoData& data,
                                std::vector<uint8>* buffer) {
  if (algorithm.id() != key.algorithm().id())
    return Status::ErrorUnexpected();
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdAesCbc:
      return EncryptDecryptAesCbc(DECRYPT, algorithm, key, data, buffer);
    case blink::WebCryptoAlgorithmIdAesGcm:
      return EncryptDecryptAesGcm(DECRYPT, algorithm, key, data, buffer);
    case blink::WebCryptoAlgorithmIdRsaOaep:
      return DecryptRsaOaep(algorithm, key, data, buffer);
    case blink::WebCryptoAlgorithmIdAesKw:
      return EncryptDecryptAesKw(DECRYPT, algorithm, key, data, buffer);
    default:
      return Status::ErrorUnsupported();
  }
}

Status EncryptDontCheckUsage(const blink::WebCryptoAlgorithm& algorithm,
                             const blink::WebCryptoKey& key,
                             const CryptoData& data,
                             std::vector<uint8>* buffer) {
  if (algorithm.id() != key.algorithm().id())
    return Status::ErrorUnexpected();
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdAesCbc:
      return EncryptDecryptAesCbc(ENCRYPT, algorithm, key, data, buffer);
    case blink::WebCryptoAlgorithmIdAesGcm:
      return EncryptDecryptAesGcm(ENCRYPT, algorithm, key, data, buffer);
    case blink::WebCryptoAlgorithmIdAesKw:
      return EncryptDecryptAesKw(ENCRYPT, algorithm, key, data, buffer);
    case blink::WebCryptoAlgorithmIdRsaOaep:
      return EncryptRsaOaep(algorithm, key, data, buffer);
    default:
      return Status::ErrorUnsupported();
  }
}

Status UnwrapKeyDecryptAndImport(
    blink::WebCryptoKeyFormat format,
    const CryptoData& wrapped_key_data,
    const blink::WebCryptoKey& wrapping_key,
    const blink::WebCryptoAlgorithm& wrapping_algorithm,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* key) {
  std::vector<uint8> buffer;
  Status status = DecryptDontCheckKeyUsage(
      wrapping_algorithm, wrapping_key, wrapped_key_data, &buffer);
  if (status.IsError())
    return status;
  // NOTE that returning the details of ImportKey() failures may leak
  // information about the plaintext of the encrypted key (for instance the JWK
  // key_ops). As long as the ImportKey error messages don't describe actual
  // key bytes however this should be OK. For more discussion see
  // http://crubg.com/372040
  return ImportKey(
      format, CryptoData(buffer), algorithm, extractable, usage_mask, key);
}

Status WrapKeyExportAndEncrypt(
    blink::WebCryptoKeyFormat format,
    const blink::WebCryptoKey& key_to_wrap,
    const blink::WebCryptoKey& wrapping_key,
    const blink::WebCryptoAlgorithm& wrapping_algorithm,
    std::vector<uint8>* buffer) {
  std::vector<uint8> exported_data;
  Status status = ExportKey(format, key_to_wrap, &exported_data);
  if (status.IsError())
    return status;
  return EncryptDontCheckUsage(
      wrapping_algorithm, wrapping_key, CryptoData(exported_data), buffer);
}

// Returns the internal block size for SHA-*
unsigned int ShaBlockSizeBytes(blink::WebCryptoAlgorithmId hash_id) {
  switch (hash_id) {
    case blink::WebCryptoAlgorithmIdSha1:
    case blink::WebCryptoAlgorithmIdSha256:
      return 64;
    case blink::WebCryptoAlgorithmIdSha384:
    case blink::WebCryptoAlgorithmIdSha512:
      return 128;
    default:
      NOTREACHED();
      return 0;
  }
}

// Returns the mask of all key usages that are possible for |algorithm| and
// |key_type|. If the combination of |algorithm| and |key_type| doesn't make
// sense, then returns 0 (no usages).
blink::WebCryptoKeyUsageMask GetValidKeyUsagesForKeyType(
    blink::WebCryptoAlgorithmId algorithm,
    blink::WebCryptoKeyType key_type) {
  if (IsAlgorithmAsymmetric(algorithm) ==
      (key_type == blink::WebCryptoKeyTypeSecret))
    return 0;

  switch (algorithm) {
    case blink::WebCryptoAlgorithmIdAesCbc:
    case blink::WebCryptoAlgorithmIdAesGcm:
    case blink::WebCryptoAlgorithmIdAesCtr:
      return blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt |
             blink::WebCryptoKeyUsageWrapKey |
             blink::WebCryptoKeyUsageUnwrapKey;
    case blink::WebCryptoAlgorithmIdAesKw:
      return blink::WebCryptoKeyUsageWrapKey |
             blink::WebCryptoKeyUsageUnwrapKey;
    case blink::WebCryptoAlgorithmIdHmac:
      return blink::WebCryptoKeyUsageSign | blink::WebCryptoKeyUsageVerify;
    case blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
      switch (key_type) {
        case blink::WebCryptoKeyTypePublic:
          return blink::WebCryptoKeyUsageVerify;
        case blink::WebCryptoKeyTypePrivate:
          return blink::WebCryptoKeyUsageSign;
        default:
          return 0;
      }
    case blink::WebCryptoAlgorithmIdRsaOaep:
      switch (key_type) {
        case blink::WebCryptoKeyTypePublic:
          return blink::WebCryptoKeyUsageEncrypt |
                 blink::WebCryptoKeyUsageWrapKey;
        case blink::WebCryptoKeyTypePrivate:
          return blink::WebCryptoKeyUsageDecrypt |
                 blink::WebCryptoKeyUsageUnwrapKey;
        default:
          return 0;
      }
    default:
      return 0;
  }
}

// Returns Status::Success() if |usages| is a valid set of key usages for
// |algorithm| and |key_type|. Otherwise returns an error.
// In the case of JWK format the check is incomplete for asymmetric algorithms.
Status BestEffortCheckKeyUsagesForImport(blink::WebCryptoAlgorithmId algorithm,
                                         blink::WebCryptoKeyFormat format,
                                         blink::WebCryptoKeyUsageMask usages) {
  if (!IsAlgorithmAsymmetric(algorithm))
    return CheckKeyUsages(algorithm, blink::WebCryptoKeyTypeSecret, usages);

  // Try to infer the key type given the import format.
  blink::WebCryptoKeyType key_type;
  bool key_type_known = false;

  switch (format) {
    case blink::WebCryptoKeyFormatRaw:
      // TODO(eroman): The spec defines Diffie-Hellman raw import for public
      // keys, so this will need to be updated in the future when DH is
      // implemented.
      return Status::ErrorUnexpected();
    case blink::WebCryptoKeyFormatSpki:
      key_type = blink::WebCryptoKeyTypePublic;
      key_type_known = true;
      break;
    case blink::WebCryptoKeyFormatPkcs8:
      key_type = blink::WebCryptoKeyTypePrivate;
      key_type_known = true;
      break;
    case blink::WebCryptoKeyFormatJwk:
      key_type_known = false;
      break;
    default:
      return Status::ErrorUnexpected();
  }

  if (key_type_known)
    return CheckKeyUsages(algorithm, key_type, usages);

  // If the key type is not known, then the algorithm is asymmetric. Whether the
  // key data describes a public or private key isn't known yet. But it must at
  // least be ONE of those two.
  DCHECK(IsAlgorithmAsymmetric(algorithm));

  if (CheckKeyUsages(algorithm, blink::WebCryptoKeyTypePublic, usages)
          .IsError() &&
      CheckKeyUsages(algorithm, blink::WebCryptoKeyTypePrivate, usages)
          .IsError()) {
    return Status::ErrorCreateKeyBadUsages();
  }

  return Status::Success();
}

// Returns an error if |combined_usage_mask| is invalid for generating a key
// pair for |algorithm|. Otherwise returns Status::Success(), and fills
// |public_key_usages| with the usages for the public key, and
// |private_key_usages| with those for the private key.
Status CheckKeyUsagesForGenerateKeyPair(
    blink::WebCryptoAlgorithmId algorithm,
    blink::WebCryptoKeyUsageMask combined_usage_mask,
    blink::WebCryptoKeyUsageMask* public_key_usages,
    blink::WebCryptoKeyUsageMask* private_key_usages) {
  DCHECK(IsAlgorithmAsymmetric(algorithm));

  blink::WebCryptoKeyUsageMask all_public_key_usages =
      GetValidKeyUsagesForKeyType(algorithm, blink::WebCryptoKeyTypePublic);
  blink::WebCryptoKeyUsageMask all_private_key_usages =
      GetValidKeyUsagesForKeyType(algorithm, blink::WebCryptoKeyTypePrivate);

  if (!ContainsKeyUsages(all_public_key_usages | all_private_key_usages,
                         combined_usage_mask))
    return Status::ErrorCreateKeyBadUsages();

  *public_key_usages = combined_usage_mask & all_public_key_usages;
  *private_key_usages = combined_usage_mask & all_private_key_usages;

  return Status::Success();
}

// Converts a (big-endian) WebCrypto BigInteger, with or without leading zeros,
// to unsigned long.
bool BigIntegerToLong(const uint8* data,
                      unsigned int data_size,
                      unsigned long* result) {
  // TODO(padolph): Is it correct to say that empty data is an error, or does it
  // mean value 0? See https://www.w3.org/Bugs/Public/show_bug.cgi?id=23655
  if (data_size == 0)
    return false;

  *result = 0;
  for (size_t i = 0; i < data_size; ++i) {
    size_t reverse_i = data_size - i - 1;

    if (reverse_i >= sizeof(unsigned long) && data[i])
      return false;  // Too large for a long.

    *result |= data[i] << 8 * reverse_i;
  }
  return true;
}


}  // namespace

void Init() { platform::Init(); }

Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
               const blink::WebCryptoKey& key,
               const CryptoData& data,
               std::vector<uint8>* buffer) {
  if (!KeyUsageAllows(key, blink::WebCryptoKeyUsageEncrypt))
    return Status::ErrorUnexpected();
  return EncryptDontCheckUsage(algorithm, key, data, buffer);
}

Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
               const blink::WebCryptoKey& key,
               const CryptoData& data,
               std::vector<uint8>* buffer) {
  if (!KeyUsageAllows(key, blink::WebCryptoKeyUsageDecrypt))
    return Status::ErrorUnexpected();
  return DecryptDontCheckKeyUsage(algorithm, key, data, buffer);
}

Status Digest(const blink::WebCryptoAlgorithm& algorithm,
              const CryptoData& data,
              std::vector<uint8>* buffer) {
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdSha1:
    case blink::WebCryptoAlgorithmIdSha256:
    case blink::WebCryptoAlgorithmIdSha384:
    case blink::WebCryptoAlgorithmIdSha512:
      return platform::DigestSha(algorithm.id(), data, buffer);
    default:
      return Status::ErrorUnsupported();
  }
}

scoped_ptr<blink::WebCryptoDigestor> CreateDigestor(
    blink::WebCryptoAlgorithmId algorithm) {
  return platform::CreateDigestor(algorithm);
}

Status GenerateSecretKey(const blink::WebCryptoAlgorithm& algorithm,
                         bool extractable,
                         blink::WebCryptoKeyUsageMask usage_mask,
                         blink::WebCryptoKey* key) {
  Status status =
      CheckKeyUsages(algorithm.id(), blink::WebCryptoKeyTypeSecret, usage_mask);
  if (status.IsError())
    return status;

  unsigned int keylen_bytes = 0;

  // Get the secret key length in bytes from generation parameters.
  // This resolves any defaults.
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdAesCbc:
    case blink::WebCryptoAlgorithmIdAesGcm:
    case blink::WebCryptoAlgorithmIdAesKw: {
      if (!IsValidAesKeyLengthBits(algorithm.aesKeyGenParams()->lengthBits())) {
        return algorithm.aesKeyGenParams()->lengthBits() == 192
                   ? Status::ErrorAes192BitUnsupported()
                   : Status::ErrorGenerateKeyLength();
      }
      keylen_bytes = algorithm.aesKeyGenParams()->lengthBits() / 8;
      break;
    }
    case blink::WebCryptoAlgorithmIdHmac: {
      const blink::WebCryptoHmacKeyGenParams* params =
          algorithm.hmacKeyGenParams();
      DCHECK(params);
      if (params->hasLengthBits()) {
        if (params->optionalLengthBits() % 8)
          return Status::ErrorGenerateKeyLength();
        keylen_bytes = params->optionalLengthBits() / 8;
      } else {
        keylen_bytes = ShaBlockSizeBytes(params->hash().id());
        if (keylen_bytes == 0)
          return Status::ErrorUnsupported();
      }
      break;
    }

    default:
      return Status::ErrorUnsupported();
  }

  // TODO(eroman): Is this correct? HMAC can import zero-length keys, so should
  //               probably be able to allowed to generate them too.
  if (keylen_bytes == 0)
    return Status::ErrorGenerateKeyLength();

  return platform::GenerateSecretKey(
      algorithm, extractable, usage_mask, keylen_bytes, key);
}

Status GenerateKeyPair(const blink::WebCryptoAlgorithm& algorithm,
                       bool extractable,
                       blink::WebCryptoKeyUsageMask combined_usage_mask,
                       blink::WebCryptoKey* public_key,
                       blink::WebCryptoKey* private_key) {
  blink::WebCryptoKeyUsageMask public_key_usage_mask = 0;
  blink::WebCryptoKeyUsageMask private_key_usage_mask = 0;

  Status status = CheckKeyUsagesForGenerateKeyPair(algorithm.id(),
                                                   combined_usage_mask,
                                                   &public_key_usage_mask,
                                                   &private_key_usage_mask);
  if (status.IsError())
    return status;

  // TODO(padolph): Handle other asymmetric algorithm key generation.
  switch (algorithm.paramsType()) {
    case blink::WebCryptoAlgorithmParamsTypeRsaHashedKeyGenParams: {
      const blink::WebCryptoRsaHashedKeyGenParams* params =
          algorithm.rsaHashedKeyGenParams();

      if (!params->modulusLengthBits())
        return Status::ErrorGenerateRsaZeroModulus();

      unsigned long public_exponent = 0;
      if (!BigIntegerToLong(params->publicExponent().data(),
                            params->publicExponent().size(),
                            &public_exponent) ||
          (public_exponent != 3 && public_exponent != 65537)) {
        return Status::ErrorGenerateKeyPublicExponent();
      }

      return platform::GenerateRsaKeyPair(algorithm,
                                          extractable,
                                          public_key_usage_mask,
                                          private_key_usage_mask,
                                          params->modulusLengthBits(),
                                          public_exponent,
                                          public_key,
                                          private_key);
    }
    default:
      return Status::ErrorUnsupported();
  }
}

// Note that this function may be called from the target Blink thread.
Status ImportKey(blink::WebCryptoKeyFormat format,
                 const CryptoData& key_data,
                 const blink::WebCryptoAlgorithm& algorithm,
                 bool extractable,
                 blink::WebCryptoKeyUsageMask usage_mask,
                 blink::WebCryptoKey* key) {
  // This is "best effort" because it is incomplete for JWK (for which the key
  // type is not yet known). ImportKeyJwk() does extra checks on key usage once
  // the key type has been determined.
  Status status =
      BestEffortCheckKeyUsagesForImport(algorithm.id(), format, usage_mask);
  if (status.IsError())
    return status;

  switch (format) {
    case blink::WebCryptoKeyFormatRaw:
      return ImportKeyRaw(key_data, algorithm, extractable, usage_mask, key);
    case blink::WebCryptoKeyFormatSpki:
      return platform::ImportKeySpki(
          algorithm, key_data, extractable, usage_mask, key);
    case blink::WebCryptoKeyFormatPkcs8:
      return platform::ImportKeyPkcs8(
          algorithm, key_data, extractable, usage_mask, key);
    case blink::WebCryptoKeyFormatJwk:
      return ImportKeyJwk(key_data, algorithm, extractable, usage_mask, key);
    default:
      return Status::ErrorUnsupported();
  }
}

// TODO(eroman): Move this to anonymous namespace.
Status ExportKeyDontCheckExtractability(blink::WebCryptoKeyFormat format,
                                        const blink::WebCryptoKey& key,
                                        std::vector<uint8>* buffer) {
  switch (format) {
    case blink::WebCryptoKeyFormatRaw: {
      platform::SymKey* sym_key;
      Status status = ToPlatformSymKey(key, &sym_key);
      if (status.IsError())
        return status;
      return platform::ExportKeyRaw(sym_key, buffer);
    }
    case blink::WebCryptoKeyFormatSpki: {
      platform::PublicKey* public_key;
      Status status = ToPlatformPublicKey(key, &public_key);
      if (status.IsError())
        return status;
      return platform::ExportKeySpki(public_key, buffer);
    }
    case blink::WebCryptoKeyFormatPkcs8: {
      platform::PrivateKey* private_key;
      Status status = ToPlatformPrivateKey(key, &private_key);
      if (status.IsError())
        return status;
      return platform::ExportKeyPkcs8(private_key, key.algorithm(), buffer);
    }
    case blink::WebCryptoKeyFormatJwk:
      return ExportKeyJwk(key, buffer);
    default:
      return Status::ErrorUnsupported();
  }
}

Status ExportKey(blink::WebCryptoKeyFormat format,
                 const blink::WebCryptoKey& key,
                 std::vector<uint8>* buffer) {
  if (!key.extractable())
    return Status::ErrorKeyNotExtractable();
  return ExportKeyDontCheckExtractability(format, key, buffer);
}

Status Sign(const blink::WebCryptoAlgorithm& algorithm,
            const blink::WebCryptoKey& key,
            const CryptoData& data,
            std::vector<uint8>* buffer) {
  if (!KeyUsageAllows(key, blink::WebCryptoKeyUsageSign))
    return Status::ErrorUnexpected();
  if (algorithm.id() != key.algorithm().id())
    return Status::ErrorUnexpected();

  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdHmac:
      return SignHmac(algorithm, key, data, buffer);
    case blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
      return SignRsaSsaPkcs1v1_5(algorithm, key, data, buffer);
    default:
      return Status::ErrorUnsupported();
  }
}

Status VerifySignature(const blink::WebCryptoAlgorithm& algorithm,
                       const blink::WebCryptoKey& key,
                       const CryptoData& signature,
                       const CryptoData& data,
                       bool* signature_match) {
  if (!KeyUsageAllows(key, blink::WebCryptoKeyUsageVerify))
    return Status::ErrorUnexpected();
  if (algorithm.id() != key.algorithm().id())
    return Status::ErrorUnexpected();

  if (!signature.byte_length()) {
    // None of the algorithms generate valid zero-length signatures so this
    // will necessarily fail verification. Early return to protect
    // implementations from dealing with a NULL signature pointer.
    *signature_match = false;
    return Status::Success();
  }

  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdHmac:
      return VerifyHmac(algorithm, key, signature, data, signature_match);
    case blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
      return VerifyRsaSsaPkcs1v1_5(
          algorithm, key, signature, data, signature_match);
    default:
      return Status::ErrorUnsupported();
  }
}

Status WrapKey(blink::WebCryptoKeyFormat format,
               const blink::WebCryptoKey& key_to_wrap,
               const blink::WebCryptoKey& wrapping_key,
               const blink::WebCryptoAlgorithm& wrapping_algorithm,
               std::vector<uint8>* buffer) {
  if (!KeyUsageAllows(wrapping_key, blink::WebCryptoKeyUsageWrapKey))
    return Status::ErrorUnexpected();
  if (wrapping_algorithm.id() != wrapping_key.algorithm().id())
    return Status::ErrorUnexpected();

  return WrapKeyExportAndEncrypt(
      format, key_to_wrap, wrapping_key, wrapping_algorithm, buffer);
}

Status UnwrapKey(blink::WebCryptoKeyFormat format,
                 const CryptoData& wrapped_key_data,
                 const blink::WebCryptoKey& wrapping_key,
                 const blink::WebCryptoAlgorithm& wrapping_algorithm,
                 const blink::WebCryptoAlgorithm& algorithm,
                 bool extractable,
                 blink::WebCryptoKeyUsageMask usage_mask,
                 blink::WebCryptoKey* key) {
  if (!KeyUsageAllows(wrapping_key, blink::WebCryptoKeyUsageUnwrapKey))
    return Status::ErrorUnexpected();
  if (wrapping_algorithm.id() != wrapping_key.algorithm().id())
    return Status::ErrorUnexpected();

  // Fail-fast if the key usages don't make sense. This avoids decrypting the
  // key only to then have import fail. It is "best effort" because when
  // unwrapping JWK for asymmetric algorithms the key type isn't known yet.
  Status status =
      BestEffortCheckKeyUsagesForImport(algorithm.id(), format, usage_mask);
  if (status.IsError())
    return status;

  return UnwrapKeyDecryptAndImport(format,
                                   wrapped_key_data,
                                   wrapping_key,
                                   wrapping_algorithm,
                                   algorithm,
                                   extractable,
                                   usage_mask,
                                   key);
}

// Note that this function is called from the target Blink thread.
bool SerializeKeyForClone(const blink::WebCryptoKey& key,
                          blink::WebVector<uint8>* key_data) {
  return static_cast<webcrypto::platform::Key*>(key.handle())
      ->ThreadSafeSerializeForClone(key_data);
}

// Note that this function is called from the target Blink thread.
bool DeserializeKeyForClone(const blink::WebCryptoKeyAlgorithm& algorithm,
                            blink::WebCryptoKeyType type,
                            bool extractable,
                            blink::WebCryptoKeyUsageMask usage_mask,
                            const CryptoData& key_data,
                            blink::WebCryptoKey* key) {
  // TODO(eroman): This should not call into the platform crypto layer.
  // Otherwise it runs the risk of stalling while the NSS/OpenSSL global locks
  // are held.
  //
  // An alternate approach is to defer the key import until the key is used.
  // However this means that any deserialization errors would have to be
  // surfaced as WebCrypto errors, leading to slightly different behaviors. For
  // instance you could clone a key which fails to be deserialized.
  Status status = ImportKey(GetCloneFormatForKeyType(type),
                            key_data,
                            KeyAlgorithmToImportAlgorithm(algorithm),
                            extractable,
                            usage_mask,
                            key);
  if (status.IsError())
    return false;
  return ValidateDeserializedKey(*key, algorithm, type);
}

Status ToPlatformSymKey(const blink::WebCryptoKey& key,
                        platform::SymKey** out) {
  *out = static_cast<platform::Key*>(key.handle())->AsSymKey();
  if (!*out)
    return Status::ErrorUnexpectedKeyType();
  return Status::Success();
}

Status ToPlatformPublicKey(const blink::WebCryptoKey& key,
                           platform::PublicKey** out) {
  *out = static_cast<platform::Key*>(key.handle())->AsPublicKey();
  if (!*out)
    return Status::ErrorUnexpectedKeyType();
  return Status::Success();
}

Status ToPlatformPrivateKey(const blink::WebCryptoKey& key,
                            platform::PrivateKey** out) {
  *out = static_cast<platform::Key*>(key.handle())->AsPrivateKey();
  if (!*out)
    return Status::ErrorUnexpectedKeyType();
  return Status::Success();
}

// Returns Status::Success() if |usages| is a valid set of key usages for
// |algorithm| and |key_type|. Otherwise returns an error.
Status CheckKeyUsages(blink::WebCryptoAlgorithmId algorithm,
                      blink::WebCryptoKeyType key_type,
                      blink::WebCryptoKeyUsageMask usages) {
  if (!ContainsKeyUsages(GetValidKeyUsagesForKeyType(algorithm, key_type),
                         usages))
    return Status::ErrorCreateKeyBadUsages();

  return Status::Success();
}

}  // namespace webcrypto

}  // namespace content
