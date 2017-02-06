// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/common/signin_switches.h"

namespace switches {

// Clears the token service before using it. This allows simulating the
// expiration of credentials during testing.
const char kClearTokenService[] = "clear-token-service";

// Disables consistent identity features.
const char kDisableAccountConsistency[] = "disable-account-consistency";

// Disables new profile management system, including new profile chooser UI.
const char kDisableNewProfileManagement[] = "disable-new-profile-management";

// Disables sending signin scoped device id to LSO with refresh token request.
const char kDisableSigninScopedDeviceId[] = "disable-signin-scoped-device-id";

// Enables consistent identity features.
const char kEnableAccountConsistency[] = "enable-account-consistency";

// Enables new profile management system, including lock mode.
const char kEnableNewProfileManagement[] = "new-profile-management";

// Enables sending EnableRefreshTokenAnnotationRequest.
extern const char kEnableRefreshTokenAnnotationRequest[] =
    "enable-refresh-token-annotation-request";

// Enables multiple account versions of chrome.identity APIs.
const char kExtensionsMultiAccount[] = "extensions-multi-account";

// Enables using GAIA information to populate profile name and icon.
const char kGoogleProfileInfo[] = "google-profile-info";

const base::Feature kMaterialDesignUserManager {
  "MaterialDesignUserManager", base::FEATURE_DISABLED_BY_DEFAULT
};

// Enables or disables the material design desktop user menu.
const base::Feature kMaterialDesignUserMenu {
  "MaterialDesignUserMenu", base::FEATURE_DISABLED_BY_DEFAULT
};

// Enables or disables the new password separated sign in flow in a tab modal
// dialog.
const base::Feature kUsePasswordSeparatedSigninFlow {
  "UsePasswordSeparatedSigninFlow", base::FEATURE_ENABLED_BY_DEFAULT
};

}  // namespace switches
