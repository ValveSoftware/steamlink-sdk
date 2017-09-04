// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_SUPPORT_SESSION_BLIMP_DEFAULT_IDENTITY_PROVIDER_H_
#define BLIMP_CLIENT_SUPPORT_SESSION_BLIMP_DEFAULT_IDENTITY_PROVIDER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "google_apis/gaia/identity_provider.h"

namespace blimp {
namespace client {

// Provides a dummy identity provider with a null token service that doesn't
// actually log in. Useful when using command-line switches to get the
// assignment.
class BlimpDefaultIdentityProvider : public IdentityProvider {
 public:
  BlimpDefaultIdentityProvider();
  ~BlimpDefaultIdentityProvider() override;

  // IdentityProvider implementation.
  std::string GetActiveUsername() override;
  std::string GetActiveAccountId() override;
  OAuth2TokenService* GetTokenService() override;
  bool RequestLogin() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpDefaultIdentityProvider);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_SUPPORT_SESSION_BLIMP_DEFAULT_IDENTITY_PROVIDER_H_
