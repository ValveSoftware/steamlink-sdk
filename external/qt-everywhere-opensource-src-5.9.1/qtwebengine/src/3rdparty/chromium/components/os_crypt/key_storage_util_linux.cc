// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/key_storage_util_linux.h"

#include "base/logging.h"

namespace os_crypt {

SelectedLinuxBackend SelectBackend(const std::string& type,
                                   base::nix::DesktopEnvironment desktop_env) {
  if (type == "kwallet")
    return SelectedLinuxBackend::KWALLET;
  if (type == "kwallet5")
    return SelectedLinuxBackend::KWALLET5;
  if (type == "gnome")
    return SelectedLinuxBackend::GNOME_ANY;
  if (type == "gnome-keyring")
    return SelectedLinuxBackend::GNOME_KEYRING;
  if (type == "gnome-libsecret")
    return SelectedLinuxBackend::GNOME_LIBSECRET;
  if (type == "basic")
    return SelectedLinuxBackend::BASIC_TEXT;

  const char* name = base::nix::GetDesktopEnvironmentName(desktop_env);
  VLOG(1) << "Password storage detected desktop environment: "
          << (name ? name : "(unknown)");

  // Detect the store to use automatically.
  switch (desktop_env) {
    case base::nix::DESKTOP_ENVIRONMENT_KDE4:
      return SelectedLinuxBackend::KWALLET;
    case base::nix::DESKTOP_ENVIRONMENT_KDE5:
      return SelectedLinuxBackend::KWALLET5;
    case base::nix::DESKTOP_ENVIRONMENT_GNOME:
    case base::nix::DESKTOP_ENVIRONMENT_UNITY:
    case base::nix::DESKTOP_ENVIRONMENT_XFCE:
      return SelectedLinuxBackend::GNOME_ANY;
    // KDE3 didn't use DBus, which our KWallet store uses.
    case base::nix::DESKTOP_ENVIRONMENT_KDE3:
    case base::nix::DESKTOP_ENVIRONMENT_OTHER:
      return SelectedLinuxBackend::BASIC_TEXT;
  }

  NOTREACHED();
  return SelectedLinuxBackend::BASIC_TEXT;
}

}  // namespace os_crypt
