// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_COOKIE_CRYPTO_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_COOKIE_CRYPTO_DELEGATE_H_

namespace content {

// Implements encryption and decryption for the persistent cookie store.
class CookieCryptoDelegate {
 public:
  virtual ~CookieCryptoDelegate() {}
  virtual bool EncryptString(const std::string& plaintext,
                             std::string* ciphertext) = 0;
  virtual bool DecryptString(const std::string& ciphertext,
                             std::string* plaintext) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_COOKIE_CRYPTO_DELEGATE_H_
