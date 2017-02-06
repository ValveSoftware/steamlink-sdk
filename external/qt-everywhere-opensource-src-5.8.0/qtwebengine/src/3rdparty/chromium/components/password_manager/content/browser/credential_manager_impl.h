// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_CREDENTIAL_MANAGER_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_CREDENTIAL_MANAGER_IMPL_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/content/public/interfaces/credential_manager.mojom.h"
#include "components/password_manager/core/browser/credential_manager_password_form_manager.h"
#include "components/password_manager/core/browser/credential_manager_pending_request_task.h"
#include "components/password_manager/core/browser/credential_manager_pending_require_user_mediation_task.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/prefs/pref_member.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/binding_set.h"

class GURL;

namespace autofill {
struct PasswordForm;
}

namespace content {
class WebContents;
}

namespace password_manager {
class CredentialManagerPasswordFormManager;
class PasswordManagerClient;
class PasswordManagerDriver;
class PasswordStore;
struct CredentialInfo;

class CredentialManagerImpl
    : public mojom::CredentialManager,
      public content::WebContentsObserver,
      public CredentialManagerPasswordFormManagerDelegate,
      public CredentialManagerPendingRequestTaskDelegate,
      public CredentialManagerPendingRequireUserMediationTaskDelegate {
 public:
  CredentialManagerImpl(content::WebContents* web_contents,
                        PasswordManagerClient* client);
  ~CredentialManagerImpl() override;

  void BindRequest(mojom::CredentialManagerRequest request);

  // mojom::CredentialManager methods:
  void Store(mojom::CredentialInfoPtr credential,
             const StoreCallback& callback) override;
  void RequireUserMediation(
      const RequireUserMediationCallback& callback) override;
  void Get(bool zero_click_only,
           bool include_passwords,
           mojo::Array<GURL> federations,
           const GetCallback& callback) override;

  // CredentialManagerPendingRequestTaskDelegate:
  bool IsZeroClickAllowed() const override;
  GURL GetOrigin() const override;
  void SendCredential(const SendCredentialCallback& send_callback,
                      const CredentialInfo& info) override;
  void SendPasswordForm(const SendCredentialCallback& send_callback,
                        const autofill::PasswordForm* form) override;
  PasswordManagerClient* client() const override;
  autofill::PasswordForm GetSynthesizedFormForOrigin() const override;

  // CredentialManagerPendingSignedOutTaskDelegate:
  PasswordStore* GetPasswordStore() override;
  void DoneRequiringUserMediation() override;

  // CredentialManagerPasswordFormManagerDelegate:
  void OnProvisionalSaveComplete() override;

 private:
  // Returns the driver for the current main frame.
  // Virtual for testing.
  virtual base::WeakPtr<PasswordManagerDriver> GetDriver();

  // Schedules a CredentiaManagerPendingRequestTask (during
  // |OnRequestCredential()|) after the PasswordStore's AffiliationMatchHelper
  // grabs a list of realms related to the current web origin.
  void ScheduleRequestTask(const GetCallback& callback,
                           bool zero_click_only,
                           bool include_passwords,
                           const std::vector<GURL>& federations,
                           const std::vector<std::string>& android_realms);

  // Schedules a CredentialManagerPendingRequireUserMediationTask after the
  // AffiliationMatchHelper grabs a list of realms related to the current
  // web origin.
  void ScheduleRequireMediationTask(
      const RequireUserMediationCallback& callback,
      const std::vector<std::string>& android_realms);

  // Returns true iff it's OK to update credentials in the password store.
  bool IsUpdatingCredentialAllowed() const;

  PasswordManagerClient* client_;
  std::unique_ptr<CredentialManagerPasswordFormManager> form_manager_;

  // Set to false to disable automatic signing in.
  BooleanPrefMember auto_signin_enabled_;

  // When 'OnRequestCredential' is called, it in turn calls out to the
  // PasswordStore; we push enough data into Pending*Task objects so that
  // they can properly respond to the request once the PasswordStore gives
  // us data.
  std::unique_ptr<CredentialManagerPendingRequestTask> pending_request_;
  std::unique_ptr<CredentialManagerPendingRequireUserMediationTask>
      pending_require_user_mediation_;

  mojo::BindingSet<mojom::CredentialManager> bindings_;

  base::WeakPtrFactory<CredentialManagerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CredentialManagerImpl);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_CREDENTIAL_MANAGER_IMPL_H_
