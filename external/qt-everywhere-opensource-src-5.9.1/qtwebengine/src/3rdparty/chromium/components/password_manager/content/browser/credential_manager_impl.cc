// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/credential_manager_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/browser/affiliated_match_helper.h"
#include "components/password_manager/core/browser/credential_manager_logger.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "content/public/browser/web_contents.h"

namespace password_manager {

namespace {

void RunMojoGetCallback(const mojom::CredentialManager::GetCallback& callback,
                        const CredentialInfo& info) {
  callback.Run(mojom::CredentialManagerError::SUCCESS, info);
}

}  // namespace

// CredentialManagerImpl -------------------------------------------------

CredentialManagerImpl::CredentialManagerImpl(content::WebContents* web_contents,
                                             PasswordManagerClient* client)
    : WebContentsObserver(web_contents), client_(client), weak_factory_(this) {
  DCHECK(web_contents);
  auto_signin_enabled_.Init(prefs::kCredentialsEnableAutosignin,
                            client_->GetPrefs());
}

CredentialManagerImpl::~CredentialManagerImpl() {}

void CredentialManagerImpl::BindRequest(
    mojom::CredentialManagerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void CredentialManagerImpl::Store(const CredentialInfo& credential,
                                  const StoreCallback& callback) {
  DCHECK_NE(CredentialType::CREDENTIAL_TYPE_EMPTY, credential.type);

  if (password_manager_util::IsLoggingActive(client_)) {
    CredentialManagerLogger(client_->GetLogManager())
        .LogStoreCredential(web_contents()->GetLastCommittedURL(),
                            credential.type);
  }

  // Send acknowledge response back.
  callback.Run();

  if (!client_->IsSavingAndFillingEnabledForCurrentPage() ||
      !client_->OnCredentialManagerUsed())
    return;

  client_->NotifyStorePasswordCalled();

  GURL origin = web_contents()->GetLastCommittedURL().GetOrigin();
  std::unique_ptr<autofill::PasswordForm> form(
      CreatePasswordFormFromCredentialInfo(credential, origin));

  form_manager_.reset(new CredentialManagerPasswordFormManager(
      client_, GetDriver(), *CreateObservedPasswordFormFromOrigin(origin),
      std::move(form), this));
}

void CredentialManagerImpl::OnProvisionalSaveComplete() {
  DCHECK(form_manager_);
  DCHECK(client_->IsSavingAndFillingEnabledForCurrentPage());
  const autofill::PasswordForm& form = form_manager_->pending_credentials();

  if (!form.federation_origin.unique()) {
    // If this is a federated credential, check it against the federated matches
    // produced by the PasswordFormManager. If a match is found, update it and
    // return.
    for (const auto& match :
         form_manager_->form_fetcher()->GetFederatedMatches()) {
      if (match->username_value == form.username_value &&
          match->federation_origin.IsSameOriginWith(form.federation_origin)) {
        form_manager_->Update(*match);
        return;
      }
    }
  } else if (!form_manager_->IsNewLogin()) {
    // Otherwise, if this is not a new password credential, update the existing
    // credential without prompting the user. This will also update the
    // 'skip_zero_click' state, as we've gotten an explicit signal that the page
    // understands the credential management API and so can be trusted to notify
    // us when they sign the user out.
    form_manager_->Update(*form_manager_->preferred_match());
    return;
  }

  // Otherwise, this is a new form, so as the user if they'd like to save.
  client_->PromptUserToSaveOrUpdatePassword(
      std::move(form_manager_), CredentialSourceType::CREDENTIAL_SOURCE_API,
      false);
}

void CredentialManagerImpl::RequireUserMediation(
    const RequireUserMediationCallback& callback) {
  if (password_manager_util::IsLoggingActive(client_)) {
    CredentialManagerLogger(client_->GetLogManager())
        .LogRequireUserMediation(web_contents()->GetLastCommittedURL());
  }
  PasswordStore* store = GetPasswordStore();
  if (!store || !client_->IsSavingAndFillingEnabledForCurrentPage() ||
      !client_->OnCredentialManagerUsed()) {
    callback.Run();
    return;
  }

  if (store->affiliated_match_helper()) {
    store->affiliated_match_helper()->GetAffiliatedAndroidRealms(
        GetSynthesizedFormForOrigin(),
        base::Bind(&CredentialManagerImpl::ScheduleRequireMediationTask,
                   weak_factory_.GetWeakPtr(), callback));
  } else {
    std::vector<std::string> no_affiliated_realms;
    ScheduleRequireMediationTask(callback, no_affiliated_realms);
  }
}

void CredentialManagerImpl::ScheduleRequireMediationTask(
    const RequireUserMediationCallback& callback,
    const std::vector<std::string>& android_realms) {
  DCHECK(GetPasswordStore());
  if (!pending_require_user_mediation_) {
    pending_require_user_mediation_.reset(
        new CredentialManagerPendingRequireUserMediationTask(
            this, web_contents()->GetLastCommittedURL().GetOrigin(),
            android_realms));

    // This will result in a callback to
    // CredentialManagerPendingRequireUserMediationTask::
    // OnGetPasswordStoreResults().
    GetPasswordStore()->GetAutofillableLogins(
        pending_require_user_mediation_.get());
  } else {
    pending_require_user_mediation_->AddOrigin(
        web_contents()->GetLastCommittedURL().GetOrigin());
  }

  // Send acknowledge response back.
  callback.Run();
}

void CredentialManagerImpl::Get(bool zero_click_only,
                                bool include_passwords,
                                const std::vector<GURL>& federations,
                                const GetCallback& callback) {
  using metrics_util::LogCredentialManagerGetResult;
  metrics_util::CredentialManagerGetMediation mediation_status =
      zero_click_only ? metrics_util::CREDENTIAL_MANAGER_GET_UNMEDIATED
                      : metrics_util::CREDENTIAL_MANAGER_GET_MEDIATED;
  PasswordStore* store = GetPasswordStore();
  if (password_manager_util::IsLoggingActive(client_)) {
    CredentialManagerLogger(client_->GetLogManager())
        .LogRequestCredential(web_contents()->GetLastCommittedURL(),
                              zero_click_only, federations);
  }
  if (pending_request_ || !store) {
    // Callback error.
    callback.Run(pending_request_
                     ? mojom::CredentialManagerError::PENDINGREQUEST
                     : mojom::CredentialManagerError::PASSWORDSTOREUNAVAILABLE,
                 base::nullopt);
    LogCredentialManagerGetResult(metrics_util::CREDENTIAL_MANAGER_GET_REJECTED,
                                  mediation_status);
    return;
  }

  // Return an empty credential if the current page has TLS errors, or if the
  // page is being prerendered.
  if (!client_->IsFillingEnabledForCurrentPage() ||
      !client_->OnCredentialManagerUsed()) {
    callback.Run(mojom::CredentialManagerError::SUCCESS, CredentialInfo());
    LogCredentialManagerGetResult(metrics_util::CREDENTIAL_MANAGER_GET_NONE,
                                  mediation_status);
    return;
  }
  // Return an empty credential if zero-click is required but disabled.
  if (zero_click_only && !IsZeroClickAllowed()) {
    // Callback with empty credential info.
    callback.Run(mojom::CredentialManagerError::SUCCESS, CredentialInfo());
    LogCredentialManagerGetResult(
        metrics_util::CREDENTIAL_MANAGER_GET_NONE_ZERO_CLICK_OFF,
        mediation_status);
    return;
  }

  if (store->affiliated_match_helper()) {
    store->affiliated_match_helper()->GetAffiliatedAndroidRealms(
        GetSynthesizedFormForOrigin(),
        base::Bind(&CredentialManagerImpl::ScheduleRequestTask,
                   weak_factory_.GetWeakPtr(), callback, zero_click_only,
                   include_passwords, federations));
  } else {
    std::vector<std::string> no_affiliated_realms;
    ScheduleRequestTask(callback, zero_click_only, include_passwords,
                        federations, no_affiliated_realms);
  }
}

void CredentialManagerImpl::ScheduleRequestTask(
    const GetCallback& callback,
    bool zero_click_only,
    bool include_passwords,
    const std::vector<GURL>& federations,
    const std::vector<std::string>& android_realms) {
  DCHECK(GetPasswordStore());
  pending_request_.reset(new CredentialManagerPendingRequestTask(
      this, base::Bind(&RunMojoGetCallback, callback), zero_click_only,
      web_contents()->GetLastCommittedURL().GetOrigin(), include_passwords,
      federations, android_realms));

  // This will result in a callback to
  // PendingRequestTask::OnGetPasswordStoreResults().
  GetPasswordStore()->GetAutofillableLogins(pending_request_.get());
}

PasswordStore* CredentialManagerImpl::GetPasswordStore() {
  return client_ ? client_->GetPasswordStore() : nullptr;
}

bool CredentialManagerImpl::IsZeroClickAllowed() const {
  return *auto_signin_enabled_ && !client_->IsOffTheRecord();
}

GURL CredentialManagerImpl::GetOrigin() const {
  return web_contents()->GetLastCommittedURL().GetOrigin();
}

base::WeakPtr<PasswordManagerDriver> CredentialManagerImpl::GetDriver() {
  ContentPasswordManagerDriverFactory* driver_factory =
      ContentPasswordManagerDriverFactory::FromWebContents(web_contents());
  DCHECK(driver_factory);
  PasswordManagerDriver* driver =
      driver_factory->GetDriverForFrame(web_contents()->GetMainFrame());
  return driver->AsWeakPtr();
}

void CredentialManagerImpl::SendCredential(
    const SendCredentialCallback& send_callback,
    const CredentialInfo& info) {
  DCHECK(pending_request_);
  DCHECK(send_callback.Equals(pending_request_->send_callback()));

  if (password_manager_util::IsLoggingActive(client_)) {
    CredentialManagerLogger(client_->GetLogManager())
        .LogSendCredential(web_contents()->GetLastCommittedURL(), info.type);
  }
  send_callback.Run(info);
  pending_request_.reset();
}

void CredentialManagerImpl::SendPasswordForm(
    const SendCredentialCallback& send_callback,
    const autofill::PasswordForm* form) {
  CredentialInfo info;
  if (form) {
    password_manager::CredentialType type_to_return =
        form->federation_origin.unique()
            ? CredentialType::CREDENTIAL_TYPE_PASSWORD
            : CredentialType::CREDENTIAL_TYPE_FEDERATED;
    info = CredentialInfo(*form, type_to_return);
    if (PasswordStore* store = GetPasswordStore()) {
      if (form->skip_zero_click && IsZeroClickAllowed()) {
        autofill::PasswordForm update_form = *form;
        update_form.skip_zero_click = false;
        store->UpdateLogin(update_form);
      }
    }
    base::RecordAction(
        base::UserMetricsAction("CredentialManager_AccountChooser_Accepted"));
    metrics_util::LogCredentialManagerGetResult(
        metrics_util::CREDENTIAL_MANAGER_GET_ACCOUNT_CHOOSER,
        metrics_util::CREDENTIAL_MANAGER_GET_MEDIATED);
  } else {
    base::RecordAction(
        base::UserMetricsAction("CredentialManager_AccountChooser_Dismissed"));
    metrics_util::LogCredentialManagerGetResult(
        metrics_util::CREDENTIAL_MANAGER_GET_NONE,
        metrics_util::CREDENTIAL_MANAGER_GET_MEDIATED);
  }
  SendCredential(send_callback, info);
}

PasswordManagerClient* CredentialManagerImpl::client() const {
  return client_;
}

PasswordStore::FormDigest CredentialManagerImpl::GetSynthesizedFormForOrigin()
    const {
  PasswordStore::FormDigest digest = {
      autofill::PasswordForm::SCHEME_HTML, std::string(),
      web_contents()->GetLastCommittedURL().GetOrigin()};
  digest.signon_realm = digest.origin.spec();
  return digest;
}

void CredentialManagerImpl::DoneRequiringUserMediation() {
  DCHECK(pending_require_user_mediation_);
  pending_require_user_mediation_.reset();
}

}  // namespace password_manager
