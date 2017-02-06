// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_tracker_service.h"

#include <stddef.h>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/signin_pref_names.h"

namespace {

const char kAccountKeyPath[] = "account_id";
const char kAccountEmailPath[] = "email";
const char kAccountGaiaPath[] = "gaia";
const char kAccountHostedDomainPath[] = "hd";
const char kAccountFullNamePath[] = "full_name";
const char kAccountGivenNamePath[] = "given_name";
const char kAccountLocalePath[] = "locale";
const char kAccountPictureURLPath[] = "picture_url";
const char kAccountChildAccountStatusPath[] = "is_child_account";

// TODO(M48): Remove deprecated preference migration.
const char kAccountServiceFlagsPath[] = "service_flags";

void RemoveDeprecatedServiceFlags(PrefService* pref_service) {
  ListPrefUpdate update(pref_service, AccountTrackerService::kAccountInfoPref);
  for (size_t i = 0; i < update->GetSize(); ++i) {
    base::DictionaryValue* dict = nullptr;
    if (update->GetDictionary(i, &dict))
      dict->RemoveWithoutPathExpansion(kAccountServiceFlagsPath, nullptr);
  }
}

}  // namespace

const char AccountTrackerService::kAccountInfoPref[] = "account_info";

const char AccountTrackerService::kChildAccountServiceFlag[] = "uca";

// This must be a string which can never be a valid domain.
const char AccountTrackerService::kNoHostedDomainFound[] = "NO_HOSTED_DOMAIN";

// This must be a string which can never be a valid picture URL.
const char AccountTrackerService::kNoPictureURLFound[] = "NO_PICTURE_URL";

AccountTrackerService::AccountTrackerService() : signin_client_(nullptr) {}

AccountTrackerService::~AccountTrackerService() {
}

// static
void AccountTrackerService::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(AccountTrackerService::kAccountInfoPref);
  registry->RegisterIntegerPref(prefs::kAccountIdMigrationState,
                                AccountTrackerService::MIGRATION_NOT_STARTED);
}

void AccountTrackerService::Initialize(SigninClient* signin_client) {
  DCHECK(signin_client);
  DCHECK(!signin_client_);
  signin_client_ = signin_client;
  LoadFromPrefs();
}

void AccountTrackerService::Shutdown() {
  signin_client_ = nullptr;
  accounts_.clear();
}

void AccountTrackerService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AccountTrackerService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

std::vector<AccountInfo> AccountTrackerService::GetAccounts() const {
  std::vector<AccountInfo> accounts;

  for (std::map<std::string, AccountState>::const_iterator it =
           accounts_.begin();
       it != accounts_.end();
       ++it) {
    const AccountState& state = it->second;
    accounts.push_back(state.info);
  }
  return accounts;
}

AccountInfo AccountTrackerService::GetAccountInfo(
    const std::string& account_id) const {
  std::map<std::string, AccountState>::const_iterator it =
      accounts_.find(account_id);
  if (it != accounts_.end())
    return it->second.info;

  return AccountInfo();
}

AccountInfo AccountTrackerService::FindAccountInfoByGaiaId(
    const std::string& gaia_id) const {
  if (!gaia_id.empty()) {
    for (std::map<std::string, AccountState>::const_iterator it =
             accounts_.begin();
         it != accounts_.end();
         ++it) {
      const AccountState& state = it->second;
      if (state.info.gaia == gaia_id)
        return state.info;
    }
  }

  return AccountInfo();
}

AccountInfo AccountTrackerService::FindAccountInfoByEmail(
    const std::string& email) const {
  if (!email.empty()) {
    for (std::map<std::string, AccountState>::const_iterator it =
             accounts_.begin();
         it != accounts_.end();
         ++it) {
      const AccountState& state = it->second;
      if (gaia::AreEmailsSame(state.info.email, email))
        return state.info;
    }
  }

  return AccountInfo();
}

AccountTrackerService::AccountIdMigrationState
AccountTrackerService::GetMigrationState() const {
  return GetMigrationState(signin_client_->GetPrefs());
}

void AccountTrackerService::SetMigrationState(AccountIdMigrationState state) {
  signin_client_->GetPrefs()->SetInteger(prefs::kAccountIdMigrationState,
                                         state);
}

void AccountTrackerService::SetMigrationDone() {
  SetMigrationState(MIGRATION_DONE);
}

// static
AccountTrackerService::AccountIdMigrationState
AccountTrackerService::GetMigrationState(const PrefService* pref_service) {
  return static_cast<AccountTrackerService::AccountIdMigrationState>(
      pref_service->GetInteger(prefs::kAccountIdMigrationState));
}

void AccountTrackerService::NotifyAccountUpdated(const AccountState& state) {
  DCHECK(!state.info.gaia.empty());
  FOR_EACH_OBSERVER(
      Observer, observer_list_, OnAccountUpdated(state.info));
}

void AccountTrackerService::NotifyAccountUpdateFailed(
    const std::string& account_id) {
  FOR_EACH_OBSERVER(
      Observer, observer_list_, OnAccountUpdateFailed(account_id));
}

void AccountTrackerService::NotifyAccountRemoved(const AccountState& state) {
  DCHECK(!state.info.gaia.empty());
  FOR_EACH_OBSERVER(
      Observer, observer_list_, OnAccountRemoved(state.info));
}

void AccountTrackerService::StartTrackingAccount(
    const std::string& account_id) {
  if (!ContainsKey(accounts_, account_id)) {
    DVLOG(1) << "StartTracking " << account_id;
    AccountState state;
    state.info.account_id = account_id;
    state.info.is_child_account = false;
    accounts_.insert(make_pair(account_id, state));
  }
}

void AccountTrackerService::StopTrackingAccount(const std::string& account_id) {
  DVLOG(1) << "StopTracking " << account_id;
  if (ContainsKey(accounts_, account_id)) {
    AccountState& state = accounts_[account_id];
    RemoveFromPrefs(state);
    if (!state.info.gaia.empty())
      NotifyAccountRemoved(state);

    accounts_.erase(account_id);
  }
}

void AccountTrackerService::SetAccountStateFromUserInfo(
    const std::string& account_id,
    const base::DictionaryValue* user_info) {
  DCHECK(ContainsKey(accounts_, account_id));
  AccountState& state = accounts_[account_id];

  std::string gaia_id;
  std::string email;
  if (user_info->GetString("id", &gaia_id) &&
      user_info->GetString("email", &email)) {
    state.info.gaia = gaia_id;
    state.info.email = email;

    std::string hosted_domain;
    if (user_info->GetString("hd", &hosted_domain) && !hosted_domain.empty()) {
      state.info.hosted_domain = hosted_domain;
    } else {
      state.info.hosted_domain = kNoHostedDomainFound;
    }

    user_info->GetString("name", &state.info.full_name);
    user_info->GetString("given_name", &state.info.given_name);
    user_info->GetString("locale", &state.info.locale);

    std::string picture_url;
    if(user_info->GetString("picture", &picture_url)) {
      state.info.picture_url = picture_url;
    } else {
      state.info.picture_url = kNoPictureURLFound;
    }
  }
  if (state.info.IsValid())
    NotifyAccountUpdated(state);
  SaveToPrefs(state);
}

void AccountTrackerService::SetIsChildAccount(const std::string& account_id,
                                              const bool& is_child_account) {
  DCHECK(ContainsKey(accounts_, account_id));
  AccountState& state = accounts_[account_id];
  if (state.info.is_child_account == is_child_account)
    return;
  state.info.is_child_account = is_child_account;
  if (state.info.IsValid())
    NotifyAccountUpdated(state);
  SaveToPrefs(state);
}

bool AccountTrackerService::IsMigratable() const {
#if !defined(OS_CHROMEOS)
  for (std::map<std::string, AccountState>::const_iterator it =
           accounts_.begin();
       it != accounts_.end(); ++it) {
    const AccountState& state = it->second;
    if ((it->first).empty() || state.info.gaia.empty())
      return false;
  }
  return true;
#else
  return false;
#endif
}

void AccountTrackerService::MigrateToGaiaId() {
  std::set<std::string> to_remove;
  std::map<std::string, AccountState> migrated_accounts;
  for (std::map<std::string, AccountState>::const_iterator it =
           accounts_.begin();
       it != accounts_.end(); ++it) {
    const AccountState& state = it->second;
    std::string account_id = it->first;
    if (account_id != state.info.gaia) {
      std::string new_account_id = state.info.gaia;
      if (!ContainsKey(accounts_, new_account_id)) {
        AccountState new_state = state;
        new_state.info.account_id = new_account_id;
        migrated_accounts.insert(make_pair(new_account_id, new_state));
        SaveToPrefs(new_state);
      }
      to_remove.insert(account_id);
    }
  }

  // Remove any obsolete account.
  for (auto account_id : to_remove) {
    if (ContainsKey(accounts_, account_id)) {
      AccountState& state = accounts_[account_id];
      RemoveFromPrefs(state);
      accounts_.erase(account_id);
    }
  }

  for (std::map<std::string, AccountState>::const_iterator it =
           migrated_accounts.begin();
       it != migrated_accounts.end(); ++it) {
    accounts_.insert(*it);
  }
}

void AccountTrackerService::LoadFromPrefs() {
  const base::ListValue* list =
      signin_client_->GetPrefs()->GetList(kAccountInfoPref);
  std::set<std::string> to_remove;
  bool contains_deprecated_service_flags = false;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    const base::DictionaryValue* dict;
    if (list->GetDictionary(i, &dict)) {
      base::string16 value;
      if (dict->GetString(kAccountKeyPath, &value)) {
        std::string account_id = base::UTF16ToUTF8(value);

        // Ignore incorrectly persisted non-canonical account ids.
        if (account_id.find('@') != std::string::npos &&
            account_id != gaia::CanonicalizeEmail(account_id)) {
          to_remove.insert(account_id);
          continue;
        }

        StartTrackingAccount(account_id);
        AccountState& state = accounts_[account_id];

        if (dict->GetString(kAccountGaiaPath, &value))
          state.info.gaia = base::UTF16ToUTF8(value);
        if (dict->GetString(kAccountEmailPath, &value))
          state.info.email = base::UTF16ToUTF8(value);
        if (dict->GetString(kAccountHostedDomainPath, &value))
          state.info.hosted_domain = base::UTF16ToUTF8(value);
        if (dict->GetString(kAccountFullNamePath, &value))
          state.info.full_name = base::UTF16ToUTF8(value);
        if (dict->GetString(kAccountGivenNamePath, &value))
          state.info.given_name = base::UTF16ToUTF8(value);
        if (dict->GetString(kAccountLocalePath, &value))
          state.info.locale = base::UTF16ToUTF8(value);
        if (dict->GetString(kAccountPictureURLPath, &value))
          state.info.picture_url = base::UTF16ToUTF8(value);

        bool is_child_account = false;
        // Migrate deprecated service flag preference.
        const base::ListValue* service_flags_list;
        if (dict->GetList(kAccountServiceFlagsPath, &service_flags_list)) {
          contains_deprecated_service_flags = true;
          std::string flag_string;
          for (const auto& flag : *service_flags_list) {
            if (flag->GetAsString(&flag_string) &&
                flag_string == kChildAccountServiceFlag) {
              is_child_account = true;
              break;
            }
          }
          state.info.is_child_account = is_child_account;
        }
        if (dict->GetBoolean(kAccountChildAccountStatusPath, &is_child_account))
          state.info.is_child_account = is_child_account;

        if (state.info.IsValid())
          NotifyAccountUpdated(state);
      }
    }
  }

  if (contains_deprecated_service_flags)
    RemoveDeprecatedServiceFlags(signin_client_->GetPrefs());

  // Remove any obsolete prefs.
  for (auto account_id : to_remove) {
    AccountState state;
    state.info.account_id = account_id;
    RemoveFromPrefs(state);
  }

  if (GetMigrationState() != MIGRATION_DONE) {
    if (IsMigratable()) {
      if (accounts_.empty()) {
        SetMigrationDone();
      } else {
        SetMigrationState(MIGRATION_IN_PROGRESS);
        MigrateToGaiaId();
      }
    }
  }
}

void AccountTrackerService::SaveToPrefs(const AccountState& state) {
  if (!signin_client_->GetPrefs())
    return;

  base::DictionaryValue* dict = nullptr;
  base::string16 account_id_16 = base::UTF8ToUTF16(state.info.account_id);
  ListPrefUpdate update(signin_client_->GetPrefs(), kAccountInfoPref);
  for (size_t i = 0; i < update->GetSize(); ++i, dict = nullptr) {
    if (update->GetDictionary(i, &dict)) {
      base::string16 value;
      if (dict->GetString(kAccountKeyPath, &value) && value == account_id_16)
        break;
    }
  }

  if (!dict) {
    dict = new base::DictionaryValue();
    update->Append(dict);  // |update| takes ownership.
    dict->SetString(kAccountKeyPath, account_id_16);
  }

  dict->SetString(kAccountEmailPath, state.info.email);
  dict->SetString(kAccountGaiaPath, state.info.gaia);
  dict->SetString(kAccountHostedDomainPath, state.info.hosted_domain);
  dict->SetString(kAccountFullNamePath, state.info.full_name);
  dict->SetString(kAccountGivenNamePath, state.info.given_name);
  dict->SetString(kAccountLocalePath, state.info.locale);
  dict->SetString(kAccountPictureURLPath, state.info.picture_url);
  dict->SetBoolean(kAccountChildAccountStatusPath, state.info.is_child_account);
}

void AccountTrackerService::RemoveFromPrefs(const AccountState& state) {
  if (!signin_client_->GetPrefs())
    return;

  base::string16 account_id_16 = base::UTF8ToUTF16(state.info.account_id);
  ListPrefUpdate update(signin_client_->GetPrefs(), kAccountInfoPref);
  for(size_t i = 0; i < update->GetSize(); ++i) {
    base::DictionaryValue* dict = nullptr;
    if (update->GetDictionary(i, &dict)) {
      base::string16 value;
      if (dict->GetString(kAccountKeyPath, &value) && value == account_id_16) {
        update->Remove(i, nullptr);
        break;
      }
    }
  }
}

std::string AccountTrackerService::PickAccountIdForAccount(
    const std::string& gaia,
    const std::string& email) const {
  return PickAccountIdForAccount(signin_client_->GetPrefs(), gaia, email);
}

// static
std::string AccountTrackerService::PickAccountIdForAccount(
    const PrefService* pref_service,
    const std::string& gaia,
    const std::string& email) {
  DCHECK(!gaia.empty() ||
      GetMigrationState(pref_service) == MIGRATION_NOT_STARTED);
  DCHECK(!email.empty());
  switch(GetMigrationState(pref_service)) {
    case MIGRATION_NOT_STARTED:
      // Some tests don't use a real email address.  To support these cases,
      // don't try to canonicalize these strings.
      return (email.find('@') == std::string::npos) ? email :
          gaia::CanonicalizeEmail(email);
    case MIGRATION_IN_PROGRESS:
    case MIGRATION_DONE:
      return gaia;
    default:
      NOTREACHED();
      return email;
  }
}

std::string AccountTrackerService::SeedAccountInfo(const std::string& gaia,
                                                   const std::string& email) {
  const std::string account_id = PickAccountIdForAccount(gaia, email);
  const bool already_exists = ContainsKey(accounts_, account_id);
  StartTrackingAccount(account_id);
  AccountState& state = accounts_[account_id];
  DCHECK(!already_exists || state.info.gaia.empty() || state.info.gaia == gaia);
  state.info.gaia = gaia;
  state.info.email = email;
  SaveToPrefs(state);

  DVLOG(1) << "AccountTrackerService::SeedAccountInfo"
           << " account_id=" << account_id
           << " gaia_id=" << gaia
           << " email=" << email;

  return account_id;
}

std::string AccountTrackerService::SeedAccountInfo(AccountInfo info) {
  info.account_id = PickAccountIdForAccount(info.gaia, info.email);

  if (!ContainsKey(accounts_, info.account_id)) {
    StartTrackingAccount(info.account_id);
  }

  AccountState& state = accounts_[info.account_id];
  // Update the missing fields in |state.info| with |info|.
  if (state.info.UpdateWith(info)) {
    if (state.info.IsValid()) {
      // Notify only if the account info is fully updated, as it is equivalent
      // to having the account fully fetched.
      NotifyAccountUpdated(state);
    }
    SaveToPrefs(state);
  }
  return info.account_id;
}

void AccountTrackerService::RemoveAccount(const std::string& account_id) {
  StopTrackingAccount(account_id);
}
