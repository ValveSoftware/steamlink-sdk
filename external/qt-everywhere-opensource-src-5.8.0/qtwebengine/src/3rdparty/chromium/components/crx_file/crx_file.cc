// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crx_file/crx_file.h"

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/crx_file/id_util.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "crypto/signature_verifier.h"

namespace crx_file {

namespace {

// The current version of the crx format.
static const uint32_t kCurrentVersion = 2;

// The current version of the crx diff format.
static const uint32_t kCurrentDiffVersion = 0;

// The maximum size the crx parser will tolerate for a public key.
static const uint32_t kMaxPublicKeySize = 1 << 16;

// The maximum size the crx parser will tolerate for a signature.
static const uint32_t kMaxSignatureSize = 1 << 16;

// Helper function to read bytes into a buffer while also updating a hash with
// those bytes. Returns the number of bytes read.
size_t ReadAndHash(void* ptr,
                   size_t size,
                   size_t nmemb,
                   FILE* stream,
                   crypto::SecureHash* hash) {
  size_t item_count = fread(ptr, size, nmemb, stream);
  base::CheckedNumeric<size_t> byte_count(item_count);
  byte_count *= size;
  if (!byte_count.IsValid())
    return 0;
  if (item_count > 0 && hash) {
    hash->Update(ptr, byte_count.ValueOrDie());
  }
  return byte_count.ValueOrDie();
}

// Helper function to finish computing a hash and return an error if the
// result of the hash didn't meet an expected base64-encoded value.
CrxFile::ValidateError FinalizeHash(const std::string& extension_id,
                                    crypto::SecureHash* hash,
                                    const std::string& expected_hash) {
  CHECK(hash != nullptr);
  uint8_t output[crypto::kSHA256Length] = {};
  hash->Finish(output, sizeof(output));
  std::string hash_base64 =
      base::ToLowerASCII(base::HexEncode(output, sizeof(output)));
  if (hash_base64 != expected_hash) {
    LOG(ERROR) << "Hash check failed for extension: " << extension_id
               << ", expected " << expected_hash << ", got " << hash_base64;
    return CrxFile::ValidateError::CRX_HASH_VERIFICATION_FAILED;
  } else {
    return CrxFile::ValidateError::NONE;
  }
}

}  // namespace

// The magic string embedded in the header.
const char kCrxFileHeaderMagic[] = "Cr24";
const char kCrxDiffFileHeaderMagic[] = "CrOD";

std::unique_ptr<CrxFile> CrxFile::Parse(const CrxFile::Header& header,
                                        CrxFile::Error* error) {
  if (HeaderIsValid(header, error))
    return base::WrapUnique(new CrxFile(header));
  return nullptr;
}

std::unique_ptr<CrxFile> CrxFile::Create(const uint32_t key_size,
                                         const uint32_t signature_size,
                                         CrxFile::Error* error) {
  CrxFile::Header header;
  memcpy(&header.magic, kCrxFileHeaderMagic, kCrxFileHeaderMagicSize);
  header.version = kCurrentVersion;
  header.key_size = key_size;
  header.signature_size = signature_size;
  if (HeaderIsValid(header, error))
    return base::WrapUnique(new CrxFile(header));
  return nullptr;
}

bool CrxFile::HeaderIsDelta(const CrxFile::Header& header) {
  return !strncmp(kCrxDiffFileHeaderMagic, header.magic, sizeof(header.magic));
}

// static
CrxFile::ValidateError CrxFile::ValidateSignature(
    const base::FilePath& crx_path,
    const std::string& expected_hash,
    std::string* public_key,
    std::string* extension_id,
    CrxFile::Header* header_out) {
  base::ScopedFILE file(base::OpenFile(crx_path, "rb"));
  std::unique_ptr<crypto::SecureHash> hash;
  if (!expected_hash.empty())
    hash = crypto::SecureHash::Create(crypto::SecureHash::SHA256);

  if (!file.get())
    return ValidateError::CRX_FILE_NOT_READABLE;

  CrxFile::Header header;
  size_t len = ReadAndHash(&header, sizeof(header), 1, file.get(), hash.get());
  if (len != sizeof(header))
    return ValidateError::CRX_HEADER_INVALID;
  if (header_out)
    *header_out = header;

  CrxFile::Error error;
  std::unique_ptr<CrxFile> crx(CrxFile::Parse(header, &error));
  if (!crx) {
    switch (error) {
      case CrxFile::kWrongMagic:
        return ValidateError::CRX_MAGIC_NUMBER_INVALID;
      case CrxFile::kInvalidVersion:
        return ValidateError::CRX_VERSION_NUMBER_INVALID;

      case CrxFile::kInvalidKeyTooLarge:
      case CrxFile::kInvalidSignatureTooLarge:
        return ValidateError::CRX_EXCESSIVELY_LARGE_KEY_OR_SIGNATURE;

      case CrxFile::kInvalidKeyTooSmall:
        return ValidateError::CRX_ZERO_KEY_LENGTH;
      case CrxFile::kInvalidSignatureTooSmall:
        return ValidateError::CRX_ZERO_SIGNATURE_LENGTH;

      default:
        return ValidateError::CRX_HEADER_INVALID;
    }
  }

  std::vector<uint8_t> key(header.key_size);
  len = ReadAndHash(&key.front(), sizeof(uint8_t), header.key_size, file.get(),
                    hash.get());
  if (len != header.key_size)
    return ValidateError::CRX_PUBLIC_KEY_INVALID;

  std::vector<uint8_t> signature(header.signature_size);
  len = ReadAndHash(&signature.front(), sizeof(uint8_t), header.signature_size,
                    file.get(), hash.get());
  if (len < header.signature_size)
    return ValidateError::CRX_SIGNATURE_INVALID;

  crypto::SignatureVerifier verifier;
  if (!verifier.VerifyInit(crypto::SignatureVerifier::RSA_PKCS1_SHA1,
                           signature.data(), static_cast<int>(signature.size()),
                           key.data(), static_cast<int>(key.size()))) {
    // Signature verification initialization failed. This is most likely
    // caused by a public key in the wrong format (should encode algorithm).
    return ValidateError::CRX_SIGNATURE_VERIFICATION_INITIALIZATION_FAILED;
  }

  uint8_t buf[1 << 12] = {};
  while ((len = ReadAndHash(buf, sizeof(buf[0]), arraysize(buf), file.get(),
                            hash.get())) > 0)
    verifier.VerifyUpdate(buf, static_cast<int>(len));

  if (!verifier.VerifyFinal())
    return ValidateError::CRX_SIGNATURE_VERIFICATION_FAILED;

  std::string public_key_bytes =
      std::string(reinterpret_cast<char*>(&key.front()), key.size());
  if (public_key)
    base::Base64Encode(public_key_bytes, public_key);

  std::string id = id_util::GenerateId(public_key_bytes);
  if (extension_id)
    *extension_id = id;

  if (!expected_hash.empty())
    return FinalizeHash(id, hash.get(), expected_hash);

  return ValidateError::NONE;
}

CrxFile::CrxFile(const Header& header) : header_(header) {}

bool CrxFile::HeaderIsValid(const CrxFile::Header& header,
                            CrxFile::Error* error) {
  bool valid = false;
  bool diffCrx = false;
  if (!strncmp(kCrxDiffFileHeaderMagic, header.magic, sizeof(header.magic)))
    diffCrx = true;
  if (strncmp(kCrxFileHeaderMagic, header.magic, sizeof(header.magic)) &&
      !diffCrx)
    *error = kWrongMagic;
  else if (header.version != kCurrentVersion
      && !(diffCrx && header.version == kCurrentDiffVersion))
    *error = kInvalidVersion;
  else if (header.key_size > kMaxPublicKeySize)
    *error = kInvalidKeyTooLarge;
  else if (header.key_size == 0)
    *error = kInvalidKeyTooSmall;
  else if (header.signature_size > kMaxSignatureSize)
    *error = kInvalidSignatureTooLarge;
  else if (header.signature_size == 0)
    *error = kInvalidSignatureTooSmall;
  else
    valid = true;
  return valid;
}

}  // namespace crx_file
