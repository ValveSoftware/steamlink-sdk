// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_CLIENT_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_CLIENT_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/password_store.h"

class PrefService;

namespace autofill {
class AutofillManager;
}

namespace password_manager {

class LogManager;
class PasswordFormManager;
class PasswordManager;
class PasswordManagerDriver;
class PasswordStore;

enum PasswordSyncState {
  NOT_SYNCING_PASSWORDS,
  SYNCING_NORMAL_ENCRYPTION,
  SYNCING_WITH_CUSTOM_PASSPHRASE
};

enum class CredentialSourceType {
  CREDENTIAL_SOURCE_PASSWORD_MANAGER = 0,
  CREDENTIAL_SOURCE_API,
  CREDENTIAL_SOURCE_LAST = CREDENTIAL_SOURCE_API
};

// An abstraction of operations that depend on the embedders (e.g. Chrome)
// environment.
class PasswordManagerClient {
 public:
  using CredentialsCallback =
      base::Callback<void(const autofill::PasswordForm*)>;

  PasswordManagerClient() {}
  virtual ~PasswordManagerClient() {}

  // For automated testing, the save password prompt should sometimes not be
  // shown, and password immediately saved instead. That can be enforced by
  // a command-line flag. If auto-saving is enforced, this method returns true.
  // The default return value is false.
  virtual bool IsAutomaticPasswordSavingEnabled() const;

  // Is saving new data for password autofill and filling of saved data enabled
  // for the current profile and page? For example, saving is disabled in
  // Incognito mode.
  virtual bool IsSavingAndFillingEnabledForCurrentPage() const;

  // Checks if filling is enabled for the current page. Filling is disabled when
  // password manager is disabled, or in the presence of SSL errors on a page.
  virtual bool IsFillingEnabledForCurrentPage() const;

  // Informs the embedder of a password form that can be saved or updated in
  // password store if the user allows it. The embedder is not required to
  // prompt the user if it decides that this form doesn't need to be saved or
  // updated. Returns true if the prompt was indeed displayed.
  // There are 3 different cases when |update_password| == true:
  // 1.A change password form was submitted and the user has only one stored
  // credential. Then form_to_save.pending_credentials() should correspond to
  // the unique element from |form_to_save.best_matches_|.
  // 2.A change password form was submitted and the user has more than one
  // stored credential. Then we shouldn't expect anything from
  // form_to_save.pending_credentials() except correct origin, since we don't
  // know which credentials should be updated.
  // 3.A sign-in password form was submitted with a password different from
  // the stored one. In this case form_to_save.password_overridden() == true
  // and form_to_save.pending_credentials() should correspond to the credential
  // that was overidden.
  // TODO(crbug.com/576747): Analyze usefulness of the |type| parameter, make a
  // decision if it should be kept or removed.
  virtual bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<PasswordFormManager> form_to_save,
      CredentialSourceType type,
      bool update_password) = 0;

  // Informs the embedder of a password forms that the user should choose from.
  // Returns true if the prompt is indeed displayed. If the prompt is not
  // displayed, returns false and does not call |callback|.
  // |callback| should be invoked with the chosen form.
  virtual bool PromptUserToChooseCredentials(
      ScopedVector<autofill::PasswordForm> local_forms,
      ScopedVector<autofill::PasswordForm> federated_forms,
      const GURL& origin,
      const CredentialsCallback& callback) = 0;

  // Informs the embedder that the user has manually requested to save the
  // password in the focused password field.
  virtual void ForceSavePassword();

  // Informs the embedder that the user has manually requested to generate a
  // password in the focused password field.
  virtual void GeneratePassword();

  // Informs the embedder that automatic signing in just happened. The form
  // returned to the site is |local_forms[0]|. |local_forms| contains all the
  // local credentials for the site. |origin| is a URL of the site the user was
  // auto signed in to.
  virtual void NotifyUserAutoSignin(
      ScopedVector<autofill::PasswordForm> local_forms,
      const GURL& origin) = 0;

  // Inform the embedder that automatic signin would have happened if the user
  // had been through the first-run experience to ensure their opt-in. |form|
  // contains the PasswordForm that would have been delivered.
  virtual void NotifyUserCouldBeAutoSignedIn(
      std::unique_ptr<autofill::PasswordForm> form) = 0;

  // Inform the embedder that the user signed in with a saved credential.
  // |form| contains the form used.
  virtual void NotifySuccessfulLoginWithExistingPassword(
      const autofill::PasswordForm& form) = 0;

  // Inform the embedder that the site called 'store()'.
  virtual void NotifyStorePasswordCalled() = 0;

  // Called when a password is saved in an automated fashion. Embedder may
  // inform the user that this save has occured.
  virtual void AutomaticPasswordSave(
      std::unique_ptr<PasswordFormManager> saved_form_manager) = 0;

  // Called when a password is autofilled. |best_matches| contains the
  // PasswordForm into which a password was filled: the client may choose to
  // save this to the PasswordStore, for example. |origin| is the origin of the
  // form into which a password was filled. |federated_matches| are the stored
  // federated matches relevant to the filled form, this argument may be null.
  // They are never filled, but might be needed in the UI, for example. Default
  // implementation is a noop.
  virtual void PasswordWasAutofilled(
      const autofill::PasswordFormMap& best_matches,
      const GURL& origin,
      const std::vector<std::unique_ptr<autofill::PasswordForm>>*
          federated_matches) const;

  // Gets prefs associated with this embedder.
  virtual PrefService* GetPrefs() = 0;

  // Returns the PasswordStore associated with this instance.
  virtual PasswordStore* GetPasswordStore() const = 0;

  // Reports whether and how passwords are synced in the embedder. The default
  // implementation always returns NOT_SYNCING_PASSWORDS.
  // TODO(vabr): Factor this out of the client to the sync layer.
  virtual PasswordSyncState GetPasswordSyncState() const;

  // Returns true if last navigation page had HTTP error i.e 5XX or 4XX
  virtual bool WasLastNavigationHTTPError() const;

  // Returns whether any SSL certificate errors were encountered as a result of
  // the last page load.
  virtual bool DidLastPageLoadEncounterSSLErrors() const;

  // If this browsing session should not be persisted.
  virtual bool IsOffTheRecord() const;

  // Returns the PasswordManager associated with this client. The non-const
  // version calls the const one.
  PasswordManager* GetPasswordManager();
  virtual const PasswordManager* GetPasswordManager() const;

  // Returns the AutofillManager for the main frame.
  virtual autofill::AutofillManager* GetAutofillManagerForMainFrame();

  // Returns the main frame URL.
  virtual const GURL& GetMainFrameURL() const;

  // Returns true if the UI for confirmation of update password is enabled.
  virtual bool IsUpdatePasswordUIEnabled() const;

  virtual const GURL& GetLastCommittedEntryURL() const = 0;

  // Use this to filter credentials before handling them in password manager.
  virtual const CredentialsFilter* GetStoreResultFilter() const = 0;

  // Returns a LogManager instance.
  virtual const LogManager* GetLogManager() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordManagerClient);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_CLIENT_H_
