// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_KEY_STORAGE_LIBSECRET_H_
#define COMPONENTS_OS_CRYPT_KEY_STORAGE_LIBSECRET_H_

#include <string>

#include "base/macros.h"
#include "components/os_crypt/key_storage_linux.h"

// Specialisation of KeyStorageLinux that uses Libsecret.
class KeyStorageLibsecret : public KeyStorageLinux {
 public:
  KeyStorageLibsecret() = default;
  ~KeyStorageLibsecret() override = default;

  // KeyStorageLinux
  std::string GetKey() override;

 protected:
  // KeyStorageLinux
  bool Init() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyStorageLibsecret);
};

#endif  // COMPONENTS_OS_CRYPT_KEY_STORAGE_LIBSECRET_H_
