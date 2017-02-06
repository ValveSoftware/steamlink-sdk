// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_pending_request_task.h"

#include <algorithm>
#include <utility>

#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/affiliated_match_helper.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {
namespace {

// Send a UMA histogram about if |local_results| has empty or duplicate
// usernames.
void ReportAccountChooserMetrics(
    const ScopedVector<autofill::PasswordForm>& local_results,
    bool had_empty_username) {
  std::vector<base::string16> usernames;
  for (const auto& form : local_results)
    usernames.push_back(form->username_value);
  std::sort(usernames.begin(), usernames.end());
  bool has_duplicates =
      std::adjacent_find(usernames.begin(), usernames.end()) != usernames.end();
  metrics_util::AccountChooserUsabilityMetric metric;
  if (had_empty_username && has_duplicates)
    metric = metrics_util::ACCOUNT_CHOOSER_EMPTY_USERNAME_AND_DUPLICATES;
  else if (had_empty_username)
    metric = metrics_util::ACCOUNT_CHOOSER_EMPTY_USERNAME;
  else if (has_duplicates)
    metric = metrics_util::ACCOUNT_CHOOSER_DUPLICATES;
  else
    metric = metrics_util::ACCOUNT_CHOOSER_LOOKS_OK;
  metrics_util::LogAccountChooserUsability(metric);
}

}  // namespace

CredentialManagerPendingRequestTask::CredentialManagerPendingRequestTask(
    CredentialManagerPendingRequestTaskDelegate* delegate,
    const SendCredentialCallback& callback,
    bool request_zero_click_only,
    const GURL& request_origin,
    bool include_passwords,
    const std::vector<GURL>& request_federations,
    const std::vector<std::string>& affiliated_realms)
    : delegate_(delegate),
      send_callback_(callback),
      zero_click_only_(request_zero_click_only),
      origin_(request_origin),
      include_passwords_(include_passwords),
      affiliated_realms_(affiliated_realms.begin(), affiliated_realms.end()) {
  CHECK(!delegate_->client()->DidLastPageLoadEncounterSSLErrors());
  for (const GURL& federation : request_federations)
    federations_.insert(url::Origin(federation.GetOrigin()).Serialize());
}

CredentialManagerPendingRequestTask::~CredentialManagerPendingRequestTask() =
    default;

void CredentialManagerPendingRequestTask::OnGetPasswordStoreResults(
    ScopedVector<autofill::PasswordForm> results) {
  if (delegate_->GetOrigin() != origin_) {
    delegate_->SendCredential(send_callback_, CredentialInfo());
    return;
  }

  ScopedVector<autofill::PasswordForm> local_results;
  ScopedVector<autofill::PasswordForm> affiliated_results;
  ScopedVector<autofill::PasswordForm> federated_results;
  for (auto& form : results) {
    // Ensure that the form we're looking at matches the password and
    // federation filters provided.
    if (!((form->federation_origin.unique() && include_passwords_) ||
          (!form->federation_origin.unique() &&
           federations_.count(form->federation_origin.Serialize())))) {
      continue;
    }

    // PasswordFrom and GURL have different definition of origin.
    // PasswordForm definition: scheme, host, port and path.
    // GURL definition: scheme, host, and port.
    // So we can't compare them directly.
    if (form->origin.GetOrigin() == origin_.GetOrigin()) {
      local_results.push_back(form);
      form = nullptr;
    } else if (affiliated_realms_.count(form->signon_realm) &&
               AffiliatedMatchHelper::IsValidAndroidCredential(*form)) {
      form->is_affiliation_based_match = true;
      affiliated_results.push_back(form);
      form = nullptr;
    }

    // TODO(mkwst): We're debating whether or not federations ought to be
    // available at this point, as it's not clear that the user experience
    // is at all reasonable. Until that's resolved, we'll drop the forms that
    // match |federations_| on the floor rather than pushing them into
    // 'federated_results'. Since we don't touch the reference in |results|,
    // they will be safely deleted after this task executes.
  }

  if (!affiliated_results.empty()) {
    password_manager_util::TrimUsernameOnlyCredentials(&affiliated_results);
    local_results.insert(local_results.end(), affiliated_results.begin(),
                         affiliated_results.end());
    affiliated_results.weak_clear();
  }

  // Remove empty usernames from the list.
  auto begin_empty = std::partition(local_results.begin(), local_results.end(),
                                    [](autofill::PasswordForm* form) {
                                      return !form->username_value.empty();
                                    });
  const bool has_empty_username = (begin_empty != local_results.end());
  local_results.erase(begin_empty, local_results.end());

  if ((local_results.empty() && federated_results.empty())) {
    delegate_->SendCredential(send_callback_, CredentialInfo());
    return;
  }

  // We only perform zero-click sign-in when the result is completely
  // unambigious. If there is one and only one entry, and zero-click is
  // enabled for that entry, return it.
  //
  // Moreover, we only return such a credential if the user has opted-in via the
  // first-run experience.
  bool can_use_autosignin = local_results.size() == 1u &&
                            delegate_->IsZeroClickAllowed();
  if (can_use_autosignin && !local_results[0]->skip_zero_click &&
      !password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          delegate_->client()->GetPrefs())) {
    CredentialInfo info(*local_results[0],
                        local_results[0]->federation_origin.unique()
                            ? CredentialType::CREDENTIAL_TYPE_PASSWORD
                            : CredentialType::CREDENTIAL_TYPE_FEDERATED);
    delegate_->client()->NotifyUserAutoSignin(std::move(local_results),
                                              origin_);
    delegate_->SendCredential(send_callback_, info);
    return;
  }

  // Otherwise, return an empty credential if we're in zero-click-only mode
  // or if the user chooses not to return a credential, and the credential the
  // user chooses if they pick one.
  std::unique_ptr<autofill::PasswordForm> potential_autosignin_form(
      new autofill::PasswordForm(*local_results[0]));
  if (!zero_click_only_)
    ReportAccountChooserMetrics(local_results, has_empty_username);
  if (zero_click_only_ ||
      !delegate_->client()->PromptUserToChooseCredentials(
          std::move(local_results), std::move(federated_results), origin_,
          base::Bind(
              &CredentialManagerPendingRequestTaskDelegate::SendPasswordForm,
              base::Unretained(delegate_), send_callback_))) {
    if (can_use_autosignin) {
      // The user had credentials, but either chose not to share them with the
      // site, or was prevented from doing so by lack of zero-click (or the
      // first-run experience). So, notify the client that we could potentially
      // have used zero-click; if the user signs in with the same form via
      // autofill, we'll toggle the flag for them.
      delegate_->client()->NotifyUserCouldBeAutoSignedIn(
          std::move(potential_autosignin_form));
    }

    delegate_->SendCredential(send_callback_, CredentialInfo());
  }
}

}  // namespace password_manager
