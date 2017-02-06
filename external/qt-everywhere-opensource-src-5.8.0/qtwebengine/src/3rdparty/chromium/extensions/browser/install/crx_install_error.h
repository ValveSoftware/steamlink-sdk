// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_INSTALL_CRX_INSTALL_ERROR_H_
#define EXTENSIONS_BROWSER_INSTALL_CRX_INSTALL_ERROR_H_

#include "base/strings/string16.h"

namespace extensions {

// Simple error class for CrxInstaller.
class CrxInstallError {
 public:
  // Typed errors that need to be handled specially by clients.
  // ERROR_OFF_STORE for disallowed off-store installations.
  // ERROR_DECLINED for situations when a .crx file seems to be OK, but there
  // are some policy restrictions or unmet dependencies that prevent it from
  // being installed.
  // ERROR_HASH_MISMATCH if the expected extension SHA256 hash sum is different
  // from the actual one.
  enum Type {
    ERROR_NONE,
    ERROR_OFF_STORE,
    ERROR_DECLINED,
    ERROR_HASH_MISMATCH,
    ERROR_OTHER
  };

  CrxInstallError() : type_(ERROR_NONE) {}

  explicit CrxInstallError(const base::string16& message)
      : type_(message.empty() ? ERROR_NONE : ERROR_OTHER), message_(message) {}

  CrxInstallError(Type type, const base::string16& message)
      : type_(type), message_(message) {}

  Type type() const { return type_; }
  const base::string16& message() const { return message_; }

 private:
  Type type_;
  base::string16 message_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_INSTALL_CRX_INSTALL_ERROR_H_
