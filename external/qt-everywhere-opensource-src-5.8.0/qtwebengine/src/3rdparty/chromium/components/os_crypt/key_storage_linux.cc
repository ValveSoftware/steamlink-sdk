// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/key_storage_linux.h"

#include <string.h>

#include "base/environment.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"

#if defined(USE_LIBSECRET)
#include "components/os_crypt/key_storage_libsecret.h"
#endif

base::LazyInstance<std::string> g_store_ = LAZY_INSTANCE_INITIALIZER;

// static
void KeyStorageLinux::SetStore(const std::string& store_type) {
  g_store_.Get() = store_type;
  VLOG(1) << "OSCrypt store set to " << store_type;
}

// static
std::unique_ptr<KeyStorageLinux> KeyStorageLinux::CreateService() {
  base::nix::DesktopEnvironment used_desktop_env;
  if (g_store_.Get() == "kwallet") {
    used_desktop_env = base::nix::DESKTOP_ENVIRONMENT_KDE4;
  } else if (g_store_.Get() == "kwallet5") {
    used_desktop_env = base::nix::DESKTOP_ENVIRONMENT_KDE5;
  } else if (g_store_.Get() == "gnome") {
    used_desktop_env = base::nix::DESKTOP_ENVIRONMENT_GNOME;
  } else if (g_store_.Get() == "basic") {
    used_desktop_env = base::nix::DESKTOP_ENVIRONMENT_OTHER;
  } else {
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    used_desktop_env = base::nix::GetDesktopEnvironment(env.get());
  }

  // Try initializing the appropriate store for our environment.
  std::unique_ptr<KeyStorageLinux> key_storage;
  if (used_desktop_env == base::nix::DESKTOP_ENVIRONMENT_GNOME ||
      used_desktop_env == base::nix::DESKTOP_ENVIRONMENT_UNITY ||
      used_desktop_env == base::nix::DESKTOP_ENVIRONMENT_XFCE) {
#if defined(USE_LIBSECRET)
    key_storage.reset(new KeyStorageLibsecret());
    if (key_storage->Init()) {
      VLOG(1) << "OSCrypt using Libsecret as backend.";
      return key_storage;
    }
#endif
  }

  // The appropriate store was not available.
  VLOG(1) << "OSCrypt could not initialize a backend.";
  return nullptr;
}
