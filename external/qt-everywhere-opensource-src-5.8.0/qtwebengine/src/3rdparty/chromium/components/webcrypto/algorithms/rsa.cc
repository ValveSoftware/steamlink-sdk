// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/algorithms/rsa.h"

#include <openssl/evp.h>
#include <utility>

#include "base/logging.h"
#include "components/webcrypto/algorithms/asymmetric_key_util.h"
#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/generate_key_result.h"
#include "components/webcrypto/jwk.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_openssl_types.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace webcrypto {

namespace {

// Describes the RSA components for a parsed key. The names of the properties
// correspond with those from the JWK spec. Note that Chromium's WebCrypto
// implementation does not support multi-primes, so there is no parsed field
// for "oth".
struct JwkRsaInfo {
  bool is_private_key = false;
  std::string n;
  std::string e;
  std::string d;
  std::string p;
  std::string q;
  std::string dp;
  std::string dq;
  std::string qi;
};

// Parses a UTF-8 encoded JWK (key_data), and extracts the RSA components to
// |*result|. Returns Status::Success() on success, otherwise an error.
// In order for this to succeed:
//   * expected_alg must match the JWK's "alg", if present.
//   * expected_extractable must be consistent with the JWK's "ext", if
//     present.
//   * expected_usages must be a subset of the JWK's "key_ops" if present.
Status ReadRsaKeyJwk(const CryptoData& key_data,
                     const std::string& expected_alg,
                     bool expected_extractable,
                     blink::WebCryptoKeyUsageMask expected_usages,
                     JwkRsaInfo* result) {
  JwkReader jwk;
  Status status = jwk.Init(key_data, expected_extractable, expected_usages,
                           "RSA", expected_alg);
  if (status.IsError())
    return status;

  // An RSA public key must have an "n" (modulus) and an "e" (exponent) entry
  // in the JWK, while an RSA private key must have those, plus at least a "d"
  // (private exponent) entry.
  // See http://tools.ietf.org/html/draft-ietf-jose-json-web-algorithms-18,
  // section 6.3.
  status = jwk.GetBigInteger("n", &result->n);
  if (status.IsError())
    return status;
  status = jwk.GetBigInteger("e", &result->e);
  if (status.IsError())
    return status;

  result->is_private_key = jwk.HasMember("d");
  if (!result->is_private_key)
    return Status::Success();

  status = jwk.GetBigInteger("d", &result->d);
  if (status.IsError())
    return status;

  // The "p", "q", "dp", "dq", and "qi" properties are optional in the JWA
  // spec. However they are required by Chromium's WebCrypto implementation.

  status = jwk.GetBigInteger("p", &result->p);
  if (status.IsError())
    return status;

  status = jwk.GetBigInteger("q", &result->q);
  if (status.IsError())
    return status;

  status = jwk.GetBigInteger("dp", &result->dp);
  if (status.IsError())
    return status;

  status = jwk.GetBigInteger("dq", &result->dq);
  if (status.IsError())
    return status;

  status = jwk.GetBigInteger("qi", &result->qi);
  if (status.IsError())
    return status;

  return Status::Success();
}

// Creates a blink::WebCryptoAlgorithm having the modulus length and public
// exponent  of |key|.
Status CreateRsaHashedKeyAlgorithm(
    blink::WebCryptoAlgorithmId rsa_algorithm,
    blink::WebCryptoAlgorithmId hash_algorithm,
    EVP_PKEY* key,
    blink::WebCryptoKeyAlgorithm* key_algorithm) {
  DCHECK_EQ(EVP_PKEY_RSA, EVP_PKEY_id(key));

  crypto::ScopedRSA rsa(EVP_PKEY_get1_RSA(key));
  if (!rsa.get())
    return Status::ErrorUnexpected();

  unsigned int modulus_length_bits = BN_num_bits(rsa.get()->n);

  // Convert the public exponent to big-endian representation.
  std::vector<uint8_t> e(BN_num_bytes(rsa.get()->e));
  if (e.size() == 0)
    return Status::ErrorUnexpected();
  if (e.size() != BN_bn2bin(rsa.get()->e, &e[0]))
    return Status::ErrorUnexpected();

  *key_algorithm = blink::WebCryptoKeyAlgorithm::createRsaHashed(
      rsa_algorithm, modulus_length_bits, &e[0],
      static_cast<unsigned int>(e.size()), hash_algorithm);

  return Status::Success();
}

// Creates a WebCryptoKey that wraps |private_key|.
Status CreateWebCryptoRsaPrivateKey(
    crypto::ScopedEVP_PKEY private_key,
    const blink::WebCryptoAlgorithmId rsa_algorithm_id,
    const blink::WebCryptoAlgorithm& hash,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) {
  blink::WebCryptoKeyAlgorithm key_algorithm;
  Status status = CreateRsaHashedKeyAlgorithm(
      rsa_algorithm_id, hash.id(), private_key.get(), &key_algorithm);
  if (status.IsError())
    return status;

  return CreateWebCryptoPrivateKey(std::move(private_key), key_algorithm,
                                   extractable, usages, key);
}

// Creates a WebCryptoKey that wraps |public_key|.
Status CreateWebCryptoRsaPublicKey(
    crypto::ScopedEVP_PKEY public_key,
    const blink::WebCryptoAlgorithmId rsa_algorithm_id,
    const blink::WebCryptoAlgorithm& hash,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) {
  blink::WebCryptoKeyAlgorithm key_algorithm;
  Status status = CreateRsaHashedKeyAlgorithm(rsa_algorithm_id, hash.id(),
                                              public_key.get(), &key_algorithm);
  if (status.IsError())
    return status;

  return CreateWebCryptoPublicKey(std::move(public_key), key_algorithm,
                                  extractable, usages, key);
}

Status ImportRsaPrivateKey(const blink::WebCryptoAlgorithm& algorithm,
                           bool extractable,
                           blink::WebCryptoKeyUsageMask usages,
                           const JwkRsaInfo& params,
                           blink::WebCryptoKey* key) {
  crypto::ScopedRSA rsa(RSA_new());

  rsa->n = CreateBIGNUM(params.n);
  rsa->e = CreateBIGNUM(params.e);
  rsa->d = CreateBIGNUM(params.d);
  rsa->p = CreateBIGNUM(params.p);
  rsa->q = CreateBIGNUM(params.q);
  rsa->dmp1 = CreateBIGNUM(params.dp);
  rsa->dmq1 = CreateBIGNUM(params.dq);
  rsa->iqmp = CreateBIGNUM(params.qi);

  if (!rsa->n || !rsa->e || !rsa->d || !rsa->p || !rsa->q || !rsa->dmp1 ||
      !rsa->dmq1 || !rsa->iqmp) {
    return Status::OperationError();
  }

  // TODO(eroman): This should be a DataError.
  if (!RSA_check_key(rsa.get()))
    return Status::OperationError();

  // Create a corresponding EVP_PKEY.
  crypto::ScopedEVP_PKEY pkey(EVP_PKEY_new());
  if (!pkey || !EVP_PKEY_set1_RSA(pkey.get(), rsa.get()))
    return Status::OperationError();

  return CreateWebCryptoRsaPrivateKey(std::move(pkey), algorithm.id(),
                                      algorithm.rsaHashedImportParams()->hash(),
                                      extractable, usages, key);
}

Status ImportRsaPublicKey(const blink::WebCryptoAlgorithm& algorithm,
                          bool extractable,
                          blink::WebCryptoKeyUsageMask usages,
                          const CryptoData& n,
                          const CryptoData& e,
                          blink::WebCryptoKey* key) {
  crypto::ScopedRSA rsa(RSA_new());

  rsa->n = BN_bin2bn(n.bytes(), n.byte_length(), NULL);
  rsa->e = BN_bin2bn(e.bytes(), e.byte_length(), NULL);

  if (!rsa->n || !rsa->e)
    return Status::OperationError();

  // Create a corresponding EVP_PKEY.
  crypto::ScopedEVP_PKEY pkey(EVP_PKEY_new());
  if (!pkey || !EVP_PKEY_set1_RSA(pkey.get(), rsa.get()))
    return Status::OperationError();

  return CreateWebCryptoRsaPublicKey(std::move(pkey), algorithm.id(),
                                     algorithm.rsaHashedImportParams()->hash(),
                                     extractable, usages, key);
}

// Converts a BIGNUM to a big endian byte array.
std::vector<uint8_t> BIGNUMToVector(const BIGNUM* n) {
  std::vector<uint8_t> v(BN_num_bytes(n));
  BN_bn2bin(n, v.data());
  return v;
}

// Synthesizes an import algorithm given a key algorithm, so that
// deserialization can re-use the ImportKey*() methods.
blink::WebCryptoAlgorithm SynthesizeImportAlgorithmForClone(
    const blink::WebCryptoKeyAlgorithm& algorithm) {
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      algorithm.id(), new blink::WebCryptoRsaHashedImportParams(
                          algorithm.rsaHashedParams()->hash()));
}

}  // namespace

Status RsaHashedAlgorithm::GenerateKey(
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask combined_usages,
    GenerateKeyResult* result) const {
  blink::WebCryptoKeyUsageMask public_usages = 0;
  blink::WebCryptoKeyUsageMask private_usages = 0;

  Status status = GetUsagesForGenerateAsymmetricKey(
      combined_usages, all_public_key_usages_, all_private_key_usages_,
      &public_usages, &private_usages);
  if (status.IsError())
    return status;

  const blink::WebCryptoRsaHashedKeyGenParams* params =
      algorithm.rsaHashedKeyGenParams();

  unsigned int modulus_length_bits = params->modulusLengthBits();

  // Limit the RSA key sizes to:
  //   * Multiple of 8 bits
  //   * 256 bits to 16K bits
  //
  // These correspond with limitations at the time there was an NSS WebCrypto
  // implementation. However in practice the upper bound is also helpful
  // because generating large RSA keys is very slow.
  if (modulus_length_bits < 256 || modulus_length_bits > 16384 ||
      (modulus_length_bits % 8) != 0) {
    return Status::ErrorGenerateRsaUnsupportedModulus();
  }

  unsigned int public_exponent = 0;
  if (!params->convertPublicExponentToUnsigned(public_exponent))
    return Status::ErrorGenerateKeyPublicExponent();

  // OpenSSL hangs when given bad public exponents. Use a whitelist.
  if (public_exponent != 3 && public_exponent != 65537)
    return Status::ErrorGenerateKeyPublicExponent();

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  // Generate an RSA key pair.
  crypto::ScopedRSA rsa_private_key(RSA_new());
  crypto::ScopedBIGNUM bn(BN_new());
  if (!rsa_private_key.get() || !bn.get() ||
      !BN_set_word(bn.get(), public_exponent)) {
    return Status::OperationError();
  }

  if (!RSA_generate_key_ex(rsa_private_key.get(), modulus_length_bits, bn.get(),
                           NULL)) {
    return Status::OperationError();
  }

  // Construct an EVP_PKEY for the private key.
  crypto::ScopedEVP_PKEY private_pkey(EVP_PKEY_new());
  if (!private_pkey ||
      !EVP_PKEY_set1_RSA(private_pkey.get(), rsa_private_key.get())) {
    return Status::OperationError();
  }

  // Construct an EVP_PKEY for the public key.
  crypto::ScopedRSA rsa_public_key(RSAPublicKey_dup(rsa_private_key.get()));
  crypto::ScopedEVP_PKEY public_pkey(EVP_PKEY_new());
  if (!public_pkey ||
      !EVP_PKEY_set1_RSA(public_pkey.get(), rsa_public_key.get())) {
    return Status::OperationError();
  }

  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;

  // Note that extractable is unconditionally set to true. This is because per
  // the WebCrypto spec generated public keys are always extractable.
  status = CreateWebCryptoRsaPublicKey(std::move(public_pkey), algorithm.id(),
                                       params->hash(), true, public_usages,
                                       &public_key);
  if (status.IsError())
    return status;

  status = CreateWebCryptoRsaPrivateKey(std::move(private_pkey), algorithm.id(),
                                        params->hash(), extractable,
                                        private_usages, &private_key);
  if (status.IsError())
    return status;

  result->AssignKeyPair(public_key, private_key);
  return Status::Success();
}

Status RsaHashedAlgorithm::VerifyKeyUsagesBeforeImportKey(
    blink::WebCryptoKeyFormat format,
    blink::WebCryptoKeyUsageMask usages) const {
  return VerifyUsagesBeforeImportAsymmetricKey(format, all_public_key_usages_,
                                               all_private_key_usages_, usages);
}

Status RsaHashedAlgorithm::ImportKeyPkcs8(
    const CryptoData& key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  crypto::ScopedEVP_PKEY private_key;
  Status status =
      ImportUnverifiedPkeyFromPkcs8(key_data, EVP_PKEY_RSA, &private_key);
  if (status.IsError())
    return status;

  // Verify the parameters of the key.
  crypto::ScopedRSA rsa(EVP_PKEY_get1_RSA(private_key.get()));
  if (!rsa.get())
    return Status::ErrorUnexpected();
  if (!RSA_check_key(rsa.get()))
    return Status::DataError();

  // TODO(eroman): Validate the algorithm OID against the webcrypto provided
  // hash. http://crbug.com/389400

  return CreateWebCryptoRsaPrivateKey(std::move(private_key), algorithm.id(),
                                      algorithm.rsaHashedImportParams()->hash(),
                                      extractable, usages, key);
}

Status RsaHashedAlgorithm::ImportKeySpki(
    const CryptoData& key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  crypto::ScopedEVP_PKEY public_key;
  Status status =
      ImportUnverifiedPkeyFromSpki(key_data, EVP_PKEY_RSA, &public_key);
  if (status.IsError())
    return status;

  // TODO(eroman): Validate the algorithm OID against the webcrypto provided
  // hash. http://crbug.com/389400

  return CreateWebCryptoRsaPublicKey(std::move(public_key), algorithm.id(),
                                     algorithm.rsaHashedImportParams()->hash(),
                                     extractable, usages, key);
}

Status RsaHashedAlgorithm::ImportKeyJwk(
    const CryptoData& key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  const char* jwk_algorithm =
      GetJwkAlgorithm(algorithm.rsaHashedImportParams()->hash().id());

  if (!jwk_algorithm)
    return Status::ErrorUnexpected();

  JwkRsaInfo jwk;
  Status status =
      ReadRsaKeyJwk(key_data, jwk_algorithm, extractable, usages, &jwk);
  if (status.IsError())
    return status;

  // Once the key type is known, verify the usages.
  if (jwk.is_private_key) {
    status = CheckPrivateKeyCreationUsages(all_private_key_usages_, usages);
  } else {
    status = CheckPublicKeyCreationUsages(all_public_key_usages_, usages);
  }

  if (status.IsError())
    return status;

  return jwk.is_private_key
             ? ImportRsaPrivateKey(algorithm, extractable, usages, jwk, key)
             : ImportRsaPublicKey(algorithm, extractable, usages,
                                  CryptoData(jwk.n), CryptoData(jwk.e), key);
}

Status RsaHashedAlgorithm::ExportKeyPkcs8(const blink::WebCryptoKey& key,
                                          std::vector<uint8_t>* buffer) const {
  if (key.type() != blink::WebCryptoKeyTypePrivate)
    return Status::ErrorUnexpectedKeyType();
  // This relies on the fact that PKCS8 formatted data was already
  // associated with the key during its creation (used by
  // structured clone).
  *buffer = GetSerializedKeyData(key);
  return Status::Success();
}

Status RsaHashedAlgorithm::ExportKeySpki(const blink::WebCryptoKey& key,
                                         std::vector<uint8_t>* buffer) const {
  if (key.type() != blink::WebCryptoKeyTypePublic)
    return Status::ErrorUnexpectedKeyType();
  // This relies on the fact that SPKI formatted data was already
  // associated with the key during its creation (used by
  // structured clone).
  *buffer = GetSerializedKeyData(key);
  return Status::Success();
}

Status RsaHashedAlgorithm::ExportKeyJwk(const blink::WebCryptoKey& key,
                                        std::vector<uint8_t>* buffer) const {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  EVP_PKEY* pkey = GetEVP_PKEY(key);
  crypto::ScopedRSA rsa(EVP_PKEY_get1_RSA(pkey));
  if (!rsa.get())
    return Status::ErrorUnexpected();

  const char* jwk_algorithm =
      GetJwkAlgorithm(key.algorithm().rsaHashedParams()->hash().id());
  if (!jwk_algorithm)
    return Status::ErrorUnexpected();

  switch (key.type()) {
    case blink::WebCryptoKeyTypePublic: {
      JwkWriter writer(jwk_algorithm, key.extractable(), key.usages(), "RSA");
      writer.SetBytes("n", CryptoData(BIGNUMToVector(rsa->n)));
      writer.SetBytes("e", CryptoData(BIGNUMToVector(rsa->e)));
      writer.ToJson(buffer);
      return Status::Success();
    }
    case blink::WebCryptoKeyTypePrivate: {
      JwkWriter writer(jwk_algorithm, key.extractable(), key.usages(), "RSA");
      writer.SetBytes("n", CryptoData(BIGNUMToVector(rsa->n)));
      writer.SetBytes("e", CryptoData(BIGNUMToVector(rsa->e)));
      writer.SetBytes("d", CryptoData(BIGNUMToVector(rsa->d)));
      // Although these are "optional" in the JWA, WebCrypto spec requires them
      // to be emitted.
      writer.SetBytes("p", CryptoData(BIGNUMToVector(rsa->p)));
      writer.SetBytes("q", CryptoData(BIGNUMToVector(rsa->q)));
      writer.SetBytes("dp", CryptoData(BIGNUMToVector(rsa->dmp1)));
      writer.SetBytes("dq", CryptoData(BIGNUMToVector(rsa->dmq1)));
      writer.SetBytes("qi", CryptoData(BIGNUMToVector(rsa->iqmp)));
      writer.ToJson(buffer);
      return Status::Success();
    }

    default:
      return Status::ErrorUnexpected();
  }
}

// TODO(eroman): Defer import to the crypto thread. http://crbug.com/430763
Status RsaHashedAlgorithm::DeserializeKeyForClone(
    const blink::WebCryptoKeyAlgorithm& algorithm,
    blink::WebCryptoKeyType type,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    const CryptoData& key_data,
    blink::WebCryptoKey* key) const {
  blink::WebCryptoAlgorithm import_algorithm =
      SynthesizeImportAlgorithmForClone(algorithm);

  Status status;

  // The serialized data will be either SPKI or PKCS8 formatted.
  switch (type) {
    case blink::WebCryptoKeyTypePublic:
      status =
          ImportKeySpki(key_data, import_algorithm, extractable, usages, key);
      break;
    case blink::WebCryptoKeyTypePrivate:
      status =
          ImportKeyPkcs8(key_data, import_algorithm, extractable, usages, key);
      break;
    default:
      return Status::ErrorUnexpected();
  }

  // There is some duplicated information in the serialized format used by
  // structured clone (since the KeyAlgorithm is serialized separately from the
  // key data). Use this extra information to further validate what was
  // deserialized from the key data.

  if (algorithm.id() != key->algorithm().id())
    return Status::ErrorUnexpected();

  if (key->type() != type)
    return Status::ErrorUnexpected();

  if (algorithm.rsaHashedParams()->modulusLengthBits() !=
      key->algorithm().rsaHashedParams()->modulusLengthBits()) {
    return Status::ErrorUnexpected();
  }

  if (algorithm.rsaHashedParams()->publicExponent().size() !=
          key->algorithm().rsaHashedParams()->publicExponent().size() ||
      0 !=
          memcmp(algorithm.rsaHashedParams()->publicExponent().data(),
                 key->algorithm().rsaHashedParams()->publicExponent().data(),
                 key->algorithm().rsaHashedParams()->publicExponent().size())) {
    return Status::ErrorUnexpected();
  }

  return Status::Success();
}

}  // namespace webcrypto
