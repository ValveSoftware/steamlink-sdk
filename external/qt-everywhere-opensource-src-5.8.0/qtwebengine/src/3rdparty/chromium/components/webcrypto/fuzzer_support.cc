// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/fuzzer_support.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "components/test_runner/test_common.h"
#include "components/webcrypto/algorithm_dispatch.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/status.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"

namespace webcrypto {

namespace {

// This mock is used to initialize blink.
class InitOnce {
 public:
  InitOnce() {
    // EnsureBlinkInitialized() depends on the command line singleton being
    // initialized.
    base::CommandLine::Init(0, nullptr);
    test_runner::EnsureBlinkInitialized();
  }
};

base::LazyInstance<InitOnce>::Leaky g_once = LAZY_INSTANCE_INITIALIZER;

void EnsureInitialized() {
  g_once.Get();
}

blink::WebCryptoAlgorithm CreateRsaHashedImportAlgorithm(
    blink::WebCryptoAlgorithmId id,
    blink::WebCryptoAlgorithmId hash_id) {
  DCHECK(blink::WebCryptoAlgorithm::isHash(hash_id));
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      id,
      new blink::WebCryptoRsaHashedImportParams(
          blink::WebCryptoAlgorithm::adoptParamsAndCreate(hash_id, nullptr)));
}

blink::WebCryptoAlgorithm CreateEcImportAlgorithm(
    blink::WebCryptoAlgorithmId id,
    blink::WebCryptoNamedCurve named_curve) {
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      id, new blink::WebCryptoEcKeyImportParams(named_curve));
}

}  // namespace

blink::WebCryptoKeyUsageMask GetCompatibleKeyUsages(
    blink::WebCryptoKeyFormat format) {
  // SPKI format implies import of a public key, whereas PKCS8 implies import
  // of a private key. Pick usages that are compatible with a signature
  // algorithm.
  return format == blink::WebCryptoKeyFormatSpki
             ? blink::WebCryptoKeyUsageVerify
             : blink::WebCryptoKeyUsageSign;
}

void ImportEcKeyFromDerFuzzData(const uint8_t* data,
                                size_t size,
                                blink::WebCryptoKeyFormat format) {
  DCHECK(format == blink::WebCryptoKeyFormatSpki ||
         format == blink::WebCryptoKeyFormatPkcs8);
  EnsureInitialized();

  // There are 3 possible EC named curves. Fix this parameter. It shouldn't
  // matter based on the current implementation for PKCS8 or SPKI. But it
  // will have an impact when parsing JWK format.
  blink::WebCryptoNamedCurve curve = blink::WebCryptoNamedCurveP384;

  // Always use ECDSA as the algorithm. Shouldn't make much difference for
  // non-JWK formats.
  blink::WebCryptoAlgorithmId algorithm_id = blink::WebCryptoAlgorithmIdEcdsa;

  // Use key usages that are compatible with the chosen algorithm and key type.
  blink::WebCryptoKeyUsageMask usages = GetCompatibleKeyUsages(format);

  blink::WebCryptoKey key;
  webcrypto::Status status = webcrypto::ImportKey(
      format, webcrypto::CryptoData(data, size),
      CreateEcImportAlgorithm(algorithm_id, curve), true, usages, &key);

  // These errors imply a bad setup of parameters, and means ImportKey() may not
  // be testing the actual parsing.
  DCHECK_NE(status.error_details(),
            Status::ErrorUnsupportedImportKeyFormat().error_details());
  DCHECK_NE(status.error_details(),
            Status::ErrorCreateKeyBadUsages().error_details());
}

void ImportRsaKeyFromDerFuzzData(const uint8_t* data,
                                 size_t size,
                                 blink::WebCryptoKeyFormat format) {
  DCHECK(format == blink::WebCryptoKeyFormatSpki ||
         format == blink::WebCryptoKeyFormatPkcs8);
  EnsureInitialized();

  // There are several possible hash functions. Fix this parameter. It shouldn't
  // matter based on the current implementation for PKCS8 or SPKI. But it
  // will have an impact when parsing JWK format.
  blink::WebCryptoAlgorithmId hash_id = blink::WebCryptoAlgorithmIdSha256;

  // Always use RSA-SSA PKCS#1 as the algorithm. Shouldn't make much difference
  // for non-JWK formats.
  blink::WebCryptoAlgorithmId algorithm_id =
      blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5;

  // Use key usages that are compatible with the chosen algorithm and key type.
  blink::WebCryptoKeyUsageMask usages = GetCompatibleKeyUsages(format);

  blink::WebCryptoKey key;
  webcrypto::Status status = webcrypto::ImportKey(
      format, webcrypto::CryptoData(data, size),
      CreateRsaHashedImportAlgorithm(algorithm_id, hash_id), true, usages,
      &key);

  // These errors imply a bad setup of parameters, and means ImportKey() may not
  // be testing the actual parsing.
  DCHECK_NE(status.error_details(),
            Status::ErrorUnsupportedImportKeyFormat().error_details());
  DCHECK_NE(status.error_details(),
            Status::ErrorCreateKeyBadUsages().error_details());
}

}  // namespace webcrypto
