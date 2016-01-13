// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/platform_crypto.h"

#include <vector>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "crypto/openssl_util.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace content {

namespace webcrypto {

namespace platform {

class SymKey : public Key {
 public:
  explicit SymKey(const CryptoData& key_data)
      : key_(key_data.bytes(), key_data.bytes() + key_data.byte_length()) {}

  virtual SymKey* AsSymKey() OVERRIDE { return this; }
  virtual PublicKey* AsPublicKey() OVERRIDE { return NULL; }
  virtual PrivateKey* AsPrivateKey() OVERRIDE { return NULL; }
  virtual bool ThreadSafeSerializeForClone(
      blink::WebVector<uint8>* key_data) OVERRIDE {
    key_data->assign(Uint8VectorStart(key_), key_.size());
    return true;
  }

  const std::vector<unsigned char>& key() const { return key_; }

 private:
  const std::vector<unsigned char> key_;

  DISALLOW_COPY_AND_ASSIGN(SymKey);
};

namespace {

const EVP_CIPHER* GetAESCipherByKeyLength(unsigned int key_length_bytes) {
  // OpenSSL supports AES CBC ciphers for only 3 key lengths: 128, 192, 256 bits
  switch (key_length_bytes) {
    case 16:
      return EVP_aes_128_cbc();
    case 24:
      return EVP_aes_192_cbc();
    case 32:
      return EVP_aes_256_cbc();
    default:
      return NULL;
  }
}

const EVP_MD* GetDigest(blink::WebCryptoAlgorithmId id) {
  switch (id) {
    case blink::WebCryptoAlgorithmIdSha1:
      return EVP_sha1();
    case blink::WebCryptoAlgorithmIdSha256:
      return EVP_sha256();
    case blink::WebCryptoAlgorithmIdSha384:
      return EVP_sha384();
    case blink::WebCryptoAlgorithmIdSha512:
      return EVP_sha512();
    default:
      return NULL;
  }
}

// OpenSSL constants for EVP_CipherInit_ex(), do not change
enum CipherOperation { kDoDecrypt = 0, kDoEncrypt = 1 };

Status AesCbcEncryptDecrypt(EncryptOrDecrypt mode,
                            SymKey* key,
                            const CryptoData& iv,
                            const CryptoData& data,
                            std::vector<uint8>* buffer) {
  CipherOperation cipher_operation =
      (mode == ENCRYPT) ? kDoEncrypt : kDoDecrypt;

  if (data.byte_length() >= INT_MAX - AES_BLOCK_SIZE) {
    // TODO(padolph): Handle this by chunking the input fed into OpenSSL. Right
    // now it doesn't make much difference since the one-shot API would end up
    // blowing out the memory and crashing anyway.
    return Status::ErrorDataTooLarge();
  }

  // Note: PKCS padding is enabled by default
  crypto::ScopedOpenSSL<EVP_CIPHER_CTX, EVP_CIPHER_CTX_free> context(
      EVP_CIPHER_CTX_new());

  if (!context.get())
    return Status::OperationError();

  const EVP_CIPHER* const cipher = GetAESCipherByKeyLength(key->key().size());
  DCHECK(cipher);

  if (!EVP_CipherInit_ex(context.get(),
                         cipher,
                         NULL,
                         &key->key()[0],
                         iv.bytes(),
                         cipher_operation)) {
    return Status::OperationError();
  }

  // According to the openssl docs, the amount of data written may be as large
  // as (data_size + cipher_block_size - 1), constrained to a multiple of
  // cipher_block_size.
  unsigned int output_max_len = data.byte_length() + AES_BLOCK_SIZE - 1;
  const unsigned remainder = output_max_len % AES_BLOCK_SIZE;
  if (remainder != 0)
    output_max_len += AES_BLOCK_SIZE - remainder;
  DCHECK_GT(output_max_len, data.byte_length());

  buffer->resize(output_max_len);

  unsigned char* const buffer_data = Uint8VectorStart(buffer);

  int output_len = 0;
  if (!EVP_CipherUpdate(context.get(),
                        buffer_data,
                        &output_len,
                        data.bytes(),
                        data.byte_length()))
    return Status::OperationError();
  int final_output_chunk_len = 0;
  if (!EVP_CipherFinal_ex(
          context.get(), buffer_data + output_len, &final_output_chunk_len)) {
    return Status::OperationError();
  }

  const unsigned int final_output_len =
      static_cast<unsigned int>(output_len) +
      static_cast<unsigned int>(final_output_chunk_len);
  DCHECK_LE(final_output_len, output_max_len);

  buffer->resize(final_output_len);

  return Status::Success();
}

}  // namespace

class DigestorOpenSSL : public blink::WebCryptoDigestor {
 public:
  explicit DigestorOpenSSL(blink::WebCryptoAlgorithmId algorithm_id)
      : initialized_(false),
        digest_context_(EVP_MD_CTX_create()),
        algorithm_id_(algorithm_id) {}

  virtual bool consume(const unsigned char* data, unsigned int size) {
    return ConsumeWithStatus(data, size).IsSuccess();
  }

  Status ConsumeWithStatus(const unsigned char* data, unsigned int size) {
    crypto::OpenSSLErrStackTracer(FROM_HERE);
    Status error = Init();
    if (!error.IsSuccess())
      return error;

    if (!EVP_DigestUpdate(digest_context_.get(), data, size))
      return Status::OperationError();

    return Status::Success();
  }

  virtual bool finish(unsigned char*& result_data,
                      unsigned int& result_data_size) {
    Status error = FinishInternal(result_, &result_data_size);
    if (!error.IsSuccess())
      return false;
    result_data = result_;
    return true;
  }

  Status FinishWithVectorAndStatus(std::vector<uint8>* result) {
    const int hash_expected_size = EVP_MD_CTX_size(digest_context_.get());
    result->resize(hash_expected_size);
    unsigned char* const hash_buffer = Uint8VectorStart(result);
    unsigned int hash_buffer_size;  // ignored
    return FinishInternal(hash_buffer, &hash_buffer_size);
  }

 private:
  Status Init() {
    if (initialized_)
      return Status::Success();

    const EVP_MD* digest_algorithm = GetDigest(algorithm_id_);
    if (!digest_algorithm)
      return Status::ErrorUnexpected();

    if (!digest_context_.get())
      return Status::OperationError();

    if (!EVP_DigestInit_ex(digest_context_.get(), digest_algorithm, NULL))
      return Status::OperationError();

    initialized_ = true;
    return Status::Success();
  }

  Status FinishInternal(unsigned char* result, unsigned int* result_size) {
    crypto::OpenSSLErrStackTracer(FROM_HERE);
    Status error = Init();
    if (!error.IsSuccess())
      return error;

    const int hash_expected_size = EVP_MD_CTX_size(digest_context_.get());
    if (hash_expected_size <= 0)
      return Status::ErrorUnexpected();
    DCHECK_LE(hash_expected_size, EVP_MAX_MD_SIZE);

    if (!EVP_DigestFinal_ex(digest_context_.get(), result, result_size) ||
        static_cast<int>(*result_size) != hash_expected_size)
      return Status::OperationError();

    return Status::Success();
  }

  bool initialized_;
  crypto::ScopedOpenSSL<EVP_MD_CTX, EVP_MD_CTX_destroy> digest_context_;
  blink::WebCryptoAlgorithmId algorithm_id_;
  unsigned char result_[EVP_MAX_MD_SIZE];
};

Status ExportKeyRaw(SymKey* key, std::vector<uint8>* buffer) {
  *buffer = key->key();
  return Status::Success();
}

void Init() { crypto::EnsureOpenSSLInit(); }

Status EncryptDecryptAesCbc(EncryptOrDecrypt mode,
                            SymKey* key,
                            const CryptoData& data,
                            const CryptoData& iv,
                            std::vector<uint8>* buffer) {
  // TODO(eroman): inline the function here.
  return AesCbcEncryptDecrypt(mode, key, iv, data, buffer);
}

Status DigestSha(blink::WebCryptoAlgorithmId algorithm,
                 const CryptoData& data,
                 std::vector<uint8>* buffer) {
  DigestorOpenSSL digestor(algorithm);
  Status error = digestor.ConsumeWithStatus(data.bytes(), data.byte_length());
  if (!error.IsSuccess())
    return error;
  return digestor.FinishWithVectorAndStatus(buffer);
}

scoped_ptr<blink::WebCryptoDigestor> CreateDigestor(
    blink::WebCryptoAlgorithmId algorithm_id) {
  return scoped_ptr<blink::WebCryptoDigestor>(
      new DigestorOpenSSL(algorithm_id));
}

Status GenerateSecretKey(const blink::WebCryptoAlgorithm& algorithm,
                         bool extractable,
                         blink::WebCryptoKeyUsageMask usage_mask,
                         unsigned keylen_bytes,
                         blink::WebCryptoKey* key) {
  // TODO(eroman): Is this right?
  if (keylen_bytes == 0)
    return Status::ErrorGenerateKeyLength();

  crypto::OpenSSLErrStackTracer(FROM_HERE);

  std::vector<unsigned char> random_bytes(keylen_bytes, 0);
  if (!(RAND_bytes(&random_bytes[0], keylen_bytes)))
    return Status::OperationError();

  blink::WebCryptoKeyAlgorithm key_algorithm;
  if (!CreateSecretKeyAlgorithm(algorithm, keylen_bytes, &key_algorithm))
    return Status::ErrorUnexpected();

  *key = blink::WebCryptoKey::create(new SymKey(CryptoData(random_bytes)),
                                     blink::WebCryptoKeyTypeSecret,
                                     extractable,
                                     key_algorithm,
                                     usage_mask);

  return Status::Success();
}

Status GenerateRsaKeyPair(const blink::WebCryptoAlgorithm& algorithm,
                          bool extractable,
                          blink::WebCryptoKeyUsageMask public_key_usage_mask,
                          blink::WebCryptoKeyUsageMask private_key_usage_mask,
                          unsigned int modulus_length_bits,
                          unsigned long public_exponent,
                          blink::WebCryptoKey* public_key,
                          blink::WebCryptoKey* private_key) {
  // TODO(padolph): Placeholder for OpenSSL implementation.
  // Issue http://crbug.com/267888.
  return Status::ErrorUnsupported();
}

Status ImportKeyRaw(const blink::WebCryptoAlgorithm& algorithm,
                    const CryptoData& key_data,
                    bool extractable,
                    blink::WebCryptoKeyUsageMask usage_mask,
                    blink::WebCryptoKey* key) {

  blink::WebCryptoKeyAlgorithm key_algorithm;
  if (!CreateSecretKeyAlgorithm(
          algorithm, key_data.byte_length(), &key_algorithm))
    return Status::ErrorUnexpected();

  *key = blink::WebCryptoKey::create(new SymKey(key_data),
                                     blink::WebCryptoKeyTypeSecret,
                                     extractable,
                                     key_algorithm,
                                     usage_mask);

  return Status::Success();
}

Status SignHmac(SymKey* key,
                const blink::WebCryptoAlgorithm& hash,
                const CryptoData& data,
                std::vector<uint8>* buffer) {
  const EVP_MD* digest_algorithm = GetDigest(hash.id());
  if (!digest_algorithm)
    return Status::ErrorUnsupported();
  unsigned int hmac_expected_length = EVP_MD_size(digest_algorithm);

  const std::vector<unsigned char>& raw_key = key->key();

  // OpenSSL wierdness here.
  // First, HMAC() needs a void* for the key data, so make one up front as a
  // cosmetic to avoid a cast. Second, OpenSSL does not like a NULL key,
  // which will result if the raw_key vector is empty; an entirely valid
  // case. Handle this specific case by pointing to an empty array.
  const unsigned char null_key[] = {};
  const void* const raw_key_voidp = raw_key.size() ? &raw_key[0] : null_key;

  buffer->resize(hmac_expected_length);
  crypto::ScopedOpenSSLSafeSizeBuffer<EVP_MAX_MD_SIZE> hmac_result(
      Uint8VectorStart(buffer), hmac_expected_length);

  crypto::OpenSSLErrStackTracer(FROM_HERE);

  unsigned int hmac_actual_length;
  unsigned char* const success = HMAC(digest_algorithm,
                                      raw_key_voidp,
                                      raw_key.size(),
                                      data.bytes(),
                                      data.byte_length(),
                                      hmac_result.safe_buffer(),
                                      &hmac_actual_length);
  if (!success || hmac_actual_length != hmac_expected_length)
    return Status::OperationError();

  return Status::Success();
}

Status ImportRsaPublicKey(const blink::WebCryptoAlgorithm& algorithm,
                          bool extractable,
                          blink::WebCryptoKeyUsageMask usage_mask,
                          const CryptoData& modulus_data,
                          const CryptoData& exponent_data,
                          blink::WebCryptoKey* key) {
  // TODO(padolph): Placeholder for OpenSSL implementation.
  // Issue
  return Status::ErrorUnsupported();
}

Status ImportRsaPrivateKey(const blink::WebCryptoAlgorithm& algorithm,
                           bool extractable,
                           blink::WebCryptoKeyUsageMask usage_mask,
                           const CryptoData& modulus,
                           const CryptoData& public_exponent,
                           const CryptoData& private_exponent,
                           const CryptoData& prime1,
                           const CryptoData& prime2,
                           const CryptoData& exponent1,
                           const CryptoData& exponent2,
                           const CryptoData& coefficient,
                           blink::WebCryptoKey* key) {
  // TODO(eroman): http://crbug.com/267888
  return Status::ErrorUnsupported();
}

Status EncryptDecryptAesGcm(EncryptOrDecrypt mode,
                            SymKey* key,
                            const CryptoData& data,
                            const CryptoData& iv,
                            const CryptoData& additional_data,
                            unsigned int tag_length_bits,
                            std::vector<uint8>* buffer) {
  // TODO(eroman): http://crbug.com/267888
  return Status::ErrorUnsupported();
}

Status EncryptRsaOaep(PublicKey* key,
                      const blink::WebCryptoAlgorithm& hash,
                      const CryptoData& label,
                      const CryptoData& data,
                      std::vector<uint8>* buffer) {
  // TODO(eroman): http://crbug.com/267888
  return Status::ErrorUnsupported();
}

Status DecryptRsaOaep(PrivateKey* key,
                      const blink::WebCryptoAlgorithm& hash,
                      const CryptoData& label,
                      const CryptoData& data,
                      std::vector<uint8>* buffer) {
  // TODO(eroman): http://crbug.com/267888
  return Status::ErrorUnsupported();
}

Status SignRsaSsaPkcs1v1_5(PrivateKey* key,
                           const blink::WebCryptoAlgorithm& hash,
                           const CryptoData& data,
                           std::vector<uint8>* buffer) {
  // TODO(eroman): http://crbug.com/267888
  return Status::ErrorUnsupported();
}

// Key is guaranteed to be an RSA SSA key.
Status VerifyRsaSsaPkcs1v1_5(PublicKey* key,
                             const blink::WebCryptoAlgorithm& hash,
                             const CryptoData& signature,
                             const CryptoData& data,
                             bool* signature_match) {
  // TODO(eroman): http://crbug.com/267888
  return Status::ErrorUnsupported();
}

Status ImportKeySpki(const blink::WebCryptoAlgorithm& algorithm,
                     const CryptoData& key_data,
                     bool extractable,
                     blink::WebCryptoKeyUsageMask usage_mask,
                     blink::WebCryptoKey* key) {
  // TODO(eroman): http://crbug.com/267888
  return Status::ErrorUnsupported();
}

Status ImportKeyPkcs8(const blink::WebCryptoAlgorithm& algorithm,
                      const CryptoData& key_data,
                      bool extractable,
                      blink::WebCryptoKeyUsageMask usage_mask,
                      blink::WebCryptoKey* key) {
  // TODO(eroman): http://crbug.com/267888
  return Status::ErrorUnsupported();
}

Status ExportKeySpki(PublicKey* key, std::vector<uint8>* buffer) {
  // TODO(eroman): http://crbug.com/267888
  return Status::ErrorUnsupported();
}

Status ExportKeyPkcs8(PrivateKey* key,
                      const blink::WebCryptoKeyAlgorithm& key_algorithm,
                      std::vector<uint8>* buffer) {
  // TODO(eroman): http://crbug.com/267888
  return Status::ErrorUnsupported();
}

Status ExportRsaPublicKey(PublicKey* key,
                          std::vector<uint8>* modulus,
                          std::vector<uint8>* public_exponent) {
  // TODO(eroman): http://crbug.com/267888
  return Status::ErrorUnsupported();
}

Status ExportRsaPrivateKey(PrivateKey* key,
                           std::vector<uint8>* modulus,
                           std::vector<uint8>* public_exponent,
                           std::vector<uint8>* private_exponent,
                           std::vector<uint8>* prime1,
                           std::vector<uint8>* prime2,
                           std::vector<uint8>* exponent1,
                           std::vector<uint8>* exponent2,
                           std::vector<uint8>* coefficient) {
  // TODO(eroman): http://crbug.com/267888
  return Status::ErrorUnsupported();
}

Status EncryptDecryptAesKw(EncryptOrDecrypt mode,
                           SymKey* key,
                           const CryptoData& data,
                           std::vector<uint8>* buffer) {
  // TODO(eroman): http://crbug.com/267888
  return Status::ErrorUnsupported();
}

bool ThreadSafeDeserializeKeyForClone(
    const blink::WebCryptoKeyAlgorithm& algorithm,
    blink::WebCryptoKeyType type,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    const CryptoData& key_data,
    blink::WebCryptoKey* key) {
  // TODO(eroman): http://crbug.com/267888
  return false;
}

}  // namespace platform

}  // namespace webcrypto

}  // namespace content
