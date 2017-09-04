// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/credentialmanager/FederatedCredential.h"

#include "bindings/core/v8/ExceptionState.h"
#include "modules/credentialmanager/FederatedCredentialData.h"
#include "platform/credentialmanager/PlatformFederatedCredential.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebFederatedCredential.h"

namespace blink {

FederatedCredential* FederatedCredential::create(
    WebFederatedCredential* webFederatedCredential) {
  return new FederatedCredential(webFederatedCredential);
}

FederatedCredential* FederatedCredential::create(
    const FederatedCredentialData& data,
    ExceptionState& exceptionState) {
  if (data.id().isEmpty()) {
    exceptionState.throwTypeError("'id' must not be empty.");
    return nullptr;
  }
  if (data.provider().isEmpty()) {
    exceptionState.throwTypeError("'provider' must not be empty.");
    return nullptr;
  }

  KURL iconURL = parseStringAsURL(data.iconURL(), exceptionState);
  KURL providerURL = parseStringAsURL(data.provider(), exceptionState);
  if (exceptionState.hadException())
    return nullptr;
  return new FederatedCredential(data.id(), providerURL, data.name(), iconURL);
}

FederatedCredential::FederatedCredential(
    WebFederatedCredential* webFederatedCredential)
    : SiteBoundCredential(webFederatedCredential->getPlatformCredential()) {}

FederatedCredential::FederatedCredential(const String& id,
                                         const KURL& provider,
                                         const String& name,
                                         const KURL& icon)
    : SiteBoundCredential(
          PlatformFederatedCredential::create(id,
                                              SecurityOrigin::create(provider),
                                              name,
                                              icon)) {}

const String FederatedCredential::provider() const {
  return static_cast<PlatformFederatedCredential*>(m_platformCredential.get())
      ->provider()
      ->toString();
}

}  // namespace blink
