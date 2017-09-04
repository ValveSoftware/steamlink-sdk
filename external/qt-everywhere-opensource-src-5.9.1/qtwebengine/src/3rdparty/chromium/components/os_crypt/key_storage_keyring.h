// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_KEY_STORAGE_KEYRING_H_
#define COMPONENTS_OS_CRYPT_KEY_STORAGE_KEYRING_H_

#include <string>

#include "base/macros.h"
#include "components/os_crypt/key_storage_linux.h"

namespace base {
class SingleThreadTaskRunner;
class WaitableEvent;
}  // namespace base

// Specialisation of KeyStorageLinux that uses Libsecret.
class KeyStorageKeyring : public KeyStorageLinux {
 public:
  explicit KeyStorageKeyring(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner);
  ~KeyStorageKeyring() override;

  // KeyStorageLinux
  std::string GetKey() override;

 protected:
  // KeyStorageLinux
  bool Init() override;

 private:
  // Gnome keyring requires calls to originate from the main thread.
  // This is the part of GetKey() that gets dispatched to the main thread.
  // The password is stored in |password_ptr|. If |password_loaded_ptr| is not
  // null, it will be signaled when |password_ptr| is safe to read.
  void GetKeyDelegate(std::string* password_ptr,
                      base::WaitableEvent* password_loaded_ptr);

  // Generate a random string and store it as OScrypt's new password.
  std::string AddRandomPasswordInKeyring();

  // Keyring calls need to originate from the main thread.
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner_;

  DISALLOW_COPY_AND_ASSIGN(KeyStorageKeyring);
};

#endif  // COMPONENTS_OS_CRYPT_KEY_STORAGE_KEYRING_H_
