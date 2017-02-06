// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/sync/browser/password_sync_util.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "url/origin.h"

using autofill::PasswordForm;
using url::Origin;

namespace password_manager {
namespace sync_util {

std::string GetSyncUsernameIfSyncingPasswords(
    const sync_driver::SyncService* sync_service,
    const SigninManagerBase* signin_manager) {
  if (!signin_manager)
    return std::string();

  // If sync is set up, return early if we aren't syncing passwords.
  if (sync_service &&
      !sync_service->GetPreferredDataTypes().Has(syncer::PASSWORDS)) {
    return std::string();
  }

  return signin_manager->GetAuthenticatedAccountInfo().email;
}

bool IsSyncAccountCredential(const autofill::PasswordForm& form,
                             const sync_driver::SyncService* sync_service,
                             const SigninManagerBase* signin_manager) {
  const Origin gaia_origin(GaiaUrls::GetInstance()->gaia_url().GetOrigin());
  if (!Origin(GURL(form.signon_realm)).IsSameOriginWith(gaia_origin)) {
    return false;
  }

  return gaia::AreEmailsSame(
      base::UTF16ToUTF8(form.username_value),
      GetSyncUsernameIfSyncingPasswords(sync_service, signin_manager));
}

}  // namespace sync_util
}  // namespace password_manager
