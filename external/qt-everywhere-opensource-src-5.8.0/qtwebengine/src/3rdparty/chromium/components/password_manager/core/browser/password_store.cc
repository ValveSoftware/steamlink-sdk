// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store.h"

#include <utility>

#include "base/bind.h"
#include "base/debug/dump_without_crashing.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/affiliated_match_helper.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_syncable_service.h"
#include "components/password_manager/core/browser/statistics_table.h"

using autofill::PasswordForm;

namespace password_manager {

namespace {

// http://crbug.com/404012. Let's see where the empty fields come from.
void CheckForEmptyUsernameAndPassword(const PasswordForm& form) {
  if (form.username_value.empty() &&
      form.password_value.empty() &&
      !form.blacklisted_by_user)
    base::debug::DumpWithoutCrashing();
}

}  // namespace

PasswordStore::GetLoginsRequest::GetLoginsRequest(
    PasswordStoreConsumer* consumer)
    : consumer_weak_(consumer->GetWeakPtr()) {
  origin_task_runner_ = base::ThreadTaskRunnerHandle::Get();
}

PasswordStore::GetLoginsRequest::~GetLoginsRequest() {
}

void PasswordStore::GetLoginsRequest::NotifyConsumerWithResults(
    ScopedVector<autofill::PasswordForm> results) {
  if (!ignore_logins_cutoff_.is_null()) {
    ScopedVector<autofill::PasswordForm> remaining_logins;
    remaining_logins.reserve(results.size());
    for (auto& login : results) {
      if (login->date_created >= ignore_logins_cutoff_) {
        remaining_logins.push_back(login);
        login = nullptr;
      }
    }
    results = std::move(remaining_logins);
  }

  origin_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PasswordStoreConsumer::OnGetPasswordStoreResults,
                            consumer_weak_, base::Passed(&results)));
}

void PasswordStore::GetLoginsRequest::NotifyWithSiteStatistics(
    std::vector<std::unique_ptr<InteractionsStats>> stats) {
  auto passed_stats(base::WrapUnique(
      new std::vector<std::unique_ptr<InteractionsStats>>(std::move(stats))));
  origin_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PasswordStoreConsumer::OnGetSiteStatistics,
                            consumer_weak_, base::Passed(&passed_stats)));
}

PasswordStore::PasswordStore(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
    scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner)
    : main_thread_runner_(main_thread_runner),
      db_thread_runner_(db_thread_runner),
      observers_(new base::ObserverListThreadSafe<Observer>()),
      is_propagating_password_changes_to_web_credentials_enabled_(false),
      shutdown_called_(false) {
}

bool PasswordStore::Init(const syncer::SyncableService::StartSyncFlare& flare) {
  ScheduleTask(base::Bind(&PasswordStore::InitSyncableService, this, flare));
  return true;
}

void PasswordStore::SetAffiliatedMatchHelper(
    std::unique_ptr<AffiliatedMatchHelper> helper) {
  affiliated_match_helper_ = std::move(helper);
}

void PasswordStore::AddLogin(const PasswordForm& form) {
  CheckForEmptyUsernameAndPassword(form);
  ScheduleTask(base::Bind(&PasswordStore::AddLoginInternal, this, form));
}

void PasswordStore::UpdateLogin(const PasswordForm& form) {
  CheckForEmptyUsernameAndPassword(form);
  ScheduleTask(base::Bind(&PasswordStore::UpdateLoginInternal, this, form));
}

void PasswordStore::UpdateLoginWithPrimaryKey(
    const autofill::PasswordForm& new_form,
    const autofill::PasswordForm& old_primary_key) {
  CheckForEmptyUsernameAndPassword(new_form);
  ScheduleTask(base::Bind(&PasswordStore::UpdateLoginWithPrimaryKeyInternal,
                          this, new_form, old_primary_key));
}

void PasswordStore::RemoveLogin(const PasswordForm& form) {
  ScheduleTask(base::Bind(&PasswordStore::RemoveLoginInternal, this, form));
}

void PasswordStore::RemoveLoginsByURLAndTime(
    const base::Callback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    const base::Closure& completion) {
  ScheduleTask(base::Bind(&PasswordStore::RemoveLoginsByURLAndTimeInternal,
                          this, url_filter, delete_begin, delete_end,
                          completion));
}

void PasswordStore::RemoveLoginsCreatedBetween(
    base::Time delete_begin,
    base::Time delete_end,
    const base::Closure& completion) {
  ScheduleTask(base::Bind(&PasswordStore::RemoveLoginsCreatedBetweenInternal,
                          this, delete_begin, delete_end, completion));
}

void PasswordStore::RemoveLoginsSyncedBetween(base::Time delete_begin,
                                              base::Time delete_end) {
  ScheduleTask(base::Bind(&PasswordStore::RemoveLoginsSyncedBetweenInternal,
                          this, delete_begin, delete_end));
}

void PasswordStore::RemoveStatisticsCreatedBetween(
    base::Time delete_begin,
    base::Time delete_end,
    const base::Closure& completion) {
  ScheduleTask(
      base::Bind(&PasswordStore::RemoveStatisticsCreatedBetweenInternal, this,
                 delete_begin, delete_end, completion));
}

void PasswordStore::DisableAutoSignInForAllLogins(
    const base::Closure& completion) {
  ScheduleTask(base::Bind(&PasswordStore::DisableAutoSignInForAllLoginsInternal,
                          this, completion));
}

void PasswordStore::TrimAffiliationCache() {
  if (affiliated_match_helper_)
    affiliated_match_helper_->TrimAffiliationCache();
}

void PasswordStore::GetLogins(const PasswordForm& form,
                              PasswordStoreConsumer* consumer) {
  // Per http://crbug.com/121738, we deliberately ignore saved logins for
  // http*://www.google.com/ that were stored prior to 2012. (Google now uses
  // https://accounts.google.com/ for all login forms, so these should be
  // unused.) We don't delete them just yet, and they'll still be visible in the
  // password manager, but we won't use them to autofill any forms. This is a
  // security feature to help minimize damage that can be done by XSS attacks.
  // TODO(mdm): actually delete them at some point, say M24 or so.
  base::Time ignore_logins_cutoff;  // the null time
  if (form.scheme == PasswordForm::SCHEME_HTML &&
      (form.signon_realm == "http://www.google.com" ||
       form.signon_realm == "http://www.google.com/" ||
       form.signon_realm == "https://www.google.com" ||
       form.signon_realm == "https://www.google.com/")) {
    static const base::Time::Exploded exploded_cutoff =
        { 2012, 1, 0, 1, 0, 0, 0, 0 };  // 00:00 Jan 1 2012
    ignore_logins_cutoff = base::Time::FromUTCExploded(exploded_cutoff);
  }
  std::unique_ptr<GetLoginsRequest> request(new GetLoginsRequest(consumer));
  request->set_ignore_logins_cutoff(ignore_logins_cutoff);

  if (affiliated_match_helper_) {
    affiliated_match_helper_->GetAffiliatedAndroidRealms(
        form, base::Bind(&PasswordStore::ScheduleGetLoginsWithAffiliations,
                         this, form, base::Passed(&request)));
  } else {
    ScheduleTask(base::Bind(&PasswordStore::GetLoginsImpl, this, form,
                            base::Passed(&request)));
  }
}

void PasswordStore::GetAutofillableLogins(PasswordStoreConsumer* consumer) {
  Schedule(&PasswordStore::GetAutofillableLoginsImpl, consumer);
}

void PasswordStore::GetAutofillableLoginsWithAffiliatedRealms(
    PasswordStoreConsumer* consumer) {
  Schedule(&PasswordStore::GetAutofillableLoginsWithAffiliatedRealmsImpl,
           consumer);
}

void PasswordStore::GetBlacklistLogins(PasswordStoreConsumer* consumer) {
  Schedule(&PasswordStore::GetBlacklistLoginsImpl, consumer);
}

void PasswordStore::GetBlacklistLoginsWithAffiliatedRealms(
    PasswordStoreConsumer* consumer) {
  Schedule(&PasswordStore::GetBlacklistLoginsWithAffiliatedRealmsImpl,
           consumer);
}

void PasswordStore::ReportMetrics(const std::string& sync_username,
                                  bool custom_passphrase_sync_enabled) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner(
      GetBackgroundTaskRunner());
  if (task_runner) {
    base::Closure task =
        base::Bind(&PasswordStore::ReportMetricsImpl, this, sync_username,
                   custom_passphrase_sync_enabled);
    task_runner->PostDelayedTask(FROM_HERE, task,
                                 base::TimeDelta::FromSeconds(30));
  }
}

void PasswordStore::AddSiteStats(const InteractionsStats& stats) {
  ScheduleTask(base::Bind(&PasswordStore::AddSiteStatsImpl, this, stats));
}

void PasswordStore::RemoveSiteStats(const GURL& origin_domain) {
  ScheduleTask(
      base::Bind(&PasswordStore::RemoveSiteStatsImpl, this, origin_domain));
}

void PasswordStore::GetSiteStats(const GURL& origin_domain,
                                 PasswordStoreConsumer* consumer) {
  std::unique_ptr<GetLoginsRequest> request(new GetLoginsRequest(consumer));
  ScheduleTask(base::Bind(&PasswordStore::NotifySiteStats, this, origin_domain,
                          base::Passed(&request)));
}

void PasswordStore::AddObserver(Observer* observer) {
  observers_->AddObserver(observer);
}

void PasswordStore::RemoveObserver(Observer* observer) {
  observers_->RemoveObserver(observer);
}

bool PasswordStore::ScheduleTask(const base::Closure& task) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner(
      GetBackgroundTaskRunner());
  if (task_runner.get())
    return task_runner->PostTask(FROM_HERE, task);
  return false;
}

void PasswordStore::ShutdownOnUIThread() {
  ScheduleTask(base::Bind(&PasswordStore::DestroySyncableService, this));
  // The AffiliationService must be destroyed from the main thread.
  affiliated_match_helper_.reset();
  shutdown_called_ = true;
}

base::WeakPtr<syncer::SyncableService>
PasswordStore::GetPasswordSyncableService() {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  DCHECK(syncable_service_);
  return syncable_service_->AsWeakPtr();
}

PasswordStore::~PasswordStore() {
  DCHECK(shutdown_called_);
}

scoped_refptr<base::SingleThreadTaskRunner>
PasswordStore::GetBackgroundTaskRunner() {
  return db_thread_runner_;
}

void PasswordStore::GetLoginsImpl(const autofill::PasswordForm& form,
                                  std::unique_ptr<GetLoginsRequest> request) {
  request->NotifyConsumerWithResults(FillMatchingLogins(form));
}


void PasswordStore::LogStatsForBulkDeletion(int num_deletions) {
  UMA_HISTOGRAM_COUNTS("PasswordManager.NumPasswordsDeletedByBulkDelete",
                       num_deletions);
}

void PasswordStore::LogStatsForBulkDeletionDuringRollback(int num_deletions) {
  UMA_HISTOGRAM_COUNTS("PasswordManager.NumPasswordsDeletedDuringRollback",
                       num_deletions);
}

PasswordStoreChangeList PasswordStore::AddLoginSync(const PasswordForm& form) {
  // There is no good way to check if the password is actually up to date, or
  // at least to check if it was actually changed. Assume it is.
  if (AffiliatedMatchHelper::IsValidAndroidCredential(form))
    ScheduleFindAndUpdateAffiliatedWebLogins(form);
  return AddLoginImpl(form);
}

PasswordStoreChangeList PasswordStore::UpdateLoginSync(
    const PasswordForm& form) {
  if (AffiliatedMatchHelper::IsValidAndroidCredential(form)) {
    // Ideally, a |form| would not be updated in any way unless it was ensured
    // that it, as a whole, can be used for a successful login. This, sadly, can
    // not be guaranteed. It might be that |form| just contains updates to some
    // meta-attribute, while it still has an out-of-date password. If such a
    // password were to be propagated to affiliated credentials in that case, it
    // may very well overwrite the actual, up-to-date password. Try to mitigate
    // this risk by ignoring updates unless they actually update the password.
    std::unique_ptr<PasswordForm> old_form(GetLoginImpl(form));
    if (old_form && form.password_value != old_form->password_value)
      ScheduleFindAndUpdateAffiliatedWebLogins(form);
  }
  return UpdateLoginImpl(form);
}

PasswordStoreChangeList PasswordStore::RemoveLoginSync(
    const PasswordForm& form) {
  return RemoveLoginImpl(form);
}

void PasswordStore::NotifyLoginsChanged(
    const PasswordStoreChangeList& changes) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  if (!changes.empty()) {
    observers_->Notify(FROM_HERE, &Observer::OnLoginsChanged, changes);
    if (syncable_service_)
      syncable_service_->ActOnPasswordStoreChanges(changes);
  }
}

void PasswordStore::Schedule(
    void (PasswordStore::*func)(std::unique_ptr<GetLoginsRequest>),
    PasswordStoreConsumer* consumer) {
  std::unique_ptr<GetLoginsRequest> request(new GetLoginsRequest(consumer));
  consumer->cancelable_task_tracker()->PostTask(
      GetBackgroundTaskRunner().get(), FROM_HERE,
      base::Bind(func, this, base::Passed(&request)));
}

void PasswordStore::WrapModificationTask(ModificationTask task) {
  PasswordStoreChangeList changes = task.Run();
  NotifyLoginsChanged(changes);
}

void PasswordStore::AddLoginInternal(const PasswordForm& form) {
  PasswordStoreChangeList changes = AddLoginImpl(form);
  NotifyLoginsChanged(changes);
}

void PasswordStore::UpdateLoginInternal(const PasswordForm& form) {
  PasswordStoreChangeList changes = UpdateLoginImpl(form);
  NotifyLoginsChanged(changes);
}

void PasswordStore::RemoveLoginInternal(const PasswordForm& form) {
  PasswordStoreChangeList changes = RemoveLoginImpl(form);
  NotifyLoginsChanged(changes);
}

void PasswordStore::UpdateLoginWithPrimaryKeyInternal(
    const PasswordForm& new_form,
    const PasswordForm& old_primary_key) {
  PasswordStoreChangeList all_changes = RemoveLoginImpl(old_primary_key);
  PasswordStoreChangeList changes = AddLoginImpl(new_form);
  all_changes.insert(all_changes.end(), changes.begin(), changes.end());
  NotifyLoginsChanged(all_changes);
}

void PasswordStore::RemoveLoginsByURLAndTimeInternal(
    const base::Callback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    const base::Closure& completion) {
  PasswordStoreChangeList changes =
      RemoveLoginsByURLAndTimeImpl(url_filter, delete_begin, delete_end);
  NotifyLoginsChanged(changes);
  if (!completion.is_null())
    main_thread_runner_->PostTask(FROM_HERE, completion);
}

void PasswordStore::RemoveLoginsCreatedBetweenInternal(
    base::Time delete_begin,
    base::Time delete_end,
    const base::Closure& completion) {
  PasswordStoreChangeList changes =
      RemoveLoginsCreatedBetweenImpl(delete_begin, delete_end);
  NotifyLoginsChanged(changes);
  if (!completion.is_null())
    main_thread_runner_->PostTask(FROM_HERE, completion);
}

void PasswordStore::RemoveLoginsSyncedBetweenInternal(base::Time delete_begin,
                                                      base::Time delete_end) {
  PasswordStoreChangeList changes =
      RemoveLoginsSyncedBetweenImpl(delete_begin, delete_end);
  NotifyLoginsChanged(changes);
}

void PasswordStore::RemoveStatisticsCreatedBetweenInternal(
    base::Time delete_begin,
    base::Time delete_end,
    const base::Closure& completion) {
  RemoveStatisticsCreatedBetweenImpl(delete_begin, delete_end);
  if (!completion.is_null())
    main_thread_runner_->PostTask(FROM_HERE, completion);
}

void PasswordStore::DisableAutoSignInForAllLoginsInternal(
    const base::Closure& completion) {
  DisableAutoSignInForAllLoginsImpl();
  if (!completion.is_null())
    main_thread_runner_->PostTask(FROM_HERE, completion);
}

void PasswordStore::GetAutofillableLoginsImpl(
    std::unique_ptr<GetLoginsRequest> request) {
  ScopedVector<PasswordForm> obtained_forms;
  if (!FillAutofillableLogins(&obtained_forms))
    obtained_forms.clear();
  request->NotifyConsumerWithResults(std::move(obtained_forms));
}

void PasswordStore::GetAutofillableLoginsWithAffiliatedRealmsImpl(
    std::unique_ptr<GetLoginsRequest> request) {
  ScopedVector<PasswordForm> obtained_forms;
  if (!FillAutofillableLogins(&obtained_forms))
    obtained_forms.clear();
  // Since AffiliatedMatchHelper's requests should be sent from UI thread,
  // post a request to UI thread.
  main_thread_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PasswordStore::InjectAffiliatedWebRealms, this,
                 base::Passed(&obtained_forms), base::Passed(&request)));
}

void PasswordStore::NotifyLoginsWithAffiliatedRealms(
    std::unique_ptr<GetLoginsRequest> request,
    ScopedVector<PasswordForm> obtained_forms) {
  request->NotifyConsumerWithResults(std::move(obtained_forms));
}

void PasswordStore::GetBlacklistLoginsImpl(
    std::unique_ptr<GetLoginsRequest> request) {
  ScopedVector<PasswordForm> obtained_forms;
  if (!FillBlacklistLogins(&obtained_forms))
    obtained_forms.clear();
  request->NotifyConsumerWithResults(std::move(obtained_forms));
}

void PasswordStore::GetBlacklistLoginsWithAffiliatedRealmsImpl(
    std::unique_ptr<GetLoginsRequest> request) {
  ScopedVector<PasswordForm> obtained_forms;
  if (!FillBlacklistLogins(&obtained_forms))
    obtained_forms.clear();
  // Since AffiliatedMatchHelper's requests should be sent from UI thread,
  // post a request to UI thread.
  main_thread_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PasswordStore::InjectAffiliatedWebRealms, this,
                 base::Passed(&obtained_forms), base::Passed(&request)));
}

void PasswordStore::NotifySiteStats(const GURL& origin_domain,
                                    std::unique_ptr<GetLoginsRequest> request) {
  request->NotifyWithSiteStatistics(GetSiteStatsImpl(origin_domain));
}

void PasswordStore::GetLoginsWithAffiliationsImpl(
    const PasswordForm& form,
    std::unique_ptr<GetLoginsRequest> request,
    const std::vector<std::string>& additional_android_realms) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  ScopedVector<PasswordForm> results(FillMatchingLogins(form));
  for (const std::string& realm : additional_android_realms) {
    PasswordForm android_form;
    android_form.scheme = PasswordForm::SCHEME_HTML;
    android_form.signon_realm = realm;
    ScopedVector<PasswordForm> more_results(FillMatchingLogins(android_form));
    for (PasswordForm* form : more_results)
      form->is_affiliation_based_match = true;
    ScopedVector<PasswordForm>::iterator it_first_federated = std::partition(
        more_results.begin(), more_results.end(),
        [](PasswordForm* form) { return form->federation_origin.unique(); });
    more_results.erase(it_first_federated, more_results.end());
    password_manager_util::TrimUsernameOnlyCredentials(&more_results);
    results.insert(results.end(), more_results.begin(), more_results.end());
    more_results.weak_clear();
  }
  request->NotifyConsumerWithResults(std::move(results));
}

void PasswordStore::InjectAffiliatedWebRealms(
    ScopedVector<PasswordForm> forms,
    std::unique_ptr<GetLoginsRequest> request) {
  if (affiliated_match_helper_) {
    affiliated_match_helper_->InjectAffiliatedWebRealms(
        std::move(forms),
        base::Bind(&PasswordStore::GetLoginsRequest::NotifyConsumerWithResults,
                   base::Owned(request.release())));
  } else {
    request->NotifyConsumerWithResults(std::move(forms));
  }
}

void PasswordStore::ScheduleGetLoginsWithAffiliations(
    const PasswordForm& form,
    std::unique_ptr<GetLoginsRequest> request,
    const std::vector<std::string>& additional_android_realms) {
  ScheduleTask(base::Bind(&PasswordStore::GetLoginsWithAffiliationsImpl, this,
                          form, base::Passed(&request),
                          additional_android_realms));
}

std::unique_ptr<PasswordForm> PasswordStore::GetLoginImpl(
    const PasswordForm& primary_key) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  ScopedVector<PasswordForm> candidates(FillMatchingLogins(primary_key));
  for (PasswordForm*& candidate : candidates) {
    if (ArePasswordFormUniqueKeyEqual(*candidate, primary_key) &&
        !candidate->is_public_suffix_match) {
      std::unique_ptr<PasswordForm> result(candidate);
      candidate = nullptr;
      return result;
    }
  }
  return base::WrapUnique<PasswordForm>(nullptr);
}

void PasswordStore::FindAndUpdateAffiliatedWebLogins(
    const PasswordForm& added_or_updated_android_form) {
  if (!affiliated_match_helper_ ||
      !is_propagating_password_changes_to_web_credentials_enabled_) {
    return;
  }
  affiliated_match_helper_->GetAffiliatedWebRealms(
      added_or_updated_android_form,
      base::Bind(&PasswordStore::ScheduleUpdateAffiliatedWebLoginsImpl, this,
                 added_or_updated_android_form));
}

void PasswordStore::ScheduleFindAndUpdateAffiliatedWebLogins(
    const PasswordForm& added_or_updated_android_form) {
  main_thread_runner_->PostTask(
      FROM_HERE, base::Bind(&PasswordStore::FindAndUpdateAffiliatedWebLogins,
                            this, added_or_updated_android_form));
}

void PasswordStore::UpdateAffiliatedWebLoginsImpl(
    const PasswordForm& updated_android_form,
    const std::vector<std::string>& affiliated_web_realms) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  PasswordStoreChangeList all_changes;
  for (const std::string& affiliated_web_realm : affiliated_web_realms) {
    PasswordForm web_form_template;
    web_form_template.scheme = PasswordForm::SCHEME_HTML;
    web_form_template.signon_realm = affiliated_web_realm;
    ScopedVector<PasswordForm> web_logins(
        FillMatchingLogins(web_form_template));
    for (PasswordForm* web_login : web_logins) {
      // Do not update HTTP logins, logins saved under insecure conditions, and
      // non-HTML login forms; PSL matches; logins with a different username;
      // and logins with the same password (to avoid generating no-op updates).
      if (!AffiliatedMatchHelper::IsValidWebCredential(*web_login) ||
          web_login->is_public_suffix_match ||
          web_login->username_value != updated_android_form.username_value ||
          web_login->password_value == updated_android_form.password_value)
        continue;

      // If the |web_login| was updated in the same or a later chunk of Sync
      // changes, assume that it is more recent and do not update it. Note that
      // this check is far from perfect conflict resolution and mostly prevents
      // long-dormant Sync clients doing damage when they wake up in the face
      // of the following list of changes:
      //
      //   Time   Source     Change
      //   ====   ======     ======
      //   #1     Android    android_login.password_value = "A"
      //   #2     Client A   web_login.password_value = "A" (propagation)
      //   #3     Client A   web_login.password_value = "B" (manual overwrite)
      //
      // When long-dormant Sync client B wakes up, it will only get a distilled
      // subset of not-yet-obsoleted changes {1, 3}. In this case, client B must
      // not propagate password "A" to |web_login|. This is prevented as change
      // #3 will arrive either in the same/later chunk of sync changes, so the
      // |date_synced| of |web_login| value will be greater or equal.
      //
      // Note that this solution has several shortcomings:
      //
      //   (1) It will not prevent local changes to |web_login| from being
      //       overwritten if they were made shortly after start-up, before
      //       Sync changes are applied. This should be tolerable.
      //
      //   (2) It assumes that all Sync clients are fully capable of propagating
      //       changes to web credentials on their own. If client C runs an
      //       older version of Chrome and updates the password for |web_login|
      //       around the time when the |android_login| is updated, the updated
      //       password will not be propagated by client B to |web_login| when
      //       it wakes up, regardless of the temporal order of the original
      //       changes, as client B will see both credentials having the same
      //       |data_synced|.
      //
      //   (2a) Above could be mitigated by looking not only at |data_synced|,
      //        but also at the actual order of Sync changes.
      //
      //   (2b) However, (2a) is still not workable, as a Sync change is made
      //        when any attribute of the credential is updated, not only the
      //        password. Hence it is not possible for client B to distinguish
      //        between two following two event orders:
      //
      //    #1     Android    android_login.password_value = "A"
      //    #2     Client C   web_login.password_value = "B" (manual overwrite)
      //    #3     Android    android_login.random_attribute = "..."
      //
      //    #1     Client C   web_login.password_value = "B" (manual overwrite)
      //    #2     Android    android_login.password_value = "A"
      //
      //        And so it must assume that it is unsafe to update |web_login|.
      if (web_login->date_synced >= updated_android_form.date_synced)
        continue;

      web_login->password_value = updated_android_form.password_value;

      PasswordStoreChangeList changes = UpdateLoginImpl(*web_login);
      all_changes.insert(all_changes.end(), changes.begin(), changes.end());
    }
  }
  NotifyLoginsChanged(all_changes);
}

void PasswordStore::ScheduleUpdateAffiliatedWebLoginsImpl(
    const PasswordForm& updated_android_form,
    const std::vector<std::string>& affiliated_web_realms) {
  ScheduleTask(base::Bind(&PasswordStore::UpdateAffiliatedWebLoginsImpl, this,
                          updated_android_form, affiliated_web_realms));
}

void PasswordStore::InitSyncableService(
    const syncer::SyncableService::StartSyncFlare& flare) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  DCHECK(!syncable_service_);
  syncable_service_.reset(new PasswordSyncableService(this));
  syncable_service_->InjectStartSyncFlare(flare);
}

void PasswordStore::DestroySyncableService() {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  syncable_service_.reset();
}

}  // namespace password_manager
