// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/algorithms/ec.h"

#include <openssl/ec.h>
#include <openssl/ec_key.h>
#include <openssl/evp.h>
#include <stddef.h>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
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

// Maps a blink::WebCryptoNamedCurve to the corresponding NID used by
// BoringSSL.
Status WebCryptoCurveToNid(blink::WebCryptoNamedCurve named_curve, int* nid) {
  switch (named_curve) {
    case blink::WebCryptoNamedCurveP256:
      *nid = NID_X9_62_prime256v1;
      return Status::Success();
    case blink::WebCryptoNamedCurveP384:
      *nid = NID_secp384r1;
      return Status::Success();
    case blink::WebCryptoNamedCurveP521:
      *nid = NID_secp521r1;
      return Status::Success();
  }
  return Status::ErrorUnsupported();
}

// Maps a BoringSSL NID to the corresponding WebCrypto named curve.
Status NidToWebCryptoCurve(int nid, blink::WebCryptoNamedCurve* named_curve) {
  switch (nid) {
    case NID_X9_62_prime256v1:
      *named_curve = blink::WebCryptoNamedCurveP256;
      return Status::Success();
    case NID_secp384r1:
      *named_curve = blink::WebCryptoNamedCurveP384;
      return Status::Success();
    case NID_secp521r1:
      *named_curve = blink::WebCryptoNamedCurveP521;
      return Status::Success();
  }
  return Status::ErrorImportedEcKeyIncorrectCurve();
}

struct JwkCrvMapping {
  const char* jwk_curve;
  blink::WebCryptoNamedCurve named_curve;
};

const JwkCrvMapping kJwkCrvMappings[] = {
    {"P-256", blink::WebCryptoNamedCurveP256},
    {"P-384", blink::WebCryptoNamedCurveP384},
    {"P-521", blink::WebCryptoNamedCurveP521},
};

// Gets the "crv" parameter from a JWK and converts it to a WebCryptoNamedCurve.
Status ReadJwkCrv(const JwkReader& jwk,
                  blink::WebCryptoNamedCurve* named_curve) {
  std::string jwk_curve;
  Status status = jwk.GetString("crv", &jwk_curve);
  if (status.IsError())
    return status;

  for (size_t i = 0; i < arraysize(kJwkCrvMappings); ++i) {
    if (kJwkCrvMappings[i].jwk_curve == jwk_curve) {
      *named_curve = kJwkCrvMappings[i].named_curve;
      return Status::Success();
    }
  }

  return Status::ErrorJwkIncorrectCrv();
}

// Converts a WebCryptoNamedCurve to an equivalent JWK "crv".
Status WebCryptoCurveToJwkCrv(blink::WebCryptoNamedCurve named_curve,
                              std::string* jwk_crv) {
  for (size_t i = 0; i < arraysize(kJwkCrvMappings); ++i) {
    if (kJwkCrvMappings[i].named_curve == named_curve) {
      *jwk_crv = kJwkCrvMappings[i].jwk_curve;
      return Status::Success();
    }
  }
  return Status::ErrorUnexpected();
}

// Verifies that an EC key imported from PKCS8 or SPKI format is correct.
// This involves verifying the key validity, and the NID for the named curve.
// Also removes the EC_PKEY_NO_PUBKEY flag if present.
Status VerifyEcKeyAfterSpkiOrPkcs8Import(
    EVP_PKEY* pkey,
    blink::WebCryptoNamedCurve expected_named_curve) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  crypto::ScopedEC_KEY ec(EVP_PKEY_get1_EC_KEY(pkey));
  if (!ec.get())
    return Status::ErrorUnexpected();

  // When importing an ECPrivateKey, the public key is optional. If it was
  // omitted then the public key will be calculated by BoringSSL and added into
  // the EC_KEY. However an encoding flag is set such that when exporting to
  // PKCS8 format the public key is once again omitted. Remove this flag.
  unsigned int enc_flags = EC_KEY_get_enc_flags(ec.get());
  enc_flags &= ~EC_PKEY_NO_PUBKEY;
  EC_KEY_set_enc_flags(ec.get(), enc_flags);

  if (!EC_KEY_check_key(ec.get()))
    return Status::ErrorEcKeyInvalid();

  // Make sure the curve matches the expected curve name.
  int curve_nid = EC_GROUP_get_curve_name(EC_KEY_get0_group(ec.get()));
  blink::WebCryptoNamedCurve named_curve = blink::WebCryptoNamedCurveP256;
  Status status = NidToWebCryptoCurve(curve_nid, &named_curve);
  if (status.IsError())
    return status;

  if (named_curve != expected_named_curve)
    return Status::ErrorImportedEcKeyIncorrectCurve();

  return Status::Success();
}

// Creates an EC_KEY for the given WebCryptoNamedCurve.
Status CreateEC_KEY(blink::WebCryptoNamedCurve named_curve,
                    crypto::ScopedEC_KEY* ec) {
  int curve_nid = 0;
  Status status = WebCryptoCurveToNid(named_curve, &curve_nid);
  if (status.IsError())
    return status;

  ec->reset(EC_KEY_new_by_curve_name(curve_nid));
  if (!ec->get())
    return Status::OperationError();

  return Status::Success();
}

// Writes an unsigned BIGNUM into |jwk|, zero-padding it to a length of
// |padded_length|.
Status WritePaddedBIGNUM(const std::string& member_name,
                         const BIGNUM* value,
                         size_t padded_length,
                         JwkWriter* jwk) {
  std::vector<uint8_t> padded_bytes(padded_length);
  if (!BN_bn2bin_padded(padded_bytes.data(), padded_bytes.size(), value))
    return Status::OperationError();
  jwk->SetBytes(member_name, CryptoData(padded_bytes));
  return Status::Success();
}

// Reads a fixed length BIGNUM from a JWK.
Status ReadPaddedBIGNUM(const JwkReader& jwk,
                        const std::string& member_name,
                        size_t expected_length,
                        crypto::ScopedBIGNUM* out) {
  std::string bytes;
  Status status = jwk.GetBytes(member_name, &bytes);
  if (status.IsError())
    return status;

  if (bytes.size() != expected_length) {
    return Status::JwkOctetStringWrongLength(member_name, expected_length,
                                             bytes.size());
  }

  out->reset(CreateBIGNUM(bytes));
  return Status::Success();
}

int GetGroupDegreeInBytes(EC_KEY* ec) {
  const EC_GROUP* group = EC_KEY_get0_group(ec);
  return NumBitsToBytes(EC_GROUP_get_degree(group));
}

// Extracts the public key as affine coordinates (x,y).
Status GetPublicKey(EC_KEY* ec,
                    crypto::ScopedBIGNUM* x,
                    crypto::ScopedBIGNUM* y) {
  const EC_GROUP* group = EC_KEY_get0_group(ec);
  const EC_POINT* point = EC_KEY_get0_public_key(ec);

  x->reset(BN_new());
  y->reset(BN_new());

  if (!EC_POINT_get_affine_coordinates_GFp(group, point, x->get(), y->get(),
                                           NULL)) {
    return Status::OperationError();
  }

  return Status::Success();
}

// Synthesizes an import algorithm given a key algorithm, so that
// deserialization can re-use the ImportKey*() methods.
blink::WebCryptoAlgorithm SynthesizeImportAlgorithmForClone(
    const blink::WebCryptoKeyAlgorithm& algorithm) {
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      algorithm.id(), new blink::WebCryptoEcKeyImportParams(
                          algorithm.ecParams()->namedCurve()));
}

}  // namespace

Status EcAlgorithm::GenerateKey(const blink::WebCryptoAlgorithm& algorithm,
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

  const blink::WebCryptoEcKeyGenParams* params = algorithm.ecKeyGenParams();

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  // Generate an EC key pair.
  crypto::ScopedEC_KEY ec_private_key;
  status = CreateEC_KEY(params->namedCurve(), &ec_private_key);
  if (status.IsError())
    return status;

  if (!EC_KEY_generate_key(ec_private_key.get()))
    return Status::OperationError();

  // Construct an EVP_PKEY for the private key.
  crypto::ScopedEVP_PKEY private_pkey(EVP_PKEY_new());
  if (!private_pkey ||
      !EVP_PKEY_set1_EC_KEY(private_pkey.get(), ec_private_key.get())) {
    return Status::OperationError();
  }

  // Construct an EVP_PKEY for just the public key.
  crypto::ScopedEC_KEY ec_public_key;
  crypto::ScopedEVP_PKEY public_pkey(EVP_PKEY_new());
  status = CreateEC_KEY(params->namedCurve(), &ec_public_key);
  if (status.IsError())
    return status;
  if (!EC_KEY_set_public_key(ec_public_key.get(),
                             EC_KEY_get0_public_key(ec_private_key.get()))) {
    return Status::OperationError();
  }
  if (!public_pkey ||
      !EVP_PKEY_set1_EC_KEY(public_pkey.get(), ec_public_key.get())) {
    return Status::OperationError();
  }

  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;

  blink::WebCryptoKeyAlgorithm key_algorithm =
      blink::WebCryptoKeyAlgorithm::createEc(algorithm.id(),
                                             params->namedCurve());

  // Note that extractable is unconditionally set to true. This is because per
  // the WebCrypto spec generated public keys are always extractable.
  status = CreateWebCryptoPublicKey(std::move(public_pkey), key_algorithm, true,
                                    public_usages, &public_key);
  if (status.IsError())
    return status;

  status = CreateWebCryptoPrivateKey(std::move(private_pkey), key_algorithm,
                                     extractable, private_usages, &private_key);
  if (status.IsError())
    return status;

  result->AssignKeyPair(public_key, private_key);
  return Status::Success();
}

Status EcAlgorithm::VerifyKeyUsagesBeforeImportKey(
    blink::WebCryptoKeyFormat format,
    blink::WebCryptoKeyUsageMask usages) const {
  return VerifyUsagesBeforeImportAsymmetricKey(format, all_public_key_usages_,
                                               all_private_key_usages_, usages);
}

Status EcAlgorithm::ImportKeyPkcs8(const CryptoData& key_data,
                                   const blink::WebCryptoAlgorithm& algorithm,
                                   bool extractable,
                                   blink::WebCryptoKeyUsageMask usages,
                                   blink::WebCryptoKey* key) const {
  crypto::ScopedEVP_PKEY private_key;
  Status status =
      ImportUnverifiedPkeyFromPkcs8(key_data, EVP_PKEY_EC, &private_key);
  if (status.IsError())
    return status;

  const blink::WebCryptoEcKeyImportParams* params =
      algorithm.ecKeyImportParams();

  status = VerifyEcKeyAfterSpkiOrPkcs8Import(private_key.get(),
                                             params->namedCurve());
  if (status.IsError())
    return status;

  return CreateWebCryptoPrivateKey(std::move(private_key),
                                   blink::WebCryptoKeyAlgorithm::createEc(
                                       algorithm.id(), params->namedCurve()),
                                   extractable, usages, key);
}

Status EcAlgorithm::ImportKeySpki(const CryptoData& key_data,
                                  const blink::WebCryptoAlgorithm& algorithm,
                                  bool extractable,
                                  blink::WebCryptoKeyUsageMask usages,
                                  blink::WebCryptoKey* key) const {
  crypto::ScopedEVP_PKEY public_key;
  Status status =
      ImportUnverifiedPkeyFromSpki(key_data, EVP_PKEY_EC, &public_key);
  if (status.IsError())
    return status;

  const blink::WebCryptoEcKeyImportParams* params =
      algorithm.ecKeyImportParams();

  status =
      VerifyEcKeyAfterSpkiOrPkcs8Import(public_key.get(), params->namedCurve());
  if (status.IsError())
    return status;

  return CreateWebCryptoPublicKey(std::move(public_key),
                                  blink::WebCryptoKeyAlgorithm::createEc(
                                      algorithm.id(), params->namedCurve()),
                                  extractable, usages, key);
}

// The format for JWK EC keys is given by:
// https://tools.ietf.org/html/draft-ietf-jose-json-web-algorithms-36#section-6.2
Status EcAlgorithm::ImportKeyJwk(const CryptoData& key_data,
                                 const blink::WebCryptoAlgorithm& algorithm,
                                 bool extractable,
                                 blink::WebCryptoKeyUsageMask usages,
                                 blink::WebCryptoKey* key) const {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  const blink::WebCryptoEcKeyImportParams* params =
      algorithm.ecKeyImportParams();

  // When importing EC keys from JWK there may be up to *three* separate curve
  // names:
  //
  //   (1) The one given to WebCrypto's importKey (params->namedCurve()).
  //   (2) JWK's "crv" member
  //   (3) A curve implied by JWK's "alg" member.
  //
  // (In the case of ECDSA, the "alg" member implicitly names a curve and hash)

  JwkReader jwk;
  Status status = jwk.Init(key_data, extractable, usages, "EC",
                           GetJwkAlgorithm(params->namedCurve()));
  if (status.IsError())
    return status;

  // Verify that "crv" matches expected curve.
  blink::WebCryptoNamedCurve jwk_crv = blink::WebCryptoNamedCurveP256;
  status = ReadJwkCrv(jwk, &jwk_crv);
  if (status.IsError())
    return status;
  if (jwk_crv != params->namedCurve())
    return Status::ErrorJwkIncorrectCrv();

  // Only private keys have a "d" parameter. The key may still be invalid, but
  // tentatively decide if it is a public or private key.
  bool is_private_key = jwk.HasMember("d");

  // Now that the key type is known, verify the usages.
  if (is_private_key) {
    status = CheckPrivateKeyCreationUsages(all_private_key_usages_, usages);
  } else {
    status = CheckPublicKeyCreationUsages(all_public_key_usages_, usages);
  }

  if (status.IsError())
    return status;

  // Create an EC_KEY.
  crypto::ScopedEC_KEY ec;
  status = CreateEC_KEY(params->namedCurve(), &ec);
  if (status.IsError())
    return status;

  // JWK requires the length of x, y, d to match the group degree.
  int degree_bytes = GetGroupDegreeInBytes(ec.get());

  // Read the public key's uncompressed affine coordinates.
  crypto::ScopedBIGNUM x;
  status = ReadPaddedBIGNUM(jwk, "x", degree_bytes, &x);
  if (status.IsError())
    return status;

  crypto::ScopedBIGNUM y;
  status = ReadPaddedBIGNUM(jwk, "y", degree_bytes, &y);
  if (status.IsError())
    return status;

  // TODO(eroman): Distinguish more accurately between a DataError and
  // OperationError. In general if this fails it was due to the key being an
  // invalid EC key.
  if (!EC_KEY_set_public_key_affine_coordinates(ec.get(), x.get(), y.get()))
    return Status::DataError();

  // Extract the "d" parameters.
  if (is_private_key) {
    crypto::ScopedBIGNUM d;
    status = ReadPaddedBIGNUM(jwk, "d", degree_bytes, &d);
    if (status.IsError())
      return status;

    if (!EC_KEY_set_private_key(ec.get(), d.get()))
      return Status::OperationError();
  }

  // Verify the key.
  if (!EC_KEY_check_key(ec.get()))
    return Status::ErrorEcKeyInvalid();

  // Wrap the EC_KEY into an EVP_PKEY.
  crypto::ScopedEVP_PKEY pkey(EVP_PKEY_new());
  if (!pkey || !EVP_PKEY_set1_EC_KEY(pkey.get(), ec.get()))
    return Status::OperationError();

  blink::WebCryptoKeyAlgorithm key_algorithm =
      blink::WebCryptoKeyAlgorithm::createEc(algorithm.id(),
                                             params->namedCurve());

  // Wrap the EVP_PKEY into a WebCryptoKey
  if (is_private_key) {
    return CreateWebCryptoPrivateKey(std::move(pkey), key_algorithm,
                                     extractable, usages, key);
  }
  return CreateWebCryptoPublicKey(std::move(pkey), key_algorithm, extractable,
                                  usages, key);
}

Status EcAlgorithm::ExportKeyPkcs8(const blink::WebCryptoKey& key,
                                   std::vector<uint8_t>* buffer) const {
  if (key.type() != blink::WebCryptoKeyTypePrivate)
    return Status::ErrorUnexpectedKeyType();
  // This relies on the fact that PKCS8 formatted data was already
  // associated with the key during its creation (used by
  // structured clone).
  *buffer = GetSerializedKeyData(key);
  return Status::Success();
}

Status EcAlgorithm::ExportKeySpki(const blink::WebCryptoKey& key,
                                  std::vector<uint8_t>* buffer) const {
  if (key.type() != blink::WebCryptoKeyTypePublic)
    return Status::ErrorUnexpectedKeyType();
  // This relies on the fact that SPKI formatted data was already
  // associated with the key during its creation (used by
  // structured clone).
  *buffer = GetSerializedKeyData(key);
  return Status::Success();
}

// The format for JWK EC keys is given by:
// https://tools.ietf.org/html/draft-ietf-jose-json-web-algorithms-36#section-6.2
Status EcAlgorithm::ExportKeyJwk(const blink::WebCryptoKey& key,
                                 std::vector<uint8_t>* buffer) const {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  EVP_PKEY* pkey = GetEVP_PKEY(key);

  crypto::ScopedEC_KEY ec(EVP_PKEY_get1_EC_KEY(pkey));
  if (!ec.get())
    return Status::ErrorUnexpected();

  // No "alg" is set for EC keys.
  JwkWriter jwk(std::string(), key.extractable(), key.usages(), "EC");

  // Set the crv
  std::string crv;
  Status status =
      WebCryptoCurveToJwkCrv(key.algorithm().ecParams()->namedCurve(), &crv);
  if (status.IsError())
    return status;

  int degree_bytes = GetGroupDegreeInBytes(ec.get());

  jwk.SetString("crv", crv);

  crypto::ScopedBIGNUM x;
  crypto::ScopedBIGNUM y;
  status = GetPublicKey(ec.get(), &x, &y);
  if (status.IsError())
    return status;

  status = WritePaddedBIGNUM("x", x.get(), degree_bytes, &jwk);
  if (status.IsError())
    return status;

  status = WritePaddedBIGNUM("y", y.get(), degree_bytes, &jwk);
  if (status.IsError())
    return status;

  if (key.type() == blink::WebCryptoKeyTypePrivate) {
    const BIGNUM* d = EC_KEY_get0_private_key(ec.get());
    status = WritePaddedBIGNUM("d", d, degree_bytes, &jwk);
    if (status.IsError())
      return status;
  }

  jwk.ToJson(buffer);
  return Status::Success();
}

// TODO(eroman): Defer import to the crypto thread. http://crbug.com/430763
Status EcAlgorithm::DeserializeKeyForClone(
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

  if (type != key->type())
    return Status::ErrorUnexpected();

  if (algorithm.ecParams()->namedCurve() !=
      key->algorithm().ecParams()->namedCurve()) {
    return Status::ErrorUnexpected();
  }

  return Status::Success();
}

}  // namespace webcrypto
