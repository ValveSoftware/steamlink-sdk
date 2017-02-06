// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/public/cpp/type_converters.h"

#include "base/logging.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "mojo/common/common_type_converters.h"
#include "third_party/WebKit/public/platform/WebCredential.h"
#include "third_party/WebKit/public/platform/WebFederatedCredential.h"
#include "third_party/WebKit/public/platform/WebPasswordCredential.h"

using namespace password_manager;

namespace mojo {

namespace {

mojom::CredentialType CMCredentialTypeToMojo(CredentialType type) {
  switch (type) {
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

CredentialType MojoCredentialTypeToCM(mojom::CredentialType type) {
  switch (type) {
    case mojom::CredentialType::EMPTY:
      return CredentialType::CREDENTIAL_TYPE_EMPTY;
    case mojom::CredentialType::PASSWORD:
      return CredentialType::CREDENTIAL_TYPE_PASSWORD;
    case mojom::CredentialType::FEDERATED:
      return CredentialType::CREDENTIAL_TYPE_FEDERATED;
  }

  NOTREACHED();
  return CredentialType::CREDENTIAL_TYPE_EMPTY;
}

}  // namespace

mojom::CredentialInfoPtr
TypeConverter<mojom::CredentialInfoPtr, CredentialInfo>::Convert(
    const CredentialInfo& input) {
  mojom::CredentialInfoPtr output(mojom::CredentialInfo::New());
  output->type = CMCredentialTypeToMojo(input.type);
  output->id = mojo::String::From(input.id);
  output->name = mojo::String::From(input.name);
  output->icon = input.icon;
  output->password = mojo::String::From(input.password);
  output->federation = input.federation;

  return output;
}

CredentialInfo TypeConverter<CredentialInfo, mojom::CredentialInfoPtr>::Convert(
    const mojom::CredentialInfoPtr& input) {
  CredentialInfo output;
  output.type = MojoCredentialTypeToCM(input->type);
  output.id = input->id.To<base::string16>();
  output.name = input->name.To<base::string16>();
  output.icon = input->icon;
  output.password = input->password.To<base::string16>();
  output.federation = input->federation;

  return output;
}

mojom::CredentialInfoPtr
TypeConverter<mojom::CredentialInfoPtr, blink::WebCredential>::Convert(
    const blink::WebCredential& input) {
  mojom::CredentialInfoPtr output(mojom::CredentialInfo::New());

  if (input.isPasswordCredential()) {
    // blink::WebPasswordCredential
    output->type = mojom::CredentialType::PASSWORD;
    output->password = mojo::String::From(base::string16(
        static_cast<const blink::WebPasswordCredential&>(input).password()));
  } else {
    DCHECK(input.isFederatedCredential());
    // blink::WebFederatedCredential
    output->type = mojom::CredentialType::FEDERATED;
    output->federation =
        static_cast<const blink::WebFederatedCredential&>(input).provider();
  }
  output->id = mojo::String::From(base::string16(input.id()));
  output->name = mojo::String::From(base::string16(input.name()));
  output->icon = input.iconURL();

  return output;
}

std::unique_ptr<blink::WebCredential> TypeConverter<
    std::unique_ptr<blink::WebCredential>,
    mojom::CredentialInfoPtr>::Convert(const mojom::CredentialInfoPtr& input) {
  std::unique_ptr<blink::WebCredential> output;

  switch (input->type) {
    case mojom::CredentialType::PASSWORD:
      output.reset(new blink::WebPasswordCredential(
          input->id.To<base::string16>(), input->password.To<base::string16>(),
          input->name.To<base::string16>(), input->icon));
      break;
    case mojom::CredentialType::FEDERATED:
      output.reset(new blink::WebFederatedCredential(
          input->id.To<base::string16>(), input->federation,
          input->name.To<base::string16>(), input->icon));
      break;
    case mojom::CredentialType::EMPTY:
      // Intentionally empty, return nullptr.
      break;
    default:
      NOTREACHED();
  }

  return output;
}

}  // namespace mojo
