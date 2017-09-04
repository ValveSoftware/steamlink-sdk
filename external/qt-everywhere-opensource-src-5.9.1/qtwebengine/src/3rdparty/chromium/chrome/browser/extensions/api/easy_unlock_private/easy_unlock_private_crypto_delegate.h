// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_CRYPTO_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_CRYPTO_DELEGATE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "chrome/common/extensions/api/easy_unlock_private.h"

namespace extensions {

// Wrapper around EasyUnlock dbus client on Chrome OS. The methods read
// extension function pearameters and invoke the associated EasyUnlock dbus
// client methods. On non-Chrome OS platforms, the methods are stubbed out.
class EasyUnlockPrivateCryptoDelegate {
 public:
  typedef base::Callback<void(const std::string& data)> DataCallback;

  typedef base::Callback<void(const std::string& public_key,
                              const std::string& private_key)>
      KeyPairCallback;

  virtual ~EasyUnlockPrivateCryptoDelegate() {}

  // Creates platform specific delegate instance. Non Chrome OS implementations
  // currently do nothing but invoke callbacks with empty data.
  static std::unique_ptr<EasyUnlockPrivateCryptoDelegate> Create();

  // See chromeos/dbus/easy_unlock_client.h for info on these methods.
  virtual void GenerateEcP256KeyPair(const KeyPairCallback& callback) = 0;
  virtual void PerformECDHKeyAgreement(
      const api::easy_unlock_private::PerformECDHKeyAgreement::Params& params,
      const DataCallback& callback) = 0;
  virtual void CreateSecureMessage(
      const api::easy_unlock_private::CreateSecureMessage::Params& params,
      const DataCallback& callback) = 0;
  virtual void UnwrapSecureMessage(
      const api::easy_unlock_private::UnwrapSecureMessage::Params& params,
      const DataCallback& callback) = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_CRYPTO_DELEGATE_H_
