// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/component_unpacker.h"

#include <stdint.h>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/json/json_file_value_serializer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/crx_file/crx_file.h"
#include "components/update_client/component_patcher.h"
#include "components/update_client/component_patcher_operation.h"
#include "components/update_client/update_client.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "third_party/zlib/google/zip.h"

using crypto::SecureHash;
using crx_file::CrxFile;

namespace update_client {

ComponentUnpacker::ComponentUnpacker(
    const std::vector<uint8_t>& pk_hash,
    const base::FilePath& path,
    const std::string& fingerprint,
    const scoped_refptr<CrxInstaller>& installer,
    const scoped_refptr<OutOfProcessPatcher>& oop_patcher,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : pk_hash_(pk_hash),
      path_(path),
      is_delta_(false),
      fingerprint_(fingerprint),
      installer_(installer),
      oop_patcher_(oop_patcher),
      error_(kNone),
      extended_error_(0),
      task_runner_(task_runner) {
}

// TODO(cpu): add a specific attribute check to a component json that the
// extension unpacker will reject, so that a component cannot be installed
// as an extension.
std::unique_ptr<base::DictionaryValue> ReadManifest(
    const base::FilePath& unpack_path) {
  base::FilePath manifest =
      unpack_path.Append(FILE_PATH_LITERAL("manifest.json"));
  if (!base::PathExists(manifest))
    return std::unique_ptr<base::DictionaryValue>();
  JSONFileValueDeserializer deserializer(manifest);
  std::string error;
  std::unique_ptr<base::Value> root = deserializer.Deserialize(NULL, &error);
  if (!root.get())
    return std::unique_ptr<base::DictionaryValue>();
  if (!root->IsType(base::Value::TYPE_DICTIONARY))
    return std::unique_ptr<base::DictionaryValue>();
  return std::unique_ptr<base::DictionaryValue>(
      static_cast<base::DictionaryValue*>(root.release()));
}

bool ComponentUnpacker::UnpackInternal() {
  return Verify() && Unzip() && BeginPatching();
}

void ComponentUnpacker::Unpack(const Callback& callback) {
  callback_ = callback;
  if (!UnpackInternal())
    Finish();
}

bool ComponentUnpacker::Verify() {
  VLOG(1) << "Verifying component: " << path_.value();
  if (pk_hash_.empty() || path_.empty()) {
    error_ = kInvalidParams;
    return false;
  }
  // First, validate the CRX header and signature. As of today
  // this is SHA1 with RSA 1024.
  std::string public_key_bytes;
  std::string public_key_base64;
  CrxFile::Header header;
  CrxFile::ValidateError error = CrxFile::ValidateSignature(
      path_, std::string(), &public_key_base64, nullptr, &header);
  if (error != CrxFile::ValidateError::NONE ||
      !base::Base64Decode(public_key_base64, &public_key_bytes)) {
    error_ = kInvalidFile;
    return false;
  }
  is_delta_ = CrxFile::HeaderIsDelta(header);

  // File is valid and the digital signature matches. Now make sure
  // the public key hash matches the expected hash. If they do we fully
  // trust this CRX.
  uint8_t hash[crypto::kSHA256Length] = {};
  std::unique_ptr<SecureHash> sha256(SecureHash::Create(SecureHash::SHA256));
  sha256->Update(public_key_bytes.data(), public_key_bytes.size());
  sha256->Finish(hash, arraysize(hash));

  if (!std::equal(pk_hash_.begin(), pk_hash_.end(), hash)) {
    VLOG(1) << "Hash mismatch: " << path_.value();
    error_ = kInvalidId;
    return false;
  }
  VLOG(1) << "Verification successful: " << path_.value();
  return true;
}

bool ComponentUnpacker::Unzip() {
  base::FilePath& destination = is_delta_ ? unpack_diff_path_ : unpack_path_;
  VLOG(1) << "Unpacking in: " << destination.value();
  if (!base::CreateNewTempDirectory(base::FilePath::StringType(),
                                    &destination)) {
    VLOG(1) << "Unable to create temporary directory for unpacking.";
    error_ = kUnzipPathError;
    return false;
  }
  if (!zip::Unzip(path_, destination)) {
    VLOG(1) << "Unzipping failed.";
    error_ = kUnzipFailed;
    return false;
  }
  VLOG(1) << "Unpacked successfully";
  return true;
}

bool ComponentUnpacker::BeginPatching() {
  if (is_delta_) {  // Package is a diff package.
    // Use a different temp directory for the patch output files.
    if (!base::CreateNewTempDirectory(base::FilePath::StringType(),
                                      &unpack_path_)) {
      error_ = kUnzipPathError;
      return false;
    }
    patcher_ = new ComponentPatcher(unpack_diff_path_, unpack_path_, installer_,
                                    oop_patcher_, task_runner_);
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ComponentPatcher::Start, patcher_,
                   base::Bind(&ComponentUnpacker::EndPatching,
                              scoped_refptr<ComponentUnpacker>(this))));
  } else {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ComponentUnpacker::EndPatching,
                   scoped_refptr<ComponentUnpacker>(this), kNone, 0));
  }
  return true;
}

void ComponentUnpacker::EndPatching(Error error, int extended_error) {
  error_ = error;
  extended_error_ = extended_error;
  patcher_ = NULL;
  if (error_ != kNone) {
    Finish();
    return;
  }
  // Optimization: clean up patch files early, in case disk space is too low to
  // install otherwise.
  if (!unpack_diff_path_.empty()) {
    base::DeleteFile(unpack_diff_path_, true);
    unpack_diff_path_.clear();
  }
  Install();
  Finish();
}

void ComponentUnpacker::Install() {
  // Write the fingerprint to disk.
  if (static_cast<int>(fingerprint_.size()) !=
      base::WriteFile(
          unpack_path_.Append(FILE_PATH_LITERAL("manifest.fingerprint")),
          fingerprint_.c_str(), base::checked_cast<int>(fingerprint_.size()))) {
    error_ = kFingerprintWriteFailed;
    return;
  }
  std::unique_ptr<base::DictionaryValue> manifest(ReadManifest(unpack_path_));
  if (!manifest.get()) {
    error_ = kBadManifest;
    return;
  }
  DCHECK(error_ == kNone);
  if (!installer_->Install(*manifest, unpack_path_)) {
    error_ = kInstallerError;
    return;
  }
}

void ComponentUnpacker::Finish() {
  if (!unpack_diff_path_.empty())
    base::DeleteFile(unpack_diff_path_, true);
  if (!unpack_path_.empty())
    base::DeleteFile(unpack_path_, true);
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(callback_, error_, extended_error_));
}

ComponentUnpacker::~ComponentUnpacker() {
}

}  // namespace update_client
