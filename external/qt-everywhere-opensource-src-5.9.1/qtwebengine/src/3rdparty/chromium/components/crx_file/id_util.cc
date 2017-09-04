// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crx_file/id_util.h"

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "crypto/sha2.h"

namespace {

// Converts a normal hexadecimal string into the alphabet used by extensions.
// We use the characters 'a'-'p' instead of '0'-'f' to avoid ever having a
// completely numeric host, since some software interprets that as an IP
// address.
static void ConvertHexadecimalToIDAlphabet(std::string* id) {
  for (size_t i = 0; i < id->size(); ++i) {
    int val;
    if (base::HexStringToInt(
            base::StringPiece(id->begin() + i, id->begin() + i + 1), &val)) {
      (*id)[i] = val + 'a';
    } else {
      (*id)[i] = 'a';
    }
  }
}

}  // namespace

namespace crx_file {
namespace id_util {

// First 16 bytes of SHA256 hashed public key.
const size_t kIdSize = 16;

std::string GenerateId(const std::string& input) {
  uint8_t hash[kIdSize];
  crypto::SHA256HashString(input, hash, sizeof(hash));
  std::string output =
      base::ToLowerASCII(base::HexEncode(hash, sizeof(hash)));
  ConvertHexadecimalToIDAlphabet(&output);

  return output;
}

std::string GenerateIdForPath(const base::FilePath& path) {
  base::FilePath new_path = MaybeNormalizePath(path);
  std::string path_bytes =
      std::string(reinterpret_cast<const char*>(new_path.value().data()),
                  new_path.value().size() * sizeof(base::FilePath::CharType));
  return GenerateId(path_bytes);
}

std::string HashedIdInHex(const std::string& id) {
  const std::string id_hash = base::SHA1HashString(id);
  DCHECK_EQ(base::kSHA1Length, id_hash.length());
  return base::HexEncode(id_hash.c_str(), id_hash.length());
}

base::FilePath MaybeNormalizePath(const base::FilePath& path) {
#if defined(OS_WIN)
  // Normalize any drive letter to upper-case. We do this for consistency with
  // net_utils::FilePathToFileURL(), which does the same thing, to make string
  // comparisons simpler.
  base::FilePath::StringType path_str = path.value();
  if (path_str.size() >= 2 && path_str[0] >= L'a' && path_str[0] <= L'z' &&
      path_str[1] == L':')
    path_str[0] = towupper(path_str[0]);

  return base::FilePath(path_str);
#else
  return path;
#endif
}

bool IdIsValid(const std::string& id) {
  // Verify that the id is legal.
  if (id.size() != (crx_file::id_util::kIdSize * 2))
    return false;

  // We only support lowercase IDs, because IDs can be used as URL components
  // (where GURL will lowercase it).
  std::string temp = base::ToLowerASCII(id);
  for (size_t i = 0; i < temp.size(); i++)
    if (temp[i] < 'a' || temp[i] > 'p')
      return false;

  return true;
}

}  // namespace id_util
}  // namespace crx_file
