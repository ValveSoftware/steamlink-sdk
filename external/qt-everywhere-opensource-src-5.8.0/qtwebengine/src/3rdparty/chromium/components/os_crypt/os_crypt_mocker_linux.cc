// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/os_crypt_mocker_linux.h"

#include "base/base64.h"
#include "base/lazy_instance.h"
#include "base/rand_util.h"
#include "components/os_crypt/os_crypt.h"

namespace {

static base::LazyInstance<OSCryptMockerLinux>::Leaky g_mocker =
    LAZY_INSTANCE_INITIALIZER;

KeyStorageLinux* GetKeyStorage() {
  return OSCryptMockerLinux::GetInstance();
}

std::string* GetPassword() {
  return OSCryptMockerLinux::GetInstance()->GetKeyPtr();
}

}  // namespace

std::string OSCryptMockerLinux::GetKey() {
  if (key_.empty())
    base::Base64Encode(base::RandBytesAsString(16), &key_);
  return key_;
}

bool OSCryptMockerLinux::Init() {
  return true;
}

void OSCryptMockerLinux::ResetTo(base::StringPiece new_key) {
  key_ = new_key.as_string();
}

std::string* OSCryptMockerLinux::GetKeyPtr() {
  return &key_;
}

// static
OSCryptMockerLinux* OSCryptMockerLinux::GetInstance() {
  return g_mocker.Pointer();
}

// static
void OSCryptMockerLinux::SetUpWithSingleton() {
  UseMockKeyStorageForTesting(&GetKeyStorage, &GetPassword);
}

// static
void OSCryptMockerLinux::TearDown() {
  UseMockKeyStorageForTesting(nullptr, nullptr);
}
