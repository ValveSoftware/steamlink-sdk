// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/filename_util_unsafe.h"

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "net/base/filename_util_internal.h"

namespace {

// Local ICU-independent implementation of filename sanitizing functions defined
// in base/i18n/file_util_icu.h. Does not require ICU because on POSIX systems
// all international characters are considered legal, so only control and
// special characters have to be replaced.
const base::FilePath::CharType illegal_characters[] =
    FILE_PATH_LITERAL("\"*/:<>?\\\\|\001\002\003\004\005\006\007\010\011\012")
    FILE_PATH_LITERAL("\013\014\015\016\017\020\021\022\023\024\025\025\027");

void ReplaceIllegalCharactersInPath(base::FilePath::StringType* file_name,
                                    char replace_char) {
  base::ReplaceChars(*file_name,
                     illegal_characters,
                     base::FilePath::StringType(1, replace_char),
                     file_name);
}

}  // namespace

namespace net {

base::FilePath::StringType GenerateFileExtensionUnsafe(
    const GURL& url,
    const std::string& content_disposition,
    const std::string& referrer_charset,
    const std::string& suggested_name,
    const std::string& mime_type,
    const std::string& default_file_name) {
  base::FilePath filepath =
      GenerateFileNameImpl(url,
                           content_disposition,
                           referrer_charset,
                           suggested_name,
                           mime_type,
                           default_file_name,
                           base::Bind(&ReplaceIllegalCharactersInPath));
  return filepath.Extension();
}

}  // namespace net
