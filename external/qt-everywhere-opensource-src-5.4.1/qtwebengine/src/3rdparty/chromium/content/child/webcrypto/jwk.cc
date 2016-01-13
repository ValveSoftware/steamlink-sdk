// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jwk.h"

#include <algorithm>
#include <functional>
#include <map>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/platform_crypto.h"
#include "content/child/webcrypto/shared_crypto.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

// JSON Web Key Format (JWK)
// http://tools.ietf.org/html/draft-ietf-jose-json-web-key-21
//
// A JWK is a simple JSON dictionary with the following entries
// - "kty" (Key Type) Parameter, REQUIRED
// - <kty-specific parameters, see below>, REQUIRED
// - "use" (Key Use) Parameter, OPTIONAL
// - "key_ops" (Key Operations) Parameter, OPTIONAL
// - "alg" (Algorithm) Parameter, OPTIONAL
// - "ext" (Key Exportability), OPTIONAL
// (all other entries are ignored)
//
// OPTIONAL here means that this code does not require the entry to be present
// in the incoming JWK, because the method input parameters contain similar
// information. If the optional JWK entry is present, it will be validated
// against the corresponding input parameter for consistency and combined with
// it according to rules defined below.
//
// Input 'key_data' contains the JWK. To build a Web Crypto Key, the JWK
// values are parsed out and combined with the method input parameters to
// build a Web Crypto Key:
// Web Crypto Key type            <-- (deduced)
// Web Crypto Key extractable     <-- JWK ext + input extractable
// Web Crypto Key algorithm       <-- JWK alg + input algorithm
// Web Crypto Key keyUsage        <-- JWK use, key_ops + input usage_mask
// Web Crypto Key keying material <-- kty-specific parameters
//
// Values for each JWK entry are case-sensitive and defined in
// http://tools.ietf.org/html/draft-ietf-jose-json-web-algorithms-18.
// Note that not all values specified by JOSE are handled by this code. Only
// handled values are listed.
// - kty (Key Type)
//   +-------+--------------------------------------------------------------+
//   | "RSA" | RSA [RFC3447]                                                |
//   | "oct" | Octet sequence (used to represent symmetric keys)            |
//   +-------+--------------------------------------------------------------+
//
// - key_ops (Key Use Details)
//   The key_ops field is an array that contains one or more strings from
//   the table below, and describes the operations for which this key may be
//   used.
//   +-------+--------------------------------------------------------------+
//   | "encrypt"    | encrypt operations                                    |
//   | "decrypt"    | decrypt operations                                    |
//   | "sign"       | sign (MAC) operations                                 |
//   | "verify"     | verify (MAC) operations                               |
//   | "wrapKey"    | key wrap                                              |
//   | "unwrapKey"  | key unwrap                                            |
//   | "deriveKey"  | key derivation                                        |
//   | "deriveBits" | key derivation                                        |
//   +-------+--------------------------------------------------------------+
//
// - use (Key Use)
//   The use field contains a single entry from the table below.
//   +-------+--------------------------------------------------------------+
//   | "sig"     | equivalent to key_ops of [sign, verify]                  |
//   | "enc"     | equivalent to key_ops of [encrypt, decrypt, wrapKey,     |
//   |           | unwrapKey, deriveKey, deriveBits]                        |
//   +-------+--------------------------------------------------------------+
//
//   NOTE: If both "use" and "key_ops" JWK members are present, the usages
//   specified by them MUST be consistent.  In particular, the "use" value
//   "sig" corresponds to "sign" and/or "verify".  The "use" value "enc"
//   corresponds to all other values defined above.  If "key_ops" values
//   corresponding to both "sig" and "enc" "use" values are present, the "use"
//   member SHOULD NOT be present, and if present, its value MUST NOT be
//   either "sig" or "enc".
//
// - ext (Key Exportability)
//   +-------+--------------------------------------------------------------+
//   | true  | Key may be exported from the trusted environment             |
//   | false | Key cannot exit the trusted environment                      |
//   +-------+--------------------------------------------------------------+
//
// - alg (Algorithm)
//   See http://tools.ietf.org/html/draft-ietf-jose-json-web-algorithms-18
//   +--------------+-------------------------------------------------------+
//   | Digital Signature or MAC Algorithm                                   |
//   +--------------+-------------------------------------------------------+
//   | "HS1"        | HMAC using SHA-1 hash algorithm                       |
//   | "HS256"      | HMAC using SHA-256 hash algorithm                     |
//   | "HS384"      | HMAC using SHA-384 hash algorithm                     |
//   | "HS512"      | HMAC using SHA-512 hash algorithm                     |
//   | "RS1"        | RSASSA using SHA-1 hash algorithm
//   | "RS256"      | RSASSA using SHA-256 hash algorithm                   |
//   | "RS384"      | RSASSA using SHA-384 hash algorithm                   |
//   | "RS512"      | RSASSA using SHA-512 hash algorithm                   |
//   +--------------+-------------------------------------------------------|
//   | Key Management Algorithm                                             |
//   +--------------+-------------------------------------------------------+
//   | "RSA-OAEP"   | RSAES using Optimal Asymmetric Encryption Padding     |
//   |              | (OAEP) [RFC3447], with the default parameters         |
//   |              | specified by RFC3447 in Section A.2.1                 |
//   | "A128KW"     | Advanced Encryption Standard (AES) Key Wrap Algorithm |
//   |              | [RFC3394] using 128 bit keys                          |
//   | "A192KW"     | AES Key Wrap Algorithm using 192 bit keys             |
//   | "A256KW"     | AES Key Wrap Algorithm using 256 bit keys             |
//   | "A128GCM"    | AES in Galois/Counter Mode (GCM) [NIST.800-38D] using |
//   |              | 128 bit keys                                          |
//   | "A192GCM"    | AES GCM using 192 bit keys                            |
//   | "A256GCM"    | AES GCM using 256 bit keys                            |
//   | "A128CBC"    | AES in Cipher Block Chaining Mode (CBC) with PKCS #5  |
//   |              | padding [NIST.800-38A]                                |
//   | "A192CBC"    | AES CBC using 192 bit keys                            |
//   | "A256CBC"    | AES CBC using 256 bit keys                            |
//   +--------------+-------------------------------------------------------+
//
// kty-specific parameters
// The value of kty determines the type and content of the keying material
// carried in the JWK to be imported.
// // - kty == "oct" (symmetric or other raw key)
//   +-------+--------------------------------------------------------------+
//   | "k"   | Contains the value of the symmetric (or other single-valued) |
//   |       | key.  It is represented as the base64url encoding of the     |
//   |       | octet sequence containing the key value.                     |
//   +-------+--------------------------------------------------------------+
// - kty == "RSA" (RSA public key)
//   +-------+--------------------------------------------------------------+
//   | "n"   | Contains the modulus value for the RSA public key.  It is    |
//   |       | represented as the base64url encoding of the value's         |
//   |       | unsigned big endian representation as an octet sequence.     |
//   +-------+--------------------------------------------------------------+
//   | "e"   | Contains the exponent value for the RSA public key.  It is   |
//   |       | represented as the base64url encoding of the value's         |
//   |       | unsigned big endian representation as an octet sequence.     |
//   +-------+--------------------------------------------------------------+
// - If key == "RSA" and the "d" parameter is present then it is a private key.
//   All the parameters above for public keys apply, as well as the following.
//   (Note that except for "d", all of these are optional):
//   +-------+--------------------------------------------------------------+
//   | "d"   | Contains the private exponent value for the RSA private key. |
//   |       | It is represented as the base64url encoding of the value's   |
//   |       | unsigned big endian representation as an octet sequence.     |
//   +-------+--------------------------------------------------------------+
//   | "p"   | Contains the first prime factor value for the RSA private    |
//   |       | key.  It is represented as the base64url encoding of the     |
//   |       | value's                                                      |
//   |       | unsigned big endian representation as an octet sequence.     |
//   +-------+--------------------------------------------------------------+
//   | "q"   | Contains the second prime factor value for the RSA private   |
//   |       | key.  It is represented as the base64url encoding of the     |
//   |       | value's unsigned big endian representation as an octet       |
//   |       | sequence.                                                    |
//   +-------+--------------------------------------------------------------+
//   | "dp"  | Contains the first factor CRT exponent value for the RSA     |
//   |       | private key.  It is represented as the base64url encoding of |
//   |       | the value's unsigned big endian representation as an octet   |
//   |       | sequence.                                                    |
//   +-------+--------------------------------------------------------------+
//   | "dq"  | Contains the second factor CRT exponent value for the RSA    |
//   |       | private key.  It is represented as the base64url encoding of |
//   |       | the value's unsigned big endian representation as an octet   |
//   |       | sequence.                                                    |
//   +-------+--------------------------------------------------------------+
//   | "dq"  | Contains the first CRT coefficient value for the RSA private |
//   |       | key.  It is represented as the base64url encoding of the     |
//   |       | value's unsigned big endian representation as an octet       |
//   |       | sequence.                                                    |
//   +-------+--------------------------------------------------------------+
//
// Consistency and conflict resolution
// The 'algorithm', 'extractable', and 'usage_mask' input parameters
// may be different than the corresponding values inside the JWK. The Web
// Crypto spec says that if a JWK value is present but is inconsistent with
// the input value, it is an error and the operation must fail. If no
// inconsistency is found then the input parameters are used.
//
// algorithm
//   If the JWK algorithm is provided, it must match the web crypto input
//   algorithm (both the algorithm ID and inner hash if applicable).
//
// extractable
//   If the JWK ext field is true but the input parameter is false, make the
//   Web Crypto Key non-extractable. Conversely, if the JWK ext field is
//   false but the input parameter is true, it is an inconsistency. If both
//   are true or both are false, use that value.
//
// usage_mask
//   The input usage_mask must be a strict subset of the interpreted JWK use
//   value, else it is judged inconsistent. In all cases the input usage_mask
//   is used as the final usage_mask.
//

namespace content {

namespace webcrypto {

namespace {

// Creates an RSASSA-PKCS1-v1_5 algorithm. It is an error to call this with a
// hash_id that is not a SHA*.
blink::WebCryptoAlgorithm CreateRsaSsaImportAlgorithm(
    blink::WebCryptoAlgorithmId hash_id) {
  return CreateRsaHashedImportAlgorithm(
      blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5, hash_id);
}

// Creates an RSA-OAEP algorithm. It is an error to call this with a hash_id
// that is not a SHA*.
blink::WebCryptoAlgorithm CreateRsaOaepImportAlgorithm(
    blink::WebCryptoAlgorithmId hash_id) {
  return CreateRsaHashedImportAlgorithm(blink::WebCryptoAlgorithmIdRsaOaep,
                                        hash_id);
}

// Web Crypto equivalent usage mask for JWK 'use' = 'enc'.
const blink::WebCryptoKeyUsageMask kJwkEncUsage =
    blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt |
    blink::WebCryptoKeyUsageWrapKey | blink::WebCryptoKeyUsageUnwrapKey |
    blink::WebCryptoKeyUsageDeriveKey | blink::WebCryptoKeyUsageDeriveBits;
// Web Crypto equivalent usage mask for JWK 'use' = 'sig'.
const blink::WebCryptoKeyUsageMask kJwkSigUsage =
    blink::WebCryptoKeyUsageSign | blink::WebCryptoKeyUsageVerify;

typedef blink::WebCryptoAlgorithm (*AlgorithmCreationFunc)();

class JwkAlgorithmInfo {
 public:
  JwkAlgorithmInfo()
      : creation_func_(NULL),
        required_key_length_bytes_(NO_KEY_SIZE_REQUIREMENT) {}

  explicit JwkAlgorithmInfo(AlgorithmCreationFunc algorithm_creation_func)
      : creation_func_(algorithm_creation_func),
        required_key_length_bytes_(NO_KEY_SIZE_REQUIREMENT) {}

  JwkAlgorithmInfo(AlgorithmCreationFunc algorithm_creation_func,
                   unsigned int required_key_length_bits)
      : creation_func_(algorithm_creation_func),
        required_key_length_bytes_(required_key_length_bits / 8) {
    DCHECK_EQ(0u, required_key_length_bits % 8);
  }

  bool CreateImportAlgorithm(blink::WebCryptoAlgorithm* algorithm) const {
    *algorithm = creation_func_();
    return !algorithm->isNull();
  }

  bool IsInvalidKeyByteLength(size_t byte_length) const {
    if (required_key_length_bytes_ == NO_KEY_SIZE_REQUIREMENT)
      return false;
    return required_key_length_bytes_ != byte_length;
  }

 private:
  enum { NO_KEY_SIZE_REQUIREMENT = UINT_MAX };

  AlgorithmCreationFunc creation_func_;

  // The expected key size for the algorithm or NO_KEY_SIZE_REQUIREMENT.
  unsigned int required_key_length_bytes_;
};

typedef std::map<std::string, JwkAlgorithmInfo> JwkAlgorithmInfoMap;

class JwkAlgorithmRegistry {
 public:
  JwkAlgorithmRegistry() {
    // TODO(eroman):
    // http://tools.ietf.org/html/draft-ietf-jose-json-web-algorithms-20
    // says HMAC with SHA-2 should have a key size at least as large as the
    // hash output.
    alg_to_info_["HS1"] =
        JwkAlgorithmInfo(&BindAlgorithmId<CreateHmacImportAlgorithm,
                                          blink::WebCryptoAlgorithmIdSha1>);
    alg_to_info_["HS256"] =
        JwkAlgorithmInfo(&BindAlgorithmId<CreateHmacImportAlgorithm,
                                          blink::WebCryptoAlgorithmIdSha256>);
    alg_to_info_["HS384"] =
        JwkAlgorithmInfo(&BindAlgorithmId<CreateHmacImportAlgorithm,
                                          blink::WebCryptoAlgorithmIdSha384>);
    alg_to_info_["HS512"] =
        JwkAlgorithmInfo(&BindAlgorithmId<CreateHmacImportAlgorithm,
                                          blink::WebCryptoAlgorithmIdSha512>);
    alg_to_info_["RS1"] =
        JwkAlgorithmInfo(&BindAlgorithmId<CreateRsaSsaImportAlgorithm,
                                          blink::WebCryptoAlgorithmIdSha1>);
    alg_to_info_["RS256"] =
        JwkAlgorithmInfo(&BindAlgorithmId<CreateRsaSsaImportAlgorithm,
                                          blink::WebCryptoAlgorithmIdSha256>);
    alg_to_info_["RS384"] =
        JwkAlgorithmInfo(&BindAlgorithmId<CreateRsaSsaImportAlgorithm,
                                          blink::WebCryptoAlgorithmIdSha384>);
    alg_to_info_["RS512"] =
        JwkAlgorithmInfo(&BindAlgorithmId<CreateRsaSsaImportAlgorithm,
                                          blink::WebCryptoAlgorithmIdSha512>);
    alg_to_info_["RSA-OAEP"] =
        JwkAlgorithmInfo(&BindAlgorithmId<CreateRsaOaepImportAlgorithm,
                                          blink::WebCryptoAlgorithmIdSha1>);
    alg_to_info_["RSA-OAEP-256"] =
        JwkAlgorithmInfo(&BindAlgorithmId<CreateRsaOaepImportAlgorithm,
                                          blink::WebCryptoAlgorithmIdSha256>);
    alg_to_info_["RSA-OAEP-384"] =
        JwkAlgorithmInfo(&BindAlgorithmId<CreateRsaOaepImportAlgorithm,
                                          blink::WebCryptoAlgorithmIdSha384>);
    alg_to_info_["RSA-OAEP-512"] =
        JwkAlgorithmInfo(&BindAlgorithmId<CreateRsaOaepImportAlgorithm,
                                          blink::WebCryptoAlgorithmIdSha512>);
    alg_to_info_["A128KW"] = JwkAlgorithmInfo(
        &BindAlgorithmId<CreateAlgorithm, blink::WebCryptoAlgorithmIdAesKw>,
        128);
    alg_to_info_["A192KW"] = JwkAlgorithmInfo(
        &BindAlgorithmId<CreateAlgorithm, blink::WebCryptoAlgorithmIdAesKw>,
        192);
    alg_to_info_["A256KW"] = JwkAlgorithmInfo(
        &BindAlgorithmId<CreateAlgorithm, blink::WebCryptoAlgorithmIdAesKw>,
        256);
    alg_to_info_["A128GCM"] = JwkAlgorithmInfo(
        &BindAlgorithmId<CreateAlgorithm, blink::WebCryptoAlgorithmIdAesGcm>,
        128);
    alg_to_info_["A192GCM"] = JwkAlgorithmInfo(
        &BindAlgorithmId<CreateAlgorithm, blink::WebCryptoAlgorithmIdAesGcm>,
        192);
    alg_to_info_["A256GCM"] = JwkAlgorithmInfo(
        &BindAlgorithmId<CreateAlgorithm, blink::WebCryptoAlgorithmIdAesGcm>,
        256);
    alg_to_info_["A128CBC"] = JwkAlgorithmInfo(
        &BindAlgorithmId<CreateAlgorithm, blink::WebCryptoAlgorithmIdAesCbc>,
        128);
    alg_to_info_["A192CBC"] = JwkAlgorithmInfo(
        &BindAlgorithmId<CreateAlgorithm, blink::WebCryptoAlgorithmIdAesCbc>,
        192);
    alg_to_info_["A256CBC"] = JwkAlgorithmInfo(
        &BindAlgorithmId<CreateAlgorithm, blink::WebCryptoAlgorithmIdAesCbc>,
        256);
  }

  // Returns NULL if the algorithm name was not registered.
  const JwkAlgorithmInfo* GetAlgorithmInfo(const std::string& jwk_alg) const {
    const JwkAlgorithmInfoMap::const_iterator pos = alg_to_info_.find(jwk_alg);
    if (pos == alg_to_info_.end())
      return NULL;
    return &pos->second;
  }

 private:
  // Binds a WebCryptoAlgorithmId value to a compatible factory function.
  typedef blink::WebCryptoAlgorithm (*FuncWithWebCryptoAlgIdArg)(
      blink::WebCryptoAlgorithmId);
  template <FuncWithWebCryptoAlgIdArg func,
            blink::WebCryptoAlgorithmId algorithm_id>
  static blink::WebCryptoAlgorithm BindAlgorithmId() {
    return func(algorithm_id);
  }

  JwkAlgorithmInfoMap alg_to_info_;
};

base::LazyInstance<JwkAlgorithmRegistry> jwk_alg_registry =
    LAZY_INSTANCE_INITIALIZER;

bool ImportAlgorithmsConsistent(const blink::WebCryptoAlgorithm& alg1,
                                const blink::WebCryptoAlgorithm& alg2) {
  DCHECK(!alg1.isNull());
  DCHECK(!alg2.isNull());
  if (alg1.id() != alg2.id())
    return false;
  if (alg1.paramsType() != alg2.paramsType())
    return false;
  switch (alg1.paramsType()) {
    case blink::WebCryptoAlgorithmParamsTypeNone:
      return true;
    case blink::WebCryptoAlgorithmParamsTypeRsaHashedImportParams:
      return ImportAlgorithmsConsistent(alg1.rsaHashedImportParams()->hash(),
                                        alg2.rsaHashedImportParams()->hash());
    case blink::WebCryptoAlgorithmParamsTypeHmacImportParams:
      return ImportAlgorithmsConsistent(alg1.hmacImportParams()->hash(),
                                        alg2.hmacImportParams()->hash());
    default:
      return false;
  }
}

// Extracts the required string property with key |path| from |dict| and saves
// the result to |*result|. If the property does not exist or is not a string,
// returns an error.
Status GetJwkString(base::DictionaryValue* dict,
                    const std::string& path,
                    std::string* result) {
  base::Value* value = NULL;
  if (!dict->Get(path, &value))
    return Status::ErrorJwkPropertyMissing(path);
  if (!value->GetAsString(result))
    return Status::ErrorJwkPropertyWrongType(path, "string");
  return Status::Success();
}

// Extracts the optional string property with key |path| from |dict| and saves
// the result to |*result| if it was found. If the property exists and is not a
// string, returns an error. Otherwise returns success, and sets
// |*property_exists| if it was found.
Status GetOptionalJwkString(base::DictionaryValue* dict,
                            const std::string& path,
                            std::string* result,
                            bool* property_exists) {
  *property_exists = false;
  base::Value* value = NULL;
  if (!dict->Get(path, &value))
    return Status::Success();

  if (!value->GetAsString(result))
    return Status::ErrorJwkPropertyWrongType(path, "string");

  *property_exists = true;
  return Status::Success();
}

// Extracts the optional array property with key |path| from |dict| and saves
// the result to |*result| if it was found. If the property exists and is not an
// array, returns an error. Otherwise returns success, and sets
// |*property_exists| if it was found. Note that |*result| is owned by |dict|.
Status GetOptionalJwkList(base::DictionaryValue* dict,
                          const std::string& path,
                          base::ListValue** result,
                          bool* property_exists) {
  *property_exists = false;
  base::Value* value = NULL;
  if (!dict->Get(path, &value))
    return Status::Success();

  if (!value->GetAsList(result))
    return Status::ErrorJwkPropertyWrongType(path, "list");

  *property_exists = true;
  return Status::Success();
}

// Extracts the required string property with key |path| from |dict| and saves
// the base64url-decoded bytes to |*result|. If the property does not exist or
// is not a string, or could not be base64url-decoded, returns an error.
Status GetJwkBytes(base::DictionaryValue* dict,
                   const std::string& path,
                   std::string* result) {
  std::string base64_string;
  Status status = GetJwkString(dict, path, &base64_string);
  if (status.IsError())
    return status;

  if (!Base64DecodeUrlSafe(base64_string, result))
    return Status::ErrorJwkBase64Decode(path);

  return Status::Success();
}

// Extracts the optional string property with key |path| from |dict| and saves
// the base64url-decoded bytes to |*result|. If the property exist and is not a
// string, or could not be base64url-decoded, returns an error. In the case
// where the property does not exist, |result| is guaranteed to be empty.
Status GetOptionalJwkBytes(base::DictionaryValue* dict,
                           const std::string& path,
                           std::string* result,
                           bool* property_exists) {
  std::string base64_string;
  Status status =
      GetOptionalJwkString(dict, path, &base64_string, property_exists);
  if (status.IsError())
    return status;

  if (!*property_exists) {
    result->clear();
    return Status::Success();
  }

  if (!Base64DecodeUrlSafe(base64_string, result))
    return Status::ErrorJwkBase64Decode(path);

  return Status::Success();
}

// Extracts the optional boolean property with key |path| from |dict| and saves
// the result to |*result| if it was found. If the property exists and is not a
// boolean, returns an error. Otherwise returns success, and sets
// |*property_exists| if it was found.
Status GetOptionalJwkBool(base::DictionaryValue* dict,
                          const std::string& path,
                          bool* result,
                          bool* property_exists) {
  *property_exists = false;
  base::Value* value = NULL;
  if (!dict->Get(path, &value))
    return Status::Success();

  if (!value->GetAsBoolean(result))
    return Status::ErrorJwkPropertyWrongType(path, "boolean");

  *property_exists = true;
  return Status::Success();
}

// Writes a secret/symmetric key to a JWK dictionary.
void WriteSecretKey(const std::vector<uint8>& raw_key,
                    base::DictionaryValue* jwk_dict) {
  DCHECK(jwk_dict);
  jwk_dict->SetString("kty", "oct");
  // For a secret/symmetric key, the only extra JWK field is 'k', containing the
  // base64url encoding of the raw key.
  const base::StringPiece key_str(
      reinterpret_cast<const char*>(Uint8VectorStart(raw_key)), raw_key.size());
  jwk_dict->SetString("k", Base64EncodeUrlSafe(key_str));
}

// Writes an RSA public key to a JWK dictionary
void WriteRsaPublicKey(const std::vector<uint8>& modulus,
                       const std::vector<uint8>& public_exponent,
                       base::DictionaryValue* jwk_dict) {
  DCHECK(jwk_dict);
  DCHECK(modulus.size());
  DCHECK(public_exponent.size());
  jwk_dict->SetString("kty", "RSA");
  jwk_dict->SetString("n", Base64EncodeUrlSafe(modulus));
  jwk_dict->SetString("e", Base64EncodeUrlSafe(public_exponent));
}

// Writes an RSA private key to a JWK dictionary
Status ExportRsaPrivateKeyJwk(const blink::WebCryptoKey& key,
                              base::DictionaryValue* jwk_dict) {
  platform::PrivateKey* private_key;
  Status status = ToPlatformPrivateKey(key, &private_key);
  if (status.IsError())
    return status;

  // TODO(eroman): Copying the key properties to temporary vectors is
  // inefficient. Once there aren't two implementations of platform_crypto this
  // and other code will be easier to streamline.
  std::vector<uint8> modulus;
  std::vector<uint8> public_exponent;
  std::vector<uint8> private_exponent;
  std::vector<uint8> prime1;
  std::vector<uint8> prime2;
  std::vector<uint8> exponent1;
  std::vector<uint8> exponent2;
  std::vector<uint8> coefficient;

  status = platform::ExportRsaPrivateKey(private_key,
                                         &modulus,
                                         &public_exponent,
                                         &private_exponent,
                                         &prime1,
                                         &prime2,
                                         &exponent1,
                                         &exponent2,
                                         &coefficient);
  if (status.IsError())
    return status;

  jwk_dict->SetString("kty", "RSA");
  jwk_dict->SetString("n", Base64EncodeUrlSafe(modulus));
  jwk_dict->SetString("e", Base64EncodeUrlSafe(public_exponent));
  jwk_dict->SetString("d", Base64EncodeUrlSafe(private_exponent));
  // Although these are "optional" in the JWA, WebCrypto spec requires them to
  // be emitted.
  jwk_dict->SetString("p", Base64EncodeUrlSafe(prime1));
  jwk_dict->SetString("q", Base64EncodeUrlSafe(prime2));
  jwk_dict->SetString("dp", Base64EncodeUrlSafe(exponent1));
  jwk_dict->SetString("dq", Base64EncodeUrlSafe(exponent2));
  jwk_dict->SetString("qi", Base64EncodeUrlSafe(coefficient));

  return Status::Success();
}

// Writes a Web Crypto usage mask to a JWK dictionary.
void WriteKeyOps(blink::WebCryptoKeyUsageMask key_usages,
                 base::DictionaryValue* jwk_dict) {
  jwk_dict->Set("key_ops", CreateJwkKeyOpsFromWebCryptoUsages(key_usages));
}

// Writes a Web Crypto extractable value to a JWK dictionary.
void WriteExt(bool extractable, base::DictionaryValue* jwk_dict) {
  jwk_dict->SetBoolean("ext", extractable);
}

// Writes a Web Crypto algorithm to a JWK dictionary.
Status WriteAlg(const blink::WebCryptoKeyAlgorithm& algorithm,
                base::DictionaryValue* jwk_dict) {
  switch (algorithm.paramsType()) {
    case blink::WebCryptoKeyAlgorithmParamsTypeAes: {
      DCHECK(algorithm.aesParams());
      const char* aes_prefix = "";
      switch (algorithm.aesParams()->lengthBits()) {
        case 128:
          aes_prefix = "A128";
          break;
        case 192:
          aes_prefix = "A192";
          break;
        case 256:
          aes_prefix = "A256";
          break;
        default:
          NOTREACHED();  // bad key length means algorithm was built improperly
          return Status::ErrorUnexpected();
      }
      const char* aes_suffix = "";
      switch (algorithm.id()) {
        case blink::WebCryptoAlgorithmIdAesCbc:
          aes_suffix = "CBC";
          break;
        case blink::WebCryptoAlgorithmIdAesCtr:
          aes_suffix = "CTR";
          break;
        case blink::WebCryptoAlgorithmIdAesGcm:
          aes_suffix = "GCM";
          break;
        case blink::WebCryptoAlgorithmIdAesKw:
          aes_suffix = "KW";
          break;
        default:
          return Status::ErrorUnsupported();
      }
      jwk_dict->SetString("alg",
                          base::StringPrintf("%s%s", aes_prefix, aes_suffix));
      break;
    }
    case blink::WebCryptoKeyAlgorithmParamsTypeHmac: {
      DCHECK(algorithm.hmacParams());
      switch (algorithm.hmacParams()->hash().id()) {
        case blink::WebCryptoAlgorithmIdSha1:
          jwk_dict->SetString("alg", "HS1");
          break;
        case blink::WebCryptoAlgorithmIdSha256:
          jwk_dict->SetString("alg", "HS256");
          break;
        case blink::WebCryptoAlgorithmIdSha384:
          jwk_dict->SetString("alg", "HS384");
          break;
        case blink::WebCryptoAlgorithmIdSha512:
          jwk_dict->SetString("alg", "HS512");
          break;
        default:
          NOTREACHED();
          return Status::ErrorUnexpected();
      }
      break;
    }
    case blink::WebCryptoKeyAlgorithmParamsTypeRsaHashed:
      switch (algorithm.id()) {
        case blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5: {
          switch (algorithm.rsaHashedParams()->hash().id()) {
            case blink::WebCryptoAlgorithmIdSha1:
              jwk_dict->SetString("alg", "RS1");
              break;
            case blink::WebCryptoAlgorithmIdSha256:
              jwk_dict->SetString("alg", "RS256");
              break;
            case blink::WebCryptoAlgorithmIdSha384:
              jwk_dict->SetString("alg", "RS384");
              break;
            case blink::WebCryptoAlgorithmIdSha512:
              jwk_dict->SetString("alg", "RS512");
              break;
            default:
              NOTREACHED();
              return Status::ErrorUnexpected();
          }
          break;
        }
        case blink::WebCryptoAlgorithmIdRsaOaep: {
          switch (algorithm.rsaHashedParams()->hash().id()) {
            case blink::WebCryptoAlgorithmIdSha1:
              jwk_dict->SetString("alg", "RSA-OAEP");
              break;
            case blink::WebCryptoAlgorithmIdSha256:
              jwk_dict->SetString("alg", "RSA-OAEP-256");
              break;
            case blink::WebCryptoAlgorithmIdSha384:
              jwk_dict->SetString("alg", "RSA-OAEP-384");
              break;
            case blink::WebCryptoAlgorithmIdSha512:
              jwk_dict->SetString("alg", "RSA-OAEP-512");
              break;
            default:
              NOTREACHED();
              return Status::ErrorUnexpected();
          }
          break;
        }
        default:
          NOTREACHED();
          return Status::ErrorUnexpected();
      }
      break;
    default:
      return Status::ErrorUnsupported();
  }
  return Status::Success();
}

bool IsRsaKey(const blink::WebCryptoKey& key) {
  return IsAlgorithmRsa(key.algorithm().id());
}

Status ImportRsaKey(base::DictionaryValue* dict,
                    const blink::WebCryptoAlgorithm& algorithm,
                    bool extractable,
                    blink::WebCryptoKeyUsageMask usage_mask,
                    blink::WebCryptoKey* key) {
  // An RSA public key must have an "n" (modulus) and an "e" (exponent) entry
  // in the JWK, while an RSA private key must have those, plus at least a "d"
  // (private exponent) entry.
  // See http://tools.ietf.org/html/draft-ietf-jose-json-web-algorithms-18,
  // section 6.3.
  std::string jwk_n_value;
  Status status = GetJwkBytes(dict, "n", &jwk_n_value);
  if (status.IsError())
    return status;
  std::string jwk_e_value;
  status = GetJwkBytes(dict, "e", &jwk_e_value);
  if (status.IsError())
    return status;

  bool is_public_key = !dict->HasKey("d");

  // Now that the key type is known, do an additional check on the usages to
  // make sure they are all applicable for this algorithm + key type.
  status = CheckKeyUsages(algorithm.id(),
                          is_public_key ? blink::WebCryptoKeyTypePublic
                                        : blink::WebCryptoKeyTypePrivate,
                          usage_mask);

  if (status.IsError())
    return status;

  if (is_public_key) {
    return platform::ImportRsaPublicKey(algorithm,
                                        extractable,
                                        usage_mask,
                                        CryptoData(jwk_n_value),
                                        CryptoData(jwk_e_value),
                                        key);
  }

  std::string jwk_d_value;
  status = GetJwkBytes(dict, "d", &jwk_d_value);
  if (status.IsError())
    return status;

  // The "p", "q", "dp", "dq", and "qi" properties are optional. Treat these
  // properties the same if they are unspecified, as if they were specified-but
  // empty, since ImportRsaPrivateKey() doesn't do validation checks anyway.

  std::string jwk_p_value;
  bool has_p;
  status = GetOptionalJwkBytes(dict, "p", &jwk_p_value, &has_p);
  if (status.IsError())
    return status;

  std::string jwk_q_value;
  bool has_q;
  status = GetOptionalJwkBytes(dict, "q", &jwk_q_value, &has_q);
  if (status.IsError())
    return status;

  std::string jwk_dp_value;
  bool has_dp;
  status = GetOptionalJwkBytes(dict, "dp", &jwk_dp_value, &has_dp);
  if (status.IsError())
    return status;

  std::string jwk_dq_value;
  bool has_dq;
  status = GetOptionalJwkBytes(dict, "dq", &jwk_dq_value, &has_dq);
  if (status.IsError())
    return status;

  std::string jwk_qi_value;
  bool has_qi;
  status = GetOptionalJwkBytes(dict, "qi", &jwk_qi_value, &has_qi);
  if (status.IsError())
    return status;

  int num_optional_properties = has_p + has_q + has_dp + has_dq + has_qi;
  if (num_optional_properties != 0 && num_optional_properties != 5)
    return Status::ErrorJwkIncompleteOptionalRsaPrivateKey();

  return platform::ImportRsaPrivateKey(
      algorithm,
      extractable,
      usage_mask,
      CryptoData(jwk_n_value),   // modulus
      CryptoData(jwk_e_value),   // public_exponent
      CryptoData(jwk_d_value),   // private_exponent
      CryptoData(jwk_p_value),   // prime1
      CryptoData(jwk_q_value),   // prime2
      CryptoData(jwk_dp_value),  // exponent1
      CryptoData(jwk_dq_value),  // exponent2
      CryptoData(jwk_qi_value),  // coefficient
      key);
}

}  // namespace

// TODO(eroman): Split this up into smaller functions.
Status ImportKeyJwk(const CryptoData& key_data,
                    const blink::WebCryptoAlgorithm& algorithm,
                    bool extractable,
                    blink::WebCryptoKeyUsageMask usage_mask,
                    blink::WebCryptoKey* key) {
  if (!key_data.byte_length())
    return Status::ErrorImportEmptyKeyData();
  DCHECK(key);

  // Parse the incoming JWK JSON.
  base::StringPiece json_string(reinterpret_cast<const char*>(key_data.bytes()),
                                key_data.byte_length());
  scoped_ptr<base::Value> value(base::JSONReader::Read(json_string));
  // Note, bare pointer dict_value is ok since it points into scoped value.
  base::DictionaryValue* dict_value = NULL;
  if (!value.get() || !value->GetAsDictionary(&dict_value) || !dict_value)
    return Status::ErrorJwkNotDictionary();

  // JWK "kty". Exit early if this required JWK parameter is missing.
  std::string jwk_kty_value;
  Status status = GetJwkString(dict_value, "kty", &jwk_kty_value);
  if (status.IsError())
    return status;

  // JWK "ext" (optional) --> extractable parameter
  {
    bool jwk_ext_value = false;
    bool has_jwk_ext;
    status =
        GetOptionalJwkBool(dict_value, "ext", &jwk_ext_value, &has_jwk_ext);
    if (status.IsError())
      return status;
    if (has_jwk_ext && !jwk_ext_value && extractable)
      return Status::ErrorJwkExtInconsistent();
  }

  // JWK "alg" --> algorithm parameter
  // 1. JWK alg present but unrecognized: error
  // 2. JWK alg valid and inconsistent with input algorithm: error
  // 3. JWK alg valid and consistent with input algorithm: use input value
  // 4. JWK alg is missing: use input value
  const JwkAlgorithmInfo* algorithm_info = NULL;
  std::string jwk_alg_value;
  bool has_jwk_alg;
  status =
      GetOptionalJwkString(dict_value, "alg", &jwk_alg_value, &has_jwk_alg);
  if (status.IsError())
    return status;

  if (has_jwk_alg) {
    // JWK alg present

    // TODO(padolph): Validate alg vs kty. For example kty="RSA" implies alg can
    // only be from the RSA family.

    blink::WebCryptoAlgorithm jwk_algorithm =
        blink::WebCryptoAlgorithm::createNull();
    algorithm_info = jwk_alg_registry.Get().GetAlgorithmInfo(jwk_alg_value);
    if (!algorithm_info ||
        !algorithm_info->CreateImportAlgorithm(&jwk_algorithm))
      return Status::ErrorJwkUnrecognizedAlgorithm();

    if (!ImportAlgorithmsConsistent(jwk_algorithm, algorithm))
      return Status::ErrorJwkAlgorithmInconsistent();
  }
  DCHECK(!algorithm.isNull());

  // JWK "key_ops" (optional) --> usage_mask parameter
  base::ListValue* jwk_key_ops_value = NULL;
  bool has_jwk_key_ops;
  status = GetOptionalJwkList(
      dict_value, "key_ops", &jwk_key_ops_value, &has_jwk_key_ops);
  if (status.IsError())
    return status;
  blink::WebCryptoKeyUsageMask jwk_key_ops_mask = 0;
  if (has_jwk_key_ops) {
    status =
        GetWebCryptoUsagesFromJwkKeyOps(jwk_key_ops_value, &jwk_key_ops_mask);
    if (status.IsError())
      return status;
    // The input usage_mask must be a subset of jwk_key_ops_mask.
    if (!ContainsKeyUsages(jwk_key_ops_mask, usage_mask))
      return Status::ErrorJwkKeyopsInconsistent();
  }

  // JWK "use" (optional) --> usage_mask parameter
  std::string jwk_use_value;
  bool has_jwk_use;
  status =
      GetOptionalJwkString(dict_value, "use", &jwk_use_value, &has_jwk_use);
  if (status.IsError())
    return status;
  blink::WebCryptoKeyUsageMask jwk_use_mask = 0;
  if (has_jwk_use) {
    if (jwk_use_value == "enc")
      jwk_use_mask = kJwkEncUsage;
    else if (jwk_use_value == "sig")
      jwk_use_mask = kJwkSigUsage;
    else
      return Status::ErrorJwkUnrecognizedUse();
    // The input usage_mask must be a subset of jwk_use_mask.
    if (!ContainsKeyUsages(jwk_use_mask, usage_mask))
      return Status::ErrorJwkUseInconsistent();
  }

  // If both 'key_ops' and 'use' are present, ensure they are consistent.
  if (has_jwk_key_ops && has_jwk_use &&
      !ContainsKeyUsages(jwk_use_mask, jwk_key_ops_mask))
    return Status::ErrorJwkUseAndKeyopsInconsistent();

  // JWK keying material --> ImportKeyInternal()
  if (jwk_kty_value == "oct") {
    std::string jwk_k_value;
    status = GetJwkBytes(dict_value, "k", &jwk_k_value);
    if (status.IsError())
      return status;

    // Some JWK alg ID's embed information about the key length in the alg ID
    // string. For example "A128CBC" implies the JWK carries 128 bits
    // of key material. For such keys validate that enough bytes were provided.
    // If this validation is not done, then it would be possible to select a
    // different algorithm by passing a different lengthed key, since that is
    // how WebCrypto interprets things.
    if (algorithm_info &&
        algorithm_info->IsInvalidKeyByteLength(jwk_k_value.size())) {
      return Status::ErrorJwkIncorrectKeyLength();
    }

    return ImportKey(blink::WebCryptoKeyFormatRaw,
                     CryptoData(jwk_k_value),
                     algorithm,
                     extractable,
                     usage_mask,
                     key);
  }

  if (jwk_kty_value == "RSA")
    return ImportRsaKey(dict_value, algorithm, extractable, usage_mask, key);

  return Status::ErrorJwkUnrecognizedKty();
}

Status ExportKeyJwk(const blink::WebCryptoKey& key,
                    std::vector<uint8>* buffer) {
  DCHECK(key.extractable());
  base::DictionaryValue jwk_dict;
  Status status = Status::OperationError();

  switch (key.type()) {
    case blink::WebCryptoKeyTypeSecret: {
      std::vector<uint8> exported_key;
      status = ExportKey(blink::WebCryptoKeyFormatRaw, key, &exported_key);
      if (status.IsError())
        return status;
      WriteSecretKey(exported_key, &jwk_dict);
      break;
    }
    case blink::WebCryptoKeyTypePublic: {
      // TODO(eroman): Update when there are asymmetric keys other than RSA.
      if (!IsRsaKey(key))
        return Status::ErrorUnsupported();
      platform::PublicKey* public_key;
      status = ToPlatformPublicKey(key, &public_key);
      if (status.IsError())
        return status;
      std::vector<uint8> modulus;
      std::vector<uint8> public_exponent;
      status =
          platform::ExportRsaPublicKey(public_key, &modulus, &public_exponent);
      if (status.IsError())
        return status;
      WriteRsaPublicKey(modulus, public_exponent, &jwk_dict);
      break;
    }
    case blink::WebCryptoKeyTypePrivate: {
      // TODO(eroman): Update when there are asymmetric keys other than RSA.
      if (!IsRsaKey(key))
        return Status::ErrorUnsupported();

      status = ExportRsaPrivateKeyJwk(key, &jwk_dict);
      if (status.IsError())
        return status;
      break;
    }

    default:
      return Status::ErrorUnsupported();
  }

  WriteKeyOps(key.usages(), &jwk_dict);
  WriteExt(key.extractable(), &jwk_dict);
  status = WriteAlg(key.algorithm(), &jwk_dict);
  if (status.IsError())
    return status;

  std::string json;
  base::JSONWriter::Write(&jwk_dict, &json);
  buffer->assign(json.data(), json.data() + json.size());
  return Status::Success();
}

}  // namespace webcrypto

}  // namespace content
