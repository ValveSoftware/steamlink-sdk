// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_KEY_STORAGE_LINUX_H_
#define COMPONENTS_OS_CRYPT_KEY_STORAGE_LINUX_H_

#include <memory>
#include <string>

#include "base/macros.h"

// An API for retrieving OSCrypt's password from the system's password storage
// service.
class KeyStorageLinux {
 public:
  KeyStorageLinux() = default;
  virtual ~KeyStorageLinux() = default;

  // Force OSCrypt to use a specific linux password store.
  static void SetStore(const std::string& store_type);

  // Tries to load the appropriate key storage. Returns null if none succeed.
  static std::unique_ptr<KeyStorageLinux> CreateService();

  // Gets the encryption key from the OS password-managing library. If a key is
  // not found, a new key will be generated, stored and returned.
  virtual std::string GetKey() = 0;

 protected:
  // Loads the key storage. Returns false if the service is not available.
  virtual bool Init() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyStorageLinux);
};

#endif  // COMPONENTS_OS_CRYPT_KEY_STORAGE_LINUX_H_
