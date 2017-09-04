// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/support/session/blimp_default_identity_provider.h"

#include "google_apis/gaia/oauth2_token_service.h"

namespace blimp {
namespace client {
namespace {
constexpr char kDummyAccountId[] = "DummyAccountId";
}  // namespace

BlimpDefaultIdentityProvider::BlimpDefaultIdentityProvider() = default;

BlimpDefaultIdentityProvider::~BlimpDefaultIdentityProvider() = default;

std::string BlimpDefaultIdentityProvider::GetActiveUsername() {
  return kDummyAccountId;
}

std::string BlimpDefaultIdentityProvider::GetActiveAccountId() {
  return kDummyAccountId;
}

OAuth2TokenService* BlimpDefaultIdentityProvider::GetTokenService() {
  return nullptr;
}

bool BlimpDefaultIdentityProvider::RequestLogin() {
  return false;
}

}  // namespace client
}  // namespace blimp
