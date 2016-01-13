// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBCRYPTO_WEBCRYPTO_IMPL_H_
#define CONTENT_CHILD_WEBCRYPTO_WEBCRYPTO_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "third_party/WebKit/public/platform/WebCrypto.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace content {

// Wrapper around the Blink WebCrypto asynchronous interface, which forwards to
// the synchronous platform (NSS or OpenSSL) implementation.
//
// WebCryptoImpl is threadsafe.
//
// EnsureInit() must be called prior to using methods on WebCryptoImpl().
class WebCryptoImpl : public blink::WebCrypto {
 public:
  WebCryptoImpl();

  virtual ~WebCryptoImpl();

  static void EnsureInit();

  virtual void encrypt(const blink::WebCryptoAlgorithm& algorithm,
                       const blink::WebCryptoKey& key,
                       const unsigned char* data,
                       unsigned int data_size,
                       blink::WebCryptoResult result);
  virtual void decrypt(const blink::WebCryptoAlgorithm& algorithm,
                       const blink::WebCryptoKey& key,
                       const unsigned char* data,
                       unsigned int data_size,
                       blink::WebCryptoResult result);
  virtual void digest(const blink::WebCryptoAlgorithm& algorithm,
                      const unsigned char* data,
                      unsigned int data_size,
                      blink::WebCryptoResult result);
  virtual void generateKey(const blink::WebCryptoAlgorithm& algorithm,
                           bool extractable,
                           blink::WebCryptoKeyUsageMask usage_mask,
                           blink::WebCryptoResult result);
  virtual void importKey(blink::WebCryptoKeyFormat format,
                         const unsigned char* key_data,
                         unsigned int key_data_size,
                         const blink::WebCryptoAlgorithm& algorithm,
                         bool extractable,
                         blink::WebCryptoKeyUsageMask usage_mask,
                         blink::WebCryptoResult result);
  virtual void exportKey(blink::WebCryptoKeyFormat format,
                         const blink::WebCryptoKey& key,
                         blink::WebCryptoResult result);
  virtual void sign(const blink::WebCryptoAlgorithm& algorithm,
                    const blink::WebCryptoKey& key,
                    const unsigned char* data,
                    unsigned int data_size,
                    blink::WebCryptoResult result);
  virtual void verifySignature(const blink::WebCryptoAlgorithm& algorithm,
                               const blink::WebCryptoKey& key,
                               const unsigned char* signature,
                               unsigned int signature_size,
                               const unsigned char* data,
                               unsigned int data_size,
                              blink::WebCryptoResult result);
  virtual void wrapKey(blink::WebCryptoKeyFormat format,
                       const blink::WebCryptoKey& key,
                       const blink::WebCryptoKey& wrapping_key,
                       const blink::WebCryptoAlgorithm& wrap_algorithm,
                       blink::WebCryptoResult result);
  virtual void unwrapKey(
      blink::WebCryptoKeyFormat format,
      const unsigned char* wrapped_key,
      unsigned wrapped_key_size,
      const blink::WebCryptoKey& wrapping_key,
      const blink::WebCryptoAlgorithm& unwrap_algorithm,
      const blink::WebCryptoAlgorithm& unwrapped_key_algorithm,
      bool extractable,
      blink::WebCryptoKeyUsageMask usages,
      blink::WebCryptoResult result);

  // This method returns a digestor object that can be used to synchronously
  // compute a digest one chunk at a time. Thus, the consume does not need to
  // hold onto a large buffer with all the data to digest. Chunks can be given
  // one at a time and the digest will be computed piecemeal. The allocated
  // WebCrytpoDigestor that is returned by createDigestor must be freed by the
  // caller.
  virtual blink::WebCryptoDigestor* createDigestor(
      blink::WebCryptoAlgorithmId algorithm_id);

  virtual bool deserializeKeyForClone(
      const blink::WebCryptoKeyAlgorithm& algorithm,
      blink::WebCryptoKeyType type,
      bool extractable,
      blink::WebCryptoKeyUsageMask usages,
      const unsigned char* key_data,
      unsigned key_data_size,
      blink::WebCryptoKey& key);

  virtual bool serializeKeyForClone(const blink::WebCryptoKey& key,
                                    blink::WebVector<unsigned char>& key_data);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebCryptoImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_WEBCRYPTO_WEBCRYPTO_IMPL_H_
