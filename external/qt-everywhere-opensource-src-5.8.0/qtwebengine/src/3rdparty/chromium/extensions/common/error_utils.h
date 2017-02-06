// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_ERROR_UTILS_H_
#define EXTENSIONS_COMMON_ERROR_UTILS_H_

#include <string>

#include "base/strings/string16.h"

namespace extensions {

class ErrorUtils {
 public:
  // Creates an error messages from a pattern.
  static std::string FormatErrorMessage(const std::string& format,
                                        const std::string& s1);

  static std::string FormatErrorMessage(const std::string& format,
                                        const std::string& s1,
                                        const std::string& s2);

  static std::string FormatErrorMessage(const std::string& format,
                                        const std::string& s1,
                                        const std::string& s2,
                                        const std::string& s3);

  static base::string16 FormatErrorMessageUTF16(const std::string& format,
                                                const std::string& s1);

  static base::string16 FormatErrorMessageUTF16(const std::string& format,
                                                const std::string& s1,
                                                const std::string& s2);

  static base::string16 FormatErrorMessageUTF16(const std::string& format,
                                                const std::string& s1,
                                                const std::string& s2,
                                                const std::string& s3);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_ERROR_UTILS_H_
