// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_KEY_STORAGE_UTIL_LINUX_H_
#define COMPONENTS_OS_CRYPT_KEY_STORAGE_UTIL_LINUX_H_

#include <string>

#include "base/nix/xdg_util.h"

namespace os_crypt {

// The supported Linux backends for storing passwords.
enum class SelectedLinuxBackend {
  DEFER,  // No selection
  BASIC_TEXT,
  GNOME_ANY,  // GNOME_KEYRING or GNOME_LIBSECRET, whichever is available.
  GNOME_KEYRING,
  GNOME_LIBSECRET,
  KWALLET,
  KWALLET5,
};

// Decide which backend to target. |type| is checked first. If it does not
// match a supported backend, |desktop_env| will be used to decide.
// TODO(crbug/571003): This is exposed as a utility only for password manager to
// use. It should be merged into key_storage_linux, once no longer needed in
// password manager.
SelectedLinuxBackend SelectBackend(const std::string& type,
                                   base::nix::DesktopEnvironment desktop_env);

}  // namespace os_crypt

#endif  // COMPONENTS_OS_CRYPT_KEY_STORAGE_UTIL_LINUX_H_
