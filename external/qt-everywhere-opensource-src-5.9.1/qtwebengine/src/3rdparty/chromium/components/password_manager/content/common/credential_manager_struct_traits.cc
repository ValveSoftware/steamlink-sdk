// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/common/credential_manager_struct_traits.h"

#include "url/mojo/origin_struct_traits.h"
#include "url/mojo/url_gurl_struct_traits.h"

using namespace password_manager;

namespace mojo {

// static
mojom::CredentialType
EnumTraits<mojom::CredentialType, CredentialType>::ToMojom(
    CredentialType input) {
  switch (input) {
    case CredentialType::CREDENTIAL_TYPE_EMPTY:
      return mojom::CredentialType::EMPTY;
    case CredentialType::CREDENTIAL_TYPE_PASSWORD:
      return mojom::CredentialType::PASSWORD;
    case CredentialType::CREDENTIAL_TYPE_FEDERATED:
      return mojom::CredentialType::FEDERATED;
  }

  NOTREACHED();
  return mojom::CredentialType::EMPTY;
}

// static
bool EnumTraits<mojom::CredentialType, CredentialType>::FromMojom(
    mojom::CredentialType input,
    CredentialType* output) {
  switch (input) {
    case mojom::CredentialType::EMPTY:
      *output = CredentialType::CREDENTIAL_TYPE_EMPTY;
      return true;
    case mojom::CredentialType::PASSWORD:
      *output = CredentialType::CREDENTIAL_TYPE_PASSWORD;
      return true;
    case mojom::CredentialType::FEDERATED:
      *output = CredentialType::CREDENTIAL_TYPE_FEDERATED;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
bool StructTraits<mojom::CredentialInfoDataView, CredentialInfo>::Read(
    mojom::CredentialInfoDataView data,
    CredentialInfo* out) {
  if (data.ReadType(&out->type) && data.ReadId(&out->id) &&
      data.ReadName(&out->name) && data.ReadIcon(&out->icon) &&
      data.ReadPassword(&out->password) &&
      data.ReadFederation(&out->federation))
    return true;

  return false;
}

}  // namespace mojo
