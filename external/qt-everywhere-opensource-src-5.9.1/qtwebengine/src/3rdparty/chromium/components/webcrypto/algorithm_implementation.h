// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_ALGORITHM_IMPLEMENTATION_H_
#define COMPONENTS_WEBCRYPTO_ALGORITHM_IMPLEMENTATION_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "third_party/WebKit/public/platform/WebCrypto.h"

namespace webcrypto {

class CryptoData;
class GenerateKeyResult;
class Status;

// AlgorithmImplementation is a base class for *executing* the operations of an
// algorithm (generating keys, encrypting, signing, etc.).
//
// This is in contrast to blink::WebCryptoAlgorithm which instead *describes*
// the operation and its parameters.
//
// AlgorithmImplementation has reasonable default implementations for all
// methods which behave as if the operation is it is unsupported, so
// implementations need only override the applicable methods.
//
// Unless stated otherwise methods of AlgorithmImplementation are responsible
// for sanitizing their inputs. The following can be assumed:
//
//   * |algorithm.id()| and |key.algorithm.id()| matches the algorithm under
//     which the implementation was registerd.
//   * |algorithm| has the correct parameters type for the operation.
//   * The key usages have already been verified. In fact in the case of calls
//     to Encrypt()/Decrypt() the corresponding key usages may not be present
//     (when wrapping/unwrapping).
//
// An AlgorithmImplementation can also assume that crypto::EnsureOpenSSLInit()
// will be called before any of its methods are invoked (except the
// constructor).
class AlgorithmImplementation {
 public:
  virtual ~AlgorithmImplementation();

  // This is what is run whenever the spec says:
  //    "Let result be the result of performing the encrypt operation"
  //
  // (crypto.subtle.encrypt() dispatches to this)
  virtual Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& key,
                         const CryptoData& data,
                         std::vector<uint8_t>* buffer) const;

  // This is what is run whenever the spec says:
  //    "Let result be the result of performing the decrypt operation"
  //
  // (crypto.subtle.decrypt() dispatches to this)
  virtual Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& key,
                         const CryptoData& data,
                         std::vector<uint8_t>* buffer) const;

  // This is what is run whenever the spec says:
  //    "Let result be the result of performing the sign operation"
  //
  // (crypto.subtle.sign() dispatches to this)
  virtual Status Sign(const blink::WebCryptoAlgorithm& algorithm,
                      const blink::WebCryptoKey& key,
                      const CryptoData& data,
                      std::vector<uint8_t>* buffer) const;

  // This is what is run whenever the spec says:
  //    "Let result be the result of performing the verify operation"
  //
  // (crypto.subtle.verify() dispatches to this)
  virtual Status Verify(const blink::WebCryptoAlgorithm& algorithm,
                        const blink::WebCryptoKey& key,
                        const CryptoData& signature,
                        const CryptoData& data,
                        bool* signature_match) const;

  // This is what is run whenever the spec says:
  //    "Let result be the result of performing the digest operation"
  //
  // (crypto.subtle.digest() dispatches to this)
  virtual Status Digest(const blink::WebCryptoAlgorithm& algorithm,
                        const CryptoData& data,
                        std::vector<uint8_t>* buffer) const;

  // This is what is run whenever the spec says:
  //    "Let result be the result of executing the generate key operation"
  //
  // (crypto.subtle.generateKey() dispatches to this)
  virtual Status GenerateKey(const blink::WebCryptoAlgorithm& algorithm,
                             bool extractable,
                             blink::WebCryptoKeyUsageMask usages,
                             GenerateKeyResult* result) const;

  // This is what is run whenever the spec says:
  //    "Let result be a new ArrayBuffer containing the result of executing the
  //    derive bits operation"
  //
  // (crypto.subtle.deriveBits() dispatches to this)
  virtual Status DeriveBits(const blink::WebCryptoAlgorithm& algorithm,
                            const blink::WebCryptoKey& base_key,
                            bool has_optional_length_bits,
                            unsigned int optional_length_bits,
                            std::vector<uint8_t>* derived_bytes) const;

  // This is what is run whenever the spec says:
  //    "Let length be the result of executing the get key length algorithm"
  //
  // In the Web Crypto spec the operation returns either "null" or an
  // "Integer". In this code "null" is represented by setting
  // |*has_length_bits = false|.
  virtual Status GetKeyLength(
      const blink::WebCryptoAlgorithm& key_length_algorithm,
      bool* has_length_bits,
      unsigned int* length_bits) const;

  // This is what is run whenever the spec says:
  //    "Let result be the result of performing the import key operation"
  //
  // (crypto.subtle.importKey() dispatches to this).
  virtual Status ImportKey(blink::WebCryptoKeyFormat format,
                           const CryptoData& key_data,
                           const blink::WebCryptoAlgorithm& algorithm,
                           bool extractable,
                           blink::WebCryptoKeyUsageMask usages,
                           blink::WebCryptoKey* key) const;

  // This is what is run whenever the spec says:
  //    "Let result be the result of performing the export key operation"
  //
  // (crypto.subtle.exportKey() dispatches to this).
  virtual Status ExportKey(blink::WebCryptoKeyFormat format,
                           const blink::WebCryptoKey& key,
                           std::vector<uint8_t>* buffer) const;

  // -----------------------------------------------
  // Structured clone
  // -----------------------------------------------

  // The Structured clone methods are used for synchronous serialization /
  // deserialization of a WebCryptoKey.
  //
  // This serialized format is used by Blink to:
  //   * Copy WebCryptoKeys between threads (postMessage to WebWorkers)
  //   * Copy WebCryptoKeys between domains (postMessage)
  //   * Copy WebCryptoKeys within the same domain (postMessage)
  //   * Persist the key to storage (IndexedDB)
  //
  // Implementations of structured cloning must:
  //   * Be threadsafe (structured cloning is called directly on the Blink
  //     thread, in contrast to the other methods of AlgorithmImplementation).
  //   * Use a stable format (a serialized key must forever be de-serializable,
  //     and be able to survive future migrations to crypto libraries)
  //   * Work for all keys (including ones marked as non-extractable).
  //
  // Tests to verify structured cloning are available in:
  //   LayoutTests/crypto/clone-*.html

  // Note that SerializeKeyForClone() is not virtual because all
  // implementations end up doing the same thing.
  Status SerializeKeyForClone(const blink::WebCryptoKey& key,
                              blink::WebVector<uint8_t>* key_data) const;

  virtual Status DeserializeKeyForClone(
      const blink::WebCryptoKeyAlgorithm& algorithm,
      blink::WebCryptoKeyType type,
      bool extractable,
      blink::WebCryptoKeyUsageMask usages,
      const CryptoData& key_data,
      blink::WebCryptoKey* key) const;
};

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_ALGORITHM_IMPLEMENTATION_H_
