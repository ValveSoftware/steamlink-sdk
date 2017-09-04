// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_CRYPTO_VERIFY_IMPL_H_
#define CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_CRYPTO_VERIFY_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "extensions/browser/api/networking_private/networking_private_delegate.h"

namespace base {
class SequencedTaskRunner;
}

namespace extensions {

// Implementation of NetworkingPrivateDelegate::VerifyDelegate using
// networking_private_crypto.
class CryptoVerifyImpl : public NetworkingPrivateDelegate::VerifyDelegate {
 public:
  CryptoVerifyImpl();
  ~CryptoVerifyImpl() override;

  struct Credentials {
    // VerificationProperties are not copyable so define a struct that can be
    // passed to tasks on the worker thread.
    explicit Credentials(const VerificationProperties& properties);
    Credentials(const Credentials& other);
    ~Credentials();

    std::string certificate;
    std::vector<std::string> intermediate_certificates;
    std::string signed_data;
    std::string unsigned_data;
    std::string device_bssid;
    std::string public_key;
  };

  // NetworkingPrivateDelegate::VerifyDelegate
  void VerifyDestination(const VerificationProperties& verification_properties,
                         const BoolCallback& success_callback,
                         const FailureCallback& failure_callback) override;
  void VerifyAndEncryptCredentials(
      const std::string& guid,
      const VerificationProperties& verification_properties,
      const StringCallback& success_callback,
      const FailureCallback& failure_callback) override;
  void VerifyAndEncryptData(
      const VerificationProperties& verification_properties,
      const std::string& data,
      const StringCallback& success_callback,
      const FailureCallback& failure_callback) override;

 private:
  // Task runner for blocking tasks.
  scoped_refptr<base::SequencedTaskRunner> blocking_pool_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(CryptoVerifyImpl);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_CRYPTO_VERIFY_IMPL_H_
