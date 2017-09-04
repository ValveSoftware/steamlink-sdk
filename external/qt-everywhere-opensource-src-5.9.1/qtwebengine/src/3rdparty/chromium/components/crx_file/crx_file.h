// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRX_FILE_CRX_FILE_H_
#define COMPONENTS_CRX_FILE_CRX_FILE_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <memory>
#include <string>
#include <vector>

namespace base {
class FilePath;
}

namespace crx_file {

// CRX files have a header that includes a magic key, version number, and
// some signature sizing information. Use CrxFile object to validate whether
// the header is valid or not.
class CrxFile {
 public:
  // The size of the magic character sequence at the beginning of each crx
  // file, in bytes. This should be a multiple of 4.
  static const size_t kCrxFileHeaderMagicSize = 4;

  // This header is the first data at the beginning of an extension. Its
  // contents are purposely 32-bit aligned so that it can just be slurped into
  // a struct without manual parsing.
  struct Header {
    char magic[kCrxFileHeaderMagicSize];
    uint32_t version;
    uint32_t key_size;        // The size of the public key, in bytes.
    uint32_t signature_size;  // The size of the signature, in bytes.
    // An ASN.1-encoded PublicKeyInfo structure follows.
    // The signature follows.
  };

  enum Error {
    kWrongMagic,
    kInvalidVersion,
    kInvalidKeyTooLarge,
    kInvalidKeyTooSmall,
    kInvalidSignatureTooLarge,
    kInvalidSignatureTooSmall,
  };

  // Construct a new CRX file header object with bytes of a header
  // read from a CRX file. If a null scoped_ptr is returned, |error|
  // contains an error code with additional information.
  static std::unique_ptr<CrxFile> Parse(const Header& header, Error* error);

  // Construct a new header for the given key and signature sizes.
  // Returns a null scoped_ptr if erroneous values of |key_size| and/or
  // |signature_size| are provided. |error| contains an error code with
  // additional information.
  // Use this constructor and then .header() to obtain the Header
  // for writing out to a CRX file.
  static std::unique_ptr<CrxFile> Create(const uint32_t key_size,
                                         const uint32_t signature_size,
                                         Error* error);

  // Returns the header structure for writing out to a CRX file.
  const Header& header() const { return header_; }

  // Checks a valid |header| to determine whether or not the CRX represents a
  // differential CRX.
  static bool HeaderIsDelta(const Header& header);

  enum class ValidateError {
    NONE,
    CRX_FILE_NOT_READABLE,
    CRX_HEADER_INVALID,
    CRX_MAGIC_NUMBER_INVALID,
    CRX_VERSION_NUMBER_INVALID,
    CRX_EXCESSIVELY_LARGE_KEY_OR_SIGNATURE,
    CRX_ZERO_KEY_LENGTH,
    CRX_ZERO_SIGNATURE_LENGTH,
    CRX_PUBLIC_KEY_INVALID,
    CRX_SIGNATURE_INVALID,
    CRX_SIGNATURE_VERIFICATION_INITIALIZATION_FAILED,
    CRX_SIGNATURE_VERIFICATION_FAILED,
    CRX_HASH_VERIFICATION_FAILED,
  };

  // Validates that the .crx file at |crx_path| is properly signed.  If
  // |expected_hash| is non-empty, it should specify a hex-encoded SHA256 hash
  // for the entire .crx file which will be checked as we read it, and any
  // mismatch will cause a CRX_HASH_VERIFICATION_FAILED result.  The
  // |public_key| argument can be provided to receive a copy of the
  // base64-encoded public key pem bytes extracted from the .crx. The
  // |extension_id| argument can be provided to receive the extension id (which
  // is computed as a hash of the public key provided in the .crx). The
  // |header| argument can be provided to receive a copy of the crx header.
  static ValidateError ValidateSignature(const base::FilePath& crx_path,
                                         const std::string& expected_hash,
                                         std::string* public_key,
                                         std::string* extension_id,
                                         CrxFile::Header* header);

 private:
  Header header_;

  // Constructor is private. Clients should use static factory methods above.
  explicit CrxFile(const Header& header);

  // Checks the |header| for validity and returns true if the values are valid.
  // If false is returned, more detailed error code is returned in |error|.
  static bool HeaderIsValid(const Header& header, Error* error);
};

}  // namespace crx_file

#endif  // COMPONENTS_CRX_FILE_CRX_FILE_H_
