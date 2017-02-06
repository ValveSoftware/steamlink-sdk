// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_default.h"

#include <set>
#include <utility>

#include "base/logging.h"
#include "base/stl_util.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/prefs/pref_service.h"

using autofill::PasswordForm;

namespace password_manager {

PasswordStoreDefault::PasswordStoreDefault(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
    scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner,
    std::unique_ptr<LoginDatabase> login_db)
    : PasswordStore(main_thread_runner, db_thread_runner),
      login_db_(std::move(login_db)) {}

PasswordStoreDefault::~PasswordStoreDefault() {
}

bool PasswordStoreDefault::Init(
    const syncer::SyncableService::StartSyncFlare& flare) {
  ScheduleTask(base::Bind(&PasswordStoreDefault::InitOnDBThread, this));
  return PasswordStore::Init(flare);
}

void PasswordStoreDefault::ShutdownOnUIThread() {
  PasswordStore::ShutdownOnUIThread();
  ScheduleTask(base::Bind(&PasswordStoreDefault::ResetLoginDB, this));
}

void PasswordStoreDefault::InitOnDBThread() {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  DCHECK(login_db_);
  if (!login_db_->Init()) {
    login_db_.reset();
    LOG(ERROR) << "Could not create/open login database.";
  }
}

void PasswordStoreDefault::ReportMetricsImpl(
    const std::string& sync_username,
    bool custom_passphrase_sync_enabled) {
  if (!login_db_)
    return;
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  login_db_->ReportMetrics(sync_username, custom_passphrase_sync_enabled);
}

PasswordStoreChangeList PasswordStoreDefault::AddLoginImpl(
    const PasswordForm& form) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  if (!login_db_)
    return PasswordStoreChangeList();
  return login_db_->AddLogin(form);
}

PasswordStoreChangeList PasswordStoreDefault::UpdateLoginImpl(
    const PasswordForm& form) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  if (!login_db_)
    return PasswordStoreChangeList();
  return login_db_->UpdateLogin(form);
}

PasswordStoreChangeList PasswordStoreDefault::RemoveLoginImpl(
    const PasswordForm& form) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  PasswordStoreChangeList changes;
  if (login_db_ && login_db_->RemoveLogin(form))
    changes.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE, form));
  return changes;
}

PasswordStoreChangeList PasswordStoreDefault::RemoveLoginsByURLAndTimeImpl(
    const base::Callback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end) {
  ScopedVector<autofill::PasswordForm> forms;
  PasswordStoreChangeList changes;
  if (login_db_ &&
      login_db_->GetLoginsCreatedBetween(delete_begin, delete_end, &forms)) {
    for (autofill::PasswordForm* form : forms) {
      if (url_filter.Run(form->origin) && login_db_->RemoveLogin(*form))
        changes.push_back(
            PasswordStoreChange(PasswordStoreChange::REMOVE, *form));
    }
    if (!changes.empty())
      LogStatsForBulkDeletion(changes.size());
  }
  return changes;
}

PasswordStoreChangeList PasswordStoreDefault::RemoveLoginsCreatedBetweenImpl(
    base::Time delete_begin,
    base::Time delete_end) {
  ScopedVector<autofill::PasswordForm> forms;
  PasswordStoreChangeList changes;
  if (login_db_ &&
      login_db_->GetLoginsCreatedBetween(delete_begin, delete_end, &forms)) {
    if (login_db_->RemoveLoginsCreatedBetween(delete_begin, delete_end)) {
      for (const auto* form : forms) {
        changes.push_back(
            PasswordStoreChange(PasswordStoreChange::REMOVE, *form));
      }
      LogStatsForBulkDeletion(changes.size());
    }
  }
  return changes;
}

PasswordStoreChangeList PasswordStoreDefault::RemoveLoginsSyncedBetweenImpl(
    base::Time delete_begin,
    base::Time delete_end) {
  ScopedVector<autofill::PasswordForm> forms;
  PasswordStoreChangeList changes;
  if (login_db_ &&
      login_db_->GetLoginsSyncedBetween(delete_begin, delete_end, &forms)) {
    if (login_db_->RemoveLoginsSyncedBetween(delete_begin, delete_end)) {
      for (const auto* form : forms) {
        changes.push_back(
            PasswordStoreChange(PasswordStoreChange::REMOVE, *form));
      }
      LogStatsForBulkDeletionDuringRollback(changes.size());
    }
  }
  return changes;
}

PasswordStoreChangeList
PasswordStoreDefault::DisableAutoSignInForAllLoginsImpl() {
  ScopedVector<autofill::PasswordForm> forms;
  PasswordStoreChangeList changes;
  if (login_db_ && login_db_->GetAutoSignInLogins(&forms)) {
    if (login_db_->DisableAutoSignInForAllLogins()) {
      for (const auto* form : forms) {
        changes.push_back(
            PasswordStoreChange(PasswordStoreChange::UPDATE, *form));
      }
    }
  }
  return changes;
}

bool PasswordStoreDefault::RemoveStatisticsCreatedBetweenImpl(
    base::Time delete_begin,
    base::Time delete_end) {
  return login_db_ &&
         login_db_->stats_table().RemoveStatsBetween(delete_begin, delete_end);
}

ScopedVector<autofill::PasswordForm> PasswordStoreDefault::FillMatchingLogins(
    const autofill::PasswordForm& form) {
  ScopedVector<autofill::PasswordForm> matched_forms;
  if (login_db_ && !login_db_->GetLogins(form, &matched_forms))
    return ScopedVector<autofill::PasswordForm>();
  return matched_forms;
}

bool PasswordStoreDefault::FillAutofillableLogins(
    ScopedVector<PasswordForm>* forms) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  return login_db_ && login_db_->GetAutofillableLogins(forms);
}

bool PasswordStoreDefault::FillBlacklistLogins(
    ScopedVector<PasswordForm>* forms) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  return login_db_ && login_db_->GetBlacklistLogins(forms);
}

void PasswordStoreDefault::AddSiteStatsImpl(const InteractionsStats& stats) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  if (login_db_)
    login_db_->stats_table().AddRow(stats);
}

void PasswordStoreDefault::RemoveSiteStatsImpl(const GURL& origin_domain) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  if (login_db_)
    login_db_->stats_table().RemoveRow(origin_domain);
}

std::vector<std::unique_ptr<InteractionsStats>>
PasswordStoreDefault::GetSiteStatsImpl(const GURL& origin_domain) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  return login_db_ ? login_db_->stats_table().GetRows(origin_domain)
                   : std::vector<std::unique_ptr<InteractionsStats>>();
}

void PasswordStoreDefault::ResetLoginDB() {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  login_db_.reset();
}

}  // namespace password_manager
