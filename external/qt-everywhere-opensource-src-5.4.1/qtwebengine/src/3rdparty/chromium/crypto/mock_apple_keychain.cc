// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/time/time.h"
#include "crypto/mock_apple_keychain.h"

namespace crypto {

OSStatus MockAppleKeychain::FindGenericPassword(
    CFTypeRef keychainOrArray,
    UInt32 serviceNameLength,
    const char* serviceName,
    UInt32 accountNameLength,
    const char* accountName,
    UInt32* passwordLength,
    void** passwordData,
    SecKeychainItemRef* itemRef) const {
  // When simulating |noErr|, return canned |passwordData| and
  // |passwordLength|.  Otherwise, just return given code.
  if (find_generic_result_ == noErr) {
    static const char kPassword[] = "my_password";
    DCHECK(passwordData);
    // The function to free this data is mocked so the cast is fine.
    *passwordData = const_cast<char*>(kPassword);
    DCHECK(passwordLength);
    *passwordLength = arraysize(kPassword);
    password_data_count_++;
  }

  return find_generic_result_;
}

OSStatus MockAppleKeychain::ItemFreeContent(SecKeychainAttributeList* attrList,
                                            void* data) const {
  // No-op.
  password_data_count_--;
  return noErr;
}

OSStatus MockAppleKeychain::AddGenericPassword(
    SecKeychainRef keychain,
    UInt32 serviceNameLength,
    const char* serviceName,
    UInt32 accountNameLength,
    const char* accountName,
    UInt32 passwordLength,
    const void* passwordData,
    SecKeychainItemRef* itemRef) const {
  called_add_generic_ = true;

  DCHECK_GT(passwordLength, 0U);
  DCHECK(passwordData);
  add_generic_password_ =
      std::string(const_cast<char*>(static_cast<const char*>(passwordData)),
                  passwordLength);
  return noErr;
}

}  // namespace crypto
