// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/known_user.h"

#include <stddef.h>

#include <memory>

#include "base/logging.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace user_manager {
namespace known_user {
namespace {

// A vector pref of preferences of known users. All new preferences should be
// placed in this list.
const char kKnownUsers[] = "KnownUsers";

// Known user preferences keys (stored in Local State).

// Key of canonical e-mail value.
const char kCanonicalEmail[] = "email";

// Key of obfuscated GAIA id value.
const char kGAIAIdKey[] = "gaia_id";

// Key of whether this user ID refers to a SAML user.
const char kUsingSAMLKey[] = "using_saml";

// Key of Device Id.
const char kDeviceId[] = "device_id";

// Key of GAPS cookie.
const char kGAPSCookie[] = "gaps_cookie";

// Key of the reason for re-auth.
const char kReauthReasonKey[] = "reauth_reason";

// Key for the GaiaId migration status.
const char kGaiaIdMigration[] = "gaia_id_migration";

PrefService* GetLocalState() {
  if (!UserManager::IsInitialized())
    return nullptr;

  return UserManager::Get()->GetLocalState();
}

// Checks if values in |dict| correspond with |account_id| identity.
bool UserMatches(const AccountId& account_id,
                 const base::DictionaryValue& dict) {
  std::string value;

  // TODO(alemate): update code once user id is really a struct.
  bool has_gaia_id = dict.GetString(kGAIAIdKey, &value);
  if (has_gaia_id && account_id.GetGaiaId() == value)
    return true;

  bool has_email = dict.GetString(kCanonicalEmail, &value);
  if (has_email && account_id.GetUserEmail() == value)
    return true;

  return false;
}

// Fills relevant |dict| values based on |account_id|.
void UpdateIdentity(const AccountId& account_id, base::DictionaryValue& dict) {
  if (!account_id.GetUserEmail().empty())
    dict.SetString(kCanonicalEmail, account_id.GetUserEmail());

  if (!account_id.GetGaiaId().empty())
    dict.SetString(kGAIAIdKey, account_id.GetGaiaId());
}

}  // namespace

bool FindPrefs(const AccountId& account_id,
               const base::DictionaryValue** out_value) {
  PrefService* local_state = GetLocalState();

  // Local State may not be initialized in tests.
  if (!local_state)
    return false;

  // UserManager is usually NULL in unit tests.
  if (UserManager::IsInitialized() &&
      UserManager::Get()->IsUserNonCryptohomeDataEphemeral(account_id))
    return false;

  const base::ListValue* known_users = local_state->GetList(kKnownUsers);
  for (size_t i = 0; i < known_users->GetSize(); ++i) {
    const base::DictionaryValue* element = nullptr;
    if (known_users->GetDictionary(i, &element)) {
      if (UserMatches(account_id, *element)) {
        known_users->GetDictionary(i, out_value);
        return true;
      }
    }
  }
  return false;
}

void UpdatePrefs(const AccountId& account_id,
                 const base::DictionaryValue& values,
                 bool clear) {
  PrefService* local_state = GetLocalState();

  // Local State may not be initialized in tests.
  if (!local_state)
    return;

  // UserManager is usually NULL in unit tests.
  if (UserManager::IsInitialized() &&
      UserManager::Get()->IsUserNonCryptohomeDataEphemeral(account_id))
    return;

  ListPrefUpdate update(local_state, kKnownUsers);
  for (size_t i = 0; i < update->GetSize(); ++i) {
    base::DictionaryValue* element = nullptr;
    if (update->GetDictionary(i, &element)) {
      if (UserMatches(account_id, *element)) {
        if (clear)
          element->Clear();
        element->MergeDictionary(&values);
        UpdateIdentity(account_id, *element);
        return;
      }
    }
  }
  std::unique_ptr<base::DictionaryValue> new_value(new base::DictionaryValue());
  new_value->MergeDictionary(&values);
  UpdateIdentity(account_id, *new_value);
  update->Append(new_value.release());
}

bool GetStringPref(const AccountId& account_id,
                   const std::string& path,
                   std::string* out_value) {
  const base::DictionaryValue* user_pref_dict = nullptr;
  if (!FindPrefs(account_id, &user_pref_dict))
    return false;

  return user_pref_dict->GetString(path, out_value);
}

void SetStringPref(const AccountId& account_id,
                   const std::string& path,
                   const std::string& in_value) {
  PrefService* local_state = GetLocalState();

  // Local State may not be initialized in tests.
  if (!local_state)
    return;

  ListPrefUpdate update(local_state, kKnownUsers);
  base::DictionaryValue dict;
  dict.SetString(path, in_value);
  UpdatePrefs(account_id, dict, false);
}

bool GetBooleanPref(const AccountId& account_id,
                    const std::string& path,
                    bool* out_value) {
  const base::DictionaryValue* user_pref_dict = nullptr;
  if (!FindPrefs(account_id, &user_pref_dict))
    return false;

  return user_pref_dict->GetBoolean(path, out_value);
}

void SetBooleanPref(const AccountId& account_id,
                    const std::string& path,
                    const bool in_value) {
  PrefService* local_state = GetLocalState();

  // Local State may not be initialized in tests.
  if (!local_state)
    return;

  ListPrefUpdate update(local_state, kKnownUsers);
  base::DictionaryValue dict;
  dict.SetBoolean(path, in_value);
  UpdatePrefs(account_id, dict, false);
}

bool GetIntegerPref(const AccountId& account_id,
                    const std::string& path,
                    int* out_value) {
  const base::DictionaryValue* user_pref_dict = nullptr;
  if (!FindPrefs(account_id, &user_pref_dict))
    return false;
  return user_pref_dict->GetInteger(path, out_value);
}

void SetIntegerPref(const AccountId& account_id,
                    const std::string& path,
                    const int in_value) {
  PrefService* local_state = GetLocalState();

  // Local State may not be initialized in tests.
  if (!local_state)
    return;

  ListPrefUpdate update(local_state, kKnownUsers);
  base::DictionaryValue dict;
  dict.SetInteger(path, in_value);
  UpdatePrefs(account_id, dict, false);
}

AccountId GetAccountId(const std::string& user_email,
                       const std::string& gaia_id) {
  // In tests empty accounts are possible.
  if (user_email.empty() && gaia_id.empty())
    return EmptyAccountId();

  AccountId result(EmptyAccountId());
  // UserManager is usually NULL in unit tests.
  if (UserManager::IsInitialized() &&
      UserManager::Get()->GetPlatformKnownUserId(user_email, gaia_id,
                                                 &result)) {
    return result;
  }

  // We can have several users with the same gaia_id but different e-mails.
  // The opposite case is not possible.
  std::string stored_gaia_id;
  const std::string sanitized_email =
      user_email.empty()
          ? std::string()
          : gaia::CanonicalizeEmail(gaia::SanitizeEmail(user_email));

  if (!sanitized_email.empty() &&
      GetStringPref(AccountId::FromUserEmail(sanitized_email), kGAIAIdKey,
                    &stored_gaia_id)) {
    if (!gaia_id.empty() && gaia_id != stored_gaia_id)
      LOG(ERROR) << "User gaia id has changed. Sync will not work.";

    // gaia_id is associated with cryptohome.
    return AccountId::FromUserEmailGaiaId(sanitized_email, stored_gaia_id);
  }

  std::string stored_email;
  // GetStringPref() returns the first user record that matches
  // given ID. So we will get the first one if there are multiples.
  if (!gaia_id.empty() && GetStringPref(AccountId::FromGaiaId(gaia_id),
                                        kCanonicalEmail, &stored_email)) {
    return AccountId::FromUserEmailGaiaId(stored_email, gaia_id);
  }

  return (gaia_id.empty()
              ? AccountId::FromUserEmail(user_email)
              : AccountId::FromUserEmailGaiaId(user_email, gaia_id));
}

std::vector<AccountId> GetKnownAccountIds() {
  std::vector<AccountId> result;
  PrefService* local_state = GetLocalState();

  // Local State may not be initialized in tests.
  if (!local_state)
    return result;

  const base::ListValue* known_users = local_state->GetList(kKnownUsers);
  for (size_t i = 0; i < known_users->GetSize(); ++i) {
    const base::DictionaryValue* element = nullptr;
    if (known_users->GetDictionary(i, &element)) {
      std::string email;
      std::string gaia_id;
      const bool has_email = element->GetString(kCanonicalEmail, &email);
      const bool has_gaia_id = element->GetString(kGAIAIdKey, &gaia_id);
      if (has_email || has_gaia_id)
        result.push_back(AccountId::FromUserEmailGaiaId(email, gaia_id));
    }
  }
  return result;
}

bool GetGaiaIdMigrationStatus(const AccountId& account_id,
                              const std::string& subsystem) {
  bool migrated = false;

  if (GetBooleanPref(account_id,
                     std::string(kGaiaIdMigration) + "." + subsystem,
                     &migrated)) {
    return migrated;
  }

  return false;
}

void SetGaiaIdMigrationStatusDone(const AccountId& account_id,
                                  const std::string& subsystem) {
  SetBooleanPref(account_id, std::string(kGaiaIdMigration) + "." + subsystem,
                 true);
}

void UpdateGaiaID(const AccountId& account_id, const std::string& gaia_id) {
  SetStringPref(account_id, kGAIAIdKey, gaia_id);
}

bool FindGaiaID(const AccountId& account_id, std::string* out_value) {
  return GetStringPref(account_id, kGAIAIdKey, out_value);
}

void SetDeviceId(const AccountId& account_id, const std::string& device_id) {
  const std::string known_device_id = GetDeviceId(account_id);
  if (!known_device_id.empty() && device_id != known_device_id) {
    NOTREACHED() << "Trying to change device ID for known user.";
  }
  SetStringPref(account_id, kDeviceId, device_id);
}

std::string GetDeviceId(const AccountId& account_id) {
  std::string device_id;
  if (GetStringPref(account_id, kDeviceId, &device_id)) {
    return device_id;
  }
  return std::string();
}

void SetGAPSCookie(const AccountId& account_id,
                   const std::string& gaps_cookie) {
  SetStringPref(account_id, kGAPSCookie, gaps_cookie);
}

std::string GetGAPSCookie(const AccountId& account_id) {
  std::string gaps_cookie;
  if (GetStringPref(account_id, kGAPSCookie, &gaps_cookie)) {
    return gaps_cookie;
  }
  return std::string();
}

void UpdateUsingSAML(const AccountId& account_id, const bool using_saml) {
  SetBooleanPref(account_id, kUsingSAMLKey, using_saml);
}

bool IsUsingSAML(const AccountId& account_id) {
  bool using_saml;
  if (GetBooleanPref(account_id, kUsingSAMLKey, &using_saml))
    return using_saml;
  return false;
}

void UpdateReauthReason(const AccountId& account_id, const int reauth_reason) {
  SetIntegerPref(account_id, kReauthReasonKey, reauth_reason);
}

bool FindReauthReason(const AccountId& account_id, int* out_value) {
  return GetIntegerPref(account_id, kReauthReasonKey, out_value);
}

void RemovePrefs(const AccountId& account_id) {
  PrefService* local_state = GetLocalState();

  // Local State may not be initialized in tests.
  if (!local_state)
    return;

  ListPrefUpdate update(local_state, kKnownUsers);
  for (size_t i = 0; i < update->GetSize(); ++i) {
    base::DictionaryValue* element = nullptr;
    if (update->GetDictionary(i, &element)) {
      if (UserMatches(account_id, *element)) {
        update->Remove(i, nullptr);
        break;
      }
    }
  }
}

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kKnownUsers);
}

}  // namespace known_user
}  // namespace user_manager
