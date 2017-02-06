// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/networking_private/networking_private_delegate.h"

#include "extensions/browser/api/networking_private/networking_private_api.h"

namespace extensions {

NetworkingPrivateDelegate::VerifyDelegate::VerifyDelegate() {
}

NetworkingPrivateDelegate::VerifyDelegate::~VerifyDelegate() {
}

NetworkingPrivateDelegate::UIDelegate::UIDelegate() {}

NetworkingPrivateDelegate::UIDelegate::~UIDelegate() {}

NetworkingPrivateDelegate::NetworkingPrivateDelegate(
    std::unique_ptr<VerifyDelegate> verify_delegate)
    : verify_delegate_(std::move(verify_delegate)) {}

NetworkingPrivateDelegate::~NetworkingPrivateDelegate() {
}

void NetworkingPrivateDelegate::AddObserver(
    NetworkingPrivateDelegateObserver* observer) {
  NOTREACHED() << "Class does not use NetworkingPrivateDelegateObserver";
}

void NetworkingPrivateDelegate::RemoveObserver(
    NetworkingPrivateDelegateObserver* observer) {
  NOTREACHED() << "Class does not use NetworkingPrivateDelegateObserver";
}

void NetworkingPrivateDelegate::StartActivate(
    const std::string& guid,
    const std::string& carrier,
    const VoidCallback& success_callback,
    const FailureCallback& failure_callback) {
  failure_callback.Run(networking_private::kErrorNotSupported);
}

void NetworkingPrivateDelegate::VerifyDestination(
    const VerificationProperties& verification_properties,
    const BoolCallback& success_callback,
    const FailureCallback& failure_callback) {
  if (!verify_delegate_) {
    failure_callback.Run(networking_private::kErrorNotSupported);
    return;
  }
  verify_delegate_->VerifyDestination(verification_properties, success_callback,
                                      failure_callback);
}

void NetworkingPrivateDelegate::VerifyAndEncryptCredentials(
    const std::string& guid,
    const VerificationProperties& verification_properties,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  if (!verify_delegate_) {
    failure_callback.Run(networking_private::kErrorNotSupported);
    return;
  }
  verify_delegate_->VerifyAndEncryptCredentials(
      guid, verification_properties, success_callback, failure_callback);
}

void NetworkingPrivateDelegate::VerifyAndEncryptData(
    const VerificationProperties& verification_properties,
    const std::string& data,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  if (!verify_delegate_) {
    failure_callback.Run(networking_private::kErrorNotSupported);
    return;
  }
  verify_delegate_->VerifyAndEncryptData(verification_properties, data,
                                         success_callback, failure_callback);
}

}  // namespace extensions
