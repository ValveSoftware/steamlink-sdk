// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/key_storage_libsecret.h"

#include "base/base64.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/os_crypt/libsecret_util_linux.h"

namespace {

#if defined(GOOGLE_CHROME_BUILD)
const char kKeyStorageEntryName[] = "Chrome Safe Storage";
#else
const char kKeyStorageEntryName[] = "Chromium Safe Storage";
#endif

const SecretSchema kKeystoreSchema = {
    "chrome_libsecret_os_crypt_password",
    SECRET_SCHEMA_NONE,
    {
        {nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING},
    }};

std::string AddRandomPasswordInLibsecret() {
  std::string password;
  base::Base64Encode(base::RandBytesAsString(16), &password);
  GError* error = nullptr;
  LibsecretLoader::secret_password_store_sync(
      &kKeystoreSchema, nullptr, kKeyStorageEntryName, password.c_str(),
      nullptr, &error, nullptr);

  if (error) {
    VLOG(1) << "Libsecret lookup failed: " << error->message;
    return std::string();
  }
  return password;
}

}  // namespace

std::string KeyStorageLibsecret::GetKey() {
  GError* error = nullptr;
  LibsecretAttributesBuilder attrs;
  SecretValue* password_libsecret = LibsecretLoader::secret_service_lookup_sync(
      nullptr, &kKeystoreSchema, attrs.Get(), nullptr, &error);

  if (error) {
    VLOG(1) << "Libsecret lookup failed: " << error->message;
    g_error_free(error);
    return std::string();
  }
  if (!password_libsecret) {
    return AddRandomPasswordInLibsecret();
  }
  std::string password(
      LibsecretLoader::secret_value_get_text(password_libsecret));
  LibsecretLoader::secret_value_unref(password_libsecret);
  return password;
}

bool KeyStorageLibsecret::Init() {
  return LibsecretLoader::EnsureLibsecretLoaded();
}
