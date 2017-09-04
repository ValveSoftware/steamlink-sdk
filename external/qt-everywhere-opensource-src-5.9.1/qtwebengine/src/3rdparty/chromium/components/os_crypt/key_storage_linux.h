// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_KEY_STORAGE_LINUX_H_
#define COMPONENTS_OS_CRYPT_KEY_STORAGE_LINUX_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

// An API for retrieving OSCrypt's password from the system's password storage
// service.
class KeyStorageLinux {
 public:
  KeyStorageLinux() = default;
  virtual ~KeyStorageLinux() = default;

  // Force OSCrypt to use a specific linux password store.
  static void SetStore(const std::string& store_type);

  // The product name to use for permission prompts.
  static void SetProductName(const std::string& product_name);

  // A runner on the main thread for gnome-keyring to be called from.
  // TODO(crbug/466975): Libsecret and KWallet don't need this. We can remove
  // this when we stop supporting keyring.
  static void SetMainThreadRunner(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner);

  // Tries to load the appropriate key storage. Returns null if none succeed.
  static std::unique_ptr<KeyStorageLinux> CreateService();

  // Gets the encryption key from the OS password-managing library. If a key is
  // not found, a new key will be generated, stored and returned.
  virtual std::string GetKey() = 0;

 protected:
  // Loads the key storage. Returns false if the service is not available.
  virtual bool Init() = 0;

  // The name of the group, if any, containing the key.
  static const char kFolderName[];
  // The name of the entry with the encryption key.
  static const char kKey[];

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyStorageLinux);
};

#endif  // COMPONENTS_OS_CRYPT_KEY_STORAGE_LINUX_H_
