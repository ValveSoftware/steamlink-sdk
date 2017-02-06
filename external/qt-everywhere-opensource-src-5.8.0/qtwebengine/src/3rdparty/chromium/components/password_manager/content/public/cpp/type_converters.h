// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_PUBLIC_CPP_TYPE_CONVERTERS_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_PUBLIC_CPP_TYPE_CONVERTERS_IMPL_H_

#include <memory>

#include "components/password_manager/content/public/interfaces/credential_manager.mojom.h"

namespace blink {
class WebCredential;
}

namespace password_manager {
struct CredentialInfo;
}

namespace mojo {

template <>
struct TypeConverter<password_manager::mojom::CredentialInfoPtr,
                     password_manager::CredentialInfo> {
  static password_manager::mojom::CredentialInfoPtr Convert(
      const password_manager::CredentialInfo& input);
};

template <>
struct TypeConverter<password_manager::CredentialInfo,
                     password_manager::mojom::CredentialInfoPtr> {
  static password_manager::CredentialInfo Convert(
      const password_manager::mojom::CredentialInfoPtr& input);
};

template <>
struct TypeConverter<password_manager::mojom::CredentialInfoPtr,
                     blink::WebCredential> {
  static password_manager::mojom::CredentialInfoPtr Convert(
      const blink::WebCredential& input);
};

template <>
struct TypeConverter<std::unique_ptr<blink::WebCredential>,
                     password_manager::mojom::CredentialInfoPtr> {
  static std::unique_ptr<blink::WebCredential> Convert(
      const password_manager::mojom::CredentialInfoPtr& input);
};

}  // namespace mojo

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_PUBLIC_CPP_TYPE_CONVERTERS_IMPL_H_
