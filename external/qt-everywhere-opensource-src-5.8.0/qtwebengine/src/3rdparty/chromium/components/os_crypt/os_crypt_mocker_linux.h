// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_OS_CRYPT_MOCKER_LINUX_H_
#define COMPONENTS_OS_CRYPT_OS_CRYPT_MOCKER_LINUX_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "components/os_crypt/key_storage_linux.h"

// Holds and serves a password from memory.
class OSCryptMockerLinux : public KeyStorageLinux {
 public:
  OSCryptMockerLinux() = default;
  ~OSCryptMockerLinux() override = default;

  // KeyStorageLinux
  std::string GetKey() override;

  // Set the password that OSCryptMockerLinux holds.
  void ResetTo(base::StringPiece new_key);

  // Get a pointer to the stored password. OSCryptMockerLinux owns the pointer.
  std::string* GetKeyPtr();

  // Getter for the singleton.
  static OSCryptMockerLinux* GetInstance();

  // Inject the singleton mock into OSCrypt.
  static void SetUpWithSingleton();

  // Restore OSCrypt to its real behaviour.
  static void TearDown();

 protected:
  // KeyStorageLinux
  bool Init() override;

 private:
  std::string key_;

  DISALLOW_COPY_AND_ASSIGN(OSCryptMockerLinux);
};

#endif  // COMPONENTS_OS_CRYPT_OS_CRYPT_MOCKER_LINUX_H_
