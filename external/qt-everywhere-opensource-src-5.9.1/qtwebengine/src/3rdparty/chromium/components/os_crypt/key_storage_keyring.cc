// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/key_storage_keyring.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "components/os_crypt/keyring_util_linux.h"

namespace {

#if defined(GOOGLE_CHROME_BUILD)
const char kApplicationName[] = "chrome";
#else
const char kApplicationName[] = "chromium";
#endif

const GnomeKeyringPasswordSchema kSchema = {
    GNOME_KEYRING_ITEM_GENERIC_SECRET,
    {{"application", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING}, {nullptr}}};

}  // namespace

KeyStorageKeyring::KeyStorageKeyring(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner)
    : main_thread_runner_(main_thread_runner) {}

KeyStorageKeyring::~KeyStorageKeyring() {}

bool KeyStorageKeyring::Init() {
  return GnomeKeyringLoader::LoadGnomeKeyring();
}

std::string KeyStorageKeyring::GetKey() {
  std::string password;

  // Ensure GetKeyDelegate() is executed on the main thread.
  if (main_thread_runner_->BelongsToCurrentThread()) {
    GetKeyDelegate(&password, nullptr);
  } else {
    base::WaitableEvent password_loaded(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    main_thread_runner_->PostTask(
        FROM_HERE,
        base::Bind(&KeyStorageKeyring::GetKeyDelegate, base::Unretained(this),
                   &password, &password_loaded));
    password_loaded.Wait();
  }

  return password;
}

void KeyStorageKeyring::GetKeyDelegate(
    std::string* password_ptr,
    base::WaitableEvent* password_loaded_ptr) {
  gchar* password = nullptr;
  GnomeKeyringResult result =
      GnomeKeyringLoader::gnome_keyring_find_password_sync_ptr(
          &kSchema, &password, "application", kApplicationName, nullptr);
  if (result == GNOME_KEYRING_RESULT_OK) {
    *password_ptr = password;
    GnomeKeyringLoader::gnome_keyring_free_password_ptr(password);
  } else if (result == GNOME_KEYRING_RESULT_NO_MATCH) {
    *password_ptr = KeyStorageKeyring::AddRandomPasswordInKeyring();
    VLOG(1) << "OSCrypt generated a new password";
  } else {
    password_ptr->clear();
    VLOG(1) << "OSCrypt failed to use gnome-keyring";
  }

  if (password_loaded_ptr)
    password_loaded_ptr->Signal();
}

std::string KeyStorageKeyring::AddRandomPasswordInKeyring() {
  // Generate password
  std::string password;
  base::Base64Encode(base::RandBytesAsString(16), &password);

  // Store generated password
  GnomeKeyringResult result =
      GnomeKeyringLoader::gnome_keyring_store_password_sync_ptr(
          &kSchema, nullptr /* default keyring */, KeyStorageLinux::kKey,
          password.c_str(), "application", kApplicationName, nullptr);
  if (result != GNOME_KEYRING_RESULT_OK) {
    VLOG(1) << "Failed to store generated password to gnome-keyring";
    return std::string();
  }

  return password;
}
