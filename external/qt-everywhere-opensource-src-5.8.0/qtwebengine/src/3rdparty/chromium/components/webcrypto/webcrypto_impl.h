// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBCRYPTO_WEBCRYPTO_IMPL_H_
#define COMPONENTS_WEBCRYPTO_WEBCRYPTO_IMPL_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "third_party/WebKit/public/platform/WebCrypto.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace webcrypto {

// Wrapper around the Blink WebCrypto asynchronous interface, which forwards to
// the synchronous OpenSSL implementation.
//
// WebCryptoImpl is threadsafe.
//
// EnsureInit() must be called prior to using methods on WebCryptoImpl().
class WebCryptoImpl : public blink::WebCrypto {
 public:
  WebCryptoImpl();

  ~WebCryptoImpl() override;

  void encrypt(const blink::WebCryptoAlgorithm& algorithm,
               const blink::WebCryptoKey& key,
               const unsigned char* data,
               unsigned int data_size,
               blink::WebCryptoResult result) override;
  void decrypt(const blink::WebCryptoAlgorithm& algorithm,
               const blink::WebCryptoKey& key,
               const unsigned char* data,
               unsigned int data_size,
               blink::WebCryptoResult result) override;
  void digest(const blink::WebCryptoAlgorithm& algorithm,
              const unsigned char* data,
              unsigned int data_size,
              blink::WebCryptoResult result) override;
  void generateKey(const blink::WebCryptoAlgorithm& algorithm,
                   bool extractable,
                   blink::WebCryptoKeyUsageMask usages,
                   blink::WebCryptoResult result) override;
  void importKey(blink::WebCryptoKeyFormat format,
                 const unsigned char* key_data,
                 unsigned int key_data_size,
                 const blink::WebCryptoAlgorithm& algorithm,
                 bool extractable,
                 blink::WebCryptoKeyUsageMask usages,
                 blink::WebCryptoResult result) override;
  void exportKey(blink::WebCryptoKeyFormat format,
                 const blink::WebCryptoKey& key,
                 blink::WebCryptoResult result) override;
  void sign(const blink::WebCryptoAlgorithm& algorithm,
            const blink::WebCryptoKey& key,
            const unsigned char* data,
            unsigned int data_size,
            blink::WebCryptoResult result) override;
  void verifySignature(const blink::WebCryptoAlgorithm& algorithm,
                       const blink::WebCryptoKey& key,
                       const unsigned char* signature,
                       unsigned int signature_size,
                       const unsigned char* data,
                       unsigned int data_size,
                       blink::WebCryptoResult result) override;
  void wrapKey(blink::WebCryptoKeyFormat format,
               const blink::WebCryptoKey& key,
               const blink::WebCryptoKey& wrapping_key,
               const blink::WebCryptoAlgorithm& wrap_algorithm,
               blink::WebCryptoResult result) override;
  void unwrapKey(blink::WebCryptoKeyFormat format,
                 const unsigned char* wrapped_key,
                 unsigned wrapped_key_size,
                 const blink::WebCryptoKey& wrapping_key,
                 const blink::WebCryptoAlgorithm& unwrap_algorithm,
                 const blink::WebCryptoAlgorithm& unwrapped_key_algorithm,
                 bool extractable,
                 blink::WebCryptoKeyUsageMask usages,
                 blink::WebCryptoResult result) override;

  void deriveBits(const blink::WebCryptoAlgorithm& algorithm,
                  const blink::WebCryptoKey& base_key,
                  unsigned int length_bits,
                  blink::WebCryptoResult result) override;

  void deriveKey(const blink::WebCryptoAlgorithm& algorithm,
                 const blink::WebCryptoKey& base_key,
                 const blink::WebCryptoAlgorithm& import_algorithm,
                 const blink::WebCryptoAlgorithm& key_length_algorithm,
                 bool extractable,
                 blink::WebCryptoKeyUsageMask usages,
                 blink::WebCryptoResult result) override;

  // This method returns a digestor object that can be used to synchronously
  // compute a digest one chunk at a time. Thus, the consume does not need to
  // hold onto a large buffer with all the data to digest. Chunks can be given
  // one at a time and the digest will be computed piecemeal. The allocated
  // WebCrytpoDigestor that is returned by createDigestor must be freed by the
  // caller.
  blink::WebCryptoDigestor* createDigestor(
      blink::WebCryptoAlgorithmId algorithm_id) override;

  bool deserializeKeyForClone(const blink::WebCryptoKeyAlgorithm& algorithm,
                              blink::WebCryptoKeyType type,
                              bool extractable,
                              blink::WebCryptoKeyUsageMask usages,
                              const unsigned char* key_data,
                              unsigned key_data_size,
                              blink::WebCryptoKey& key) override;

  bool serializeKeyForClone(const blink::WebCryptoKey& key,
                            blink::WebVector<unsigned char>& key_data) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebCryptoImpl);
};

}  // namespace webcrypto

#endif  // COMPONENTS_WEBCRYPTO_WEBCRYPTO_IMPL_H_
