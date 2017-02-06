// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/public/cpp/names.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace shell {

const char kNameType_Mojo[] = "mojo";
const char kNameType_Exe[] = "exe";

bool IsValidName(const std::string& name) {
  std::vector<std::string> parts =
      base::SplitString(name, ":", base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_ALL);
  if (parts.size() != 2)
    return false;

  if (parts.front().empty())
    return false;

  const std::string& path = parts.back();
  return !path.empty() &&
      !base::StartsWith(path, "//", base::CompareCase::INSENSITIVE_ASCII);
}

std::string GetNameType(const std::string& name) {
  std::vector<std::string> parts =
      base::SplitString(name, ":", base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_ALL);
  DCHECK(2 == parts.size());
  return parts.front();
}

std::string GetNamePath(const std::string& name) {
  std::vector<std::string> parts =
      base::SplitString(name, ":", base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_ALL);
  DCHECK(2 == parts.size());
  return parts.back();
}

}  // namespace shell
