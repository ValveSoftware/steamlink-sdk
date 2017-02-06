// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

namespace password_manager {

class FormSaver;
class PasswordManager;
class PasswordManagerClient;

// Per-password-form-{on-page, dialog} class responsible for interactions
// between a given form, the per-tab PasswordManager, and the PasswordStore.
class PasswordFormManager : public PasswordStoreConsumer {
 public:
  // |password_manager| owns this object
  // |form_on_page| is the form that may be submitted and could need login data.
  // |ssl_valid| represents the security of the page containing observed_form,
  //           used to filter login results from database.
  PasswordFormManager(PasswordManager* password_manager,
                      PasswordManagerClient* client,
                      const base::WeakPtr<PasswordManagerDriver>& driver,
                      const autofill::PasswordForm& observed_form,
                      bool ssl_valid,
                      std::unique_ptr<FormSaver> form_saver);
  ~PasswordFormManager() override;

  // Flags describing the result of comparing two forms as performed by
  // DoesMatch. Individual flags are only relevant for HTML forms, but
  // RESULT_COMPLETE_MATCH will also be returned to indicate non-HTML forms
  // completely matching.
  // The ordering of these flags is important. Larger matches are more
  // preferred than lower matches. That is, since RESULT_HTML_ATTRIBUTES_MATCH
  // is greater than RESULT_ACTION_MATCH, a match of only attributes and not
  // actions will be preferred to one of actions and not attributes.
  enum MatchResultFlags {
    RESULT_NO_MATCH = 0,
    RESULT_ACTION_MATCH = 1 << 0,
    RESULT_HTML_ATTRIBUTES_MATCH = 1 << 1,
    RESULT_ORIGINS_MATCH = 1 << 2,
    RESULT_COMPLETE_MATCH = RESULT_ACTION_MATCH | RESULT_HTML_ATTRIBUTES_MATCH |
                            RESULT_ORIGINS_MATCH
  };
  // Use MatchResultMask to contain combinations of MatchResultFlags values.
  // It's a signed int rather than unsigned to avoid signed/unsigned mismatch
  // caused by the enum values implicitly converting to signed int.
  typedef int MatchResultMask;

  enum OtherPossibleUsernamesAction {
    ALLOW_OTHER_POSSIBLE_USERNAMES,
    IGNORE_OTHER_POSSIBLE_USERNAMES
  };

  // Chooses between the current and new password value which one to save. This
  // is whichever is non-empty, with the preference being given to the new one.
  static base::string16 PasswordToSave(const autofill::PasswordForm& form);

  // Compares basic data of |observed_form_| with |form| and returns how much
  // they match. The return value is a MatchResultMask bitmask.
  MatchResultMask DoesManage(const autofill::PasswordForm& form) const;

  // Retrieves potential matching logins from the database. In addition the
  // statistics is retrived on platforms with the password bubble.
  void FetchDataFromPasswordStore();

  // Simple state-check to verify whether this object as received a callback
  // from the PasswordStore and completed its matching phase. Note that the
  // callback in question occurs on the same (and only) main thread from which
  // instances of this class are ever used, but it is required since it is
  // conceivable that a user (or ui test) could attempt to submit a login
  // prompt before the callback has occured, which would InvokeLater a call to
  // PasswordManager::ProvisionallySave, which would interact with this object
  // before the db has had time to answer with matching password entries.
  // This is intended to be a one-time check; if the return value is false the
  // expectation is caller will give up. This clearly won't work if you put it
  // in a loop and wait for matching to complete; you're (supposed to be) on
  // the same thread!
  bool HasCompletedMatching() const;

  // Update |this| with the |form| that was actually submitted. Used to
  // determine what type the submitted form is for
  // IsIgnorableChangePasswordForm() and UMA stats.
  void SetSubmittedForm(const autofill::PasswordForm& form);

  // Determines if the user opted to 'never remember' passwords for this form.
  bool IsBlacklisted() const;

  // Used by PasswordManager to determine whether or not to display
  // a SavePasswordBar when given the green light to save the PasswordForm
  // managed by this.
  bool IsNewLogin() const;

  // Returns true if the current pending credentials were found using
  // origin matching of the public suffix, instead of the signon realm of the
  // form.
  bool IsPendingCredentialsPublicSuffixMatch() const;

  // Through |driver|, supply the associated frame with appropriate information
  // (fill data, whether to allow password generation, etc.). If this is called
  // before |this| has data from the PasswordStore, the execution will be
  // delayed until the data arrives.
  void ProcessFrame(const base::WeakPtr<PasswordManagerDriver>& driver);

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      ScopedVector<autofill::PasswordForm> results) override;
  void OnGetSiteStatistics(
      std::unique_ptr<std::vector<std::unique_ptr<InteractionsStats>>> stats)
      override;

  // A user opted to 'never remember' passwords for this form.
  // Blacklist it so that from now on when it is seen we ignore it.
  // TODO(vasilii): remove the 'virtual' specifier.
  virtual void PermanentlyBlacklist();

  // If the user has submitted observed_form_, provisionally hold on to
  // the submitted credentials until we are told by PasswordManager whether
  // or not the login was successful. |action| describes how we deal with
  // possible usernames. If |action| is ALLOW_OTHER_POSSIBLE_USERNAMES we will
  // treat a possible usernames match as a sign that our original heuristics
  // were wrong and that the user selected the correct username from the
  // Autofill UI.
  void ProvisionallySave(const autofill::PasswordForm& credentials,
                         OtherPossibleUsernamesAction action);

  // Handles save-as-new or update of the form managed by this manager.
  // Note the basic data of updated_credentials must match that of
  // observed_form_ (e.g DoesManage(pending_credentials_) == true).
  void Save();

  // Update the password store entry for |credentials_to_update|, using the
  // password from |pending_credentials_|. It modifies |pending_credentials_|.
  // |credentials_to_update| should be one of |best_matches_| or
  // |pending_credentials_|.
  void Update(const autofill::PasswordForm& credentials_to_update);

  // Call these if/when we know the form submission worked or failed.
  // These routines are used to update internal statistics ("ActionsTaken").
  void LogSubmitPassed();
  void LogSubmitFailed();

  // When attempting to provisionally save |form|, call this to check if it is
  // actually a change-password form which should be ignored, i.e., whether:
  // * the username was not explicitly marked with "autocomplete=username", and
  // * both the current and new password fields are non-empty, and
  // * the username and password do not match any credentials already stored.
  // In these cases the username field is detection is unreliable (there might
  // even be none), and the user should not be bothered with saving a
  // potentially malformed credential. Once we handle change password forms
  // correctly, this method should be replaced accordingly.
  //
  // Must be called after SetSubmittedForm().
  bool is_ignorable_change_password_form() const {
    DCHECK(form_type_ != kFormTypeUnspecified);
    return is_ignorable_change_password_form_;
  }

  // These functions are used to determine if this form has had it's password
  // auto generated by the browser.
  bool has_generated_password() const { return has_generated_password_; }
  void set_has_generated_password(bool generated_password) {
    has_generated_password_ = generated_password;
  }

  bool is_manual_generation() { return is_manual_generation_; }
  void set_is_manual_generation(bool is_manual_generation) {
    is_manual_generation_ = is_manual_generation;
  }

  const base::string16& generation_element() { return generation_element_; }
  void set_generation_element(const base::string16& generation_element) {
    generation_element_ = generation_element;
  }

  bool get_generation_popup_was_shown() const {
    return generation_popup_was_shown_;
  }
  void set_generation_popup_was_shown(bool generation_popup_was_shown) {
    generation_popup_was_shown_ = generation_popup_was_shown;
  }

  bool password_overridden() const { return password_overridden_; }

  bool retry_password_form_password_update() const {
    return retry_password_form_password_update_;
  }

  // Called if the user could generate a password for this form.
  void MarkGenerationAvailable() { generation_available_ = true; }

  // Returns the pending credentials.
  const autofill::PasswordForm& pending_credentials() const {
    return pending_credentials_;
  }

  // Returns the best matches.
  const autofill::PasswordFormMap& best_matches() const {
    return best_matches_;
  }

  const autofill::PasswordForm* preferred_match() const {
    return preferred_match_;
  }

  const std::vector<std::unique_ptr<autofill::PasswordForm>>&
  blacklisted_matches() const {
    return blacklisted_matches_;
  }

#if defined(UNIT_TEST)
  void SimulateFetchMatchingLoginsFromPasswordStore() {
    // Just need to update the internal states.
    state_ = MATCHING_PHASE;
  }
#endif

  const std::vector<std::unique_ptr<InteractionsStats>>& interactions_stats()
      const {
    return interactions_stats_;
  }

  const autofill::PasswordForm& observed_form() const { return observed_form_; }

  bool is_possible_change_password_form_without_username() const {
    return is_possible_change_password_form_without_username_;
  }

  const std::vector<std::unique_ptr<autofill::PasswordForm>>&
  federated_matches() {
    return federated_matches_;
  }

  // Use this to wipe copies of |pending_credentials_| from the password store
  // (and |best_matches_| as well. It will only wipe if:
  // 1. The stored password differs from the one in |pending_credentials_|.
  // 2. And the store already returned results for the observed form.
  // This is designed for use with sync credentials, so it will use GAIA utils
  // to catch equivalent usernames (e.g., if |pending_credentials_| have
  // username 'test', and the store also contains outdated entries for
  // 'test@gmail.com' and 'test@googlemail.com', those will be wiped).
  void WipeStoreCopyIfOutdated();

  // Called when the user chose not to update password.
  void OnNopeUpdateClicked();

  // Called when the user didn't interact with Update UI.
  void OnNoInteractionOnUpdate();

  // Saves the outcome of HTML parsing based form classifier to upload proto.
  void SaveGenerationFieldDetectedByClassifier(
      const base::string16& generation_field);

  FormSaver* form_saver() { return form_saver_.get(); }

 private:
  // ManagerAction - What does the manager do with this form? Either it
  // fills it, or it doesn't. If it doesn't fill it, that's either
  // because it has no match, or it is blacklisted, or it is disabled
  // via the AUTOCOMPLETE=off attribute. Note that if we don't have
  // an exact match, we still provide candidates that the user may
  // end up choosing.
  enum ManagerAction {
    kManagerActionNone = 0,
    kManagerActionAutofilled,
    kManagerActionBlacklisted,
    kManagerActionMax
  };

  // UserAction - What does the user do with this form? If they do nothing
  // (either by accepting what the password manager did, or by simply (not
  // typing anything at all), you get None. If there were multiple choices and
  // the user selects one other than the default, you get Choose, if user
  // selects an entry from matching against the Public Suffix List you get
  // ChoosePslMatch, if the user types in a new value for just the password you
  // get OverridePassword, and if the user types in a new value for the
  // username and password you get OverrideUsernameAndPassword.
  enum UserAction {
    kUserActionNone = 0,
    kUserActionChoose,
    kUserActionChoosePslMatch,
    kUserActionOverridePassword,
    kUserActionOverrideUsernameAndPassword,
    kUserActionMax
  };

  // Result - What happens to the form?
  enum SubmitResult {
    kSubmitResultNotSubmitted = 0,
    kSubmitResultFailed,
    kSubmitResultPassed,
    kSubmitResultMax
  };

  // What the form is used for. kFormTypeUnspecified is only set before
  // the SetSubmittedForm() is called, and should never be actually uploaded.
  enum FormType {
    kFormTypeLogin,
    kFormTypeLoginNoUsername,
    kFormTypeChangePasswordEnabled,
    kFormTypeChangePasswordDisabled,
    kFormTypeChangePasswordNoUsername,
    kFormTypeSignup,
    kFormTypeSignupNoUsername,
    kFormTypeLoginAndSignup,
    kFormTypeUnspecified,
    kFormTypeMax
  };

  // The outcome of the form classifier.
  enum FormClassifierOutcome {
    kNoOutcome,
    kNoGenerationElement,
    kFoundGenerationElement
  };

  // The maximum number of combinations of the three preceding enums.
  // This is used when recording the actions taken by the form in UMA.
  static const int kMaxNumActionsTaken =
      kManagerActionMax * kUserActionMax * kSubmitResultMax;

  // Through |driver|, supply the associated frame with appropriate information
  // (fill data, whether to allow password generation, etc.).
  void ProcessFrameInternal(const base::WeakPtr<PasswordManagerDriver>& driver);

  // Trigger filling of HTTP auth dialog and update |manager_action_|.
  void ProcessLoginPrompt();

  // Determines if we need to autofill given the results of the query.
  // Takes ownership of the elements in |result|.
  void OnRequestDone(ScopedVector<autofill::PasswordForm> result);

  // Helper for Save in the case that best_matches.size() == 0, meaning
  // we have no prior record of this form/username/password and the user
  // has opted to 'Save Password'. The previously preferred login from
  // |best_matches_| will be reset.
  void SaveAsNewLogin();

  // Helper for OnGetPasswordStoreResults to score an individual result
  // against the observed_form_.
  uint32_t ScoreResult(const autofill::PasswordForm& candidate) const;

  // For the blacklisted |form| returns true iff it blacklists |observed_form_|.
  bool IsBlacklistMatch(const autofill::PasswordForm& form) const;

  // Helper for Save in the case there is at least one match for the pending
  // credentials. This sends needed signals to the autofill server, and also
  // triggers some UMA reporting.
  void ProcessUpdate();

  // Check to see if |pending| corresponds to an account creation form. If we
  // think that it does, we label it as such and upload this state to the
  // Autofill server to vote for the correct username field, and also so that
  // we will trigger password generation in the future. This function will
  // update generation_upload_status of |pending| if an upload is performed.
  void SendAutofillVotes(const autofill::PasswordForm& observed,
                         autofill::PasswordForm* pending);

  // Update all login matches to reflect new preferred state - preferred flag
  // will be reset on all matched logins that different than the current
  // |pending_credentials_|.
  void UpdatePreferredLoginState(PasswordStore* password_store);

  // Returns true if |username| is one of the other possible usernames for a
  // password form in |best_matches_| and sets |pending_credentials_| to the
  // match which had this username.
  bool UpdatePendingCredentialsIfOtherPossibleUsername(
      const base::string16& username);

  // Returns true if |form| is a username update of a credential already in
  // |best_matches_|. Sets |pending_credentials_| to the appropriate
  // PasswordForm if it returns true.
  bool UpdatePendingCredentialsIfUsernameChanged(
      const autofill::PasswordForm& form);

  // Converts the "ActionsTaken" fields into an int so they can be logged to
  // UMA.
  int GetActionsTaken() const;

  // Try to label password fields and upload |form_data|. This differs from
  // AutofillManager::OnFormSubmitted() in a few ways.
  //   - This function will only label the first <input type="password"> field
  //     as |password_type|. Other fields will stay unlabeled, as they
  //     should have been labeled during the upload for OnFormSubmitted().
  //   - If the |username_field| attribute is nonempty, we will additionally
  //     label the field with that name as the username field.
  //   - This function does not assume that |form| is being uploaded during
  //     the same browsing session as it was originally submitted (as we may
  //     not have the necessary information to classify the form at that time)
  //     so it bypasses the cache and doesn't log the same quality UMA metrics.
  // |login_form_signature| may be empty.  It is non-empty when the user fills
  // and submits a login form using a generated password. In this case,
  // |login_form_signature| should be set to the submitted form's signature.
  // Note that in this case, |form.FormSignature()| gives the signature for the
  // registration form on which the password was generated, rather than the
  // submitted form's signature.
  bool UploadPasswordForm(const autofill::FormData& form_data,
                          const base::string16& username_field,
                          const autofill::ServerFieldType& password_type,
                          const std::string& login_form_signature);

  // Try to label username, password and new password fields of |observed_form_|
  // which is considered to be change password forms. Returns true on success.
  // |password_type| should be equal to NEW_PASSWORD, PROBABLY_NEW_PASSWORD or
  // NOT_NEW_PASSWORD. These values correspond to cases when the user conrirmed
  // password update, did nothing or declined to update password respectively.
  bool UploadChangePasswordForm(const autofill::ServerFieldType& password_type,
                                const std::string& login_form_signature);

  // Adds a vote on password generation usage to |form_structure|.
  void AddGeneratedVote(autofill::FormStructure* form_structure);

  // Adds a vote from HTML parsing based form classifier to |form_structure|.
  void AddFormClassifierVote(autofill::FormStructure* form_structure);

  // Create pending credentials from provisionally saved form and forms received
  // from password store.
  void CreatePendingCredentials();

  // Create pending credentials from provisionally saved form when this form
  // represents credentials that were not previosly saved.
  void CreatePendingCredentialsForNewCredentials();

  // If |best_matches| contains only one entry then return this entry. Otherwise
  // for empty |password| return nullptr and for non-empty |password| returns
  // the unique entry in |best_matches_| with the same password, if it exists,
  // and nullptr otherwise.
  autofill::PasswordForm* FindBestMatchForUpdatePassword(
      const base::string16& password) const;

  // Try to find best matched to |form| from |best_matches_| by the rules:
  // 1. If there is an element in |best_matches_| with the same username then
  // return it;
  // 2. If |form| is created with Credential API return nullptr, i.e. we match
  // Credentials API forms only by username;
  // 3. If |form| has no |username_element| and no |new_password_element| (i.e.
  // a form contains only one field which is a password) and there is an element
  // from |best_matches_| with the same password as in |form| then return it;
  // 4. Otherwise return nullptr.
  autofill::PasswordForm* FindBestSavedMatch(
      const autofill::PasswordForm* form) const;

  // Send appropriate votes based on what is currently being saved.
  void SendVotesOnSave();

  // Edits some fields in |pending_credentials_| before it can be used to
  // update the password store. It also goes through |not_best_matches|,
  // updates the password of those which share the old password and username
  // with |pending_credentials_| to the new password of |pending_credentials_|,
  // and adds pointers to all such modified credentials to
  // |credentials_to_update|. If needed, this also returns a PasswordForm to be
  // used as the old primary key during the store update.
  base::Optional<autofill::PasswordForm> UpdatePendingAndGetOldKey(
      std::vector<const autofill::PasswordForm*>* credentials_to_update);

  // Set of nonblacklisted PasswordForms from the DB that best match the form
  // being managed by this. Use a map instead of vector, because we most
  // frequently require lookups by username value in IsNewLogin.
  autofill::PasswordFormMap best_matches_;

  // Set of forms from PasswordStore that correspond to the current site and
  // that are not in |best_matches_|.
  std::vector<std::unique_ptr<autofill::PasswordForm>> not_best_matches_;

  // Federated credentials relevant to the observed form. They are neither
  // filled not saved by this PasswordFormManager, so they are kept separately
  // from |best_matches_|. The PasswordFormManager passes them further to
  // PasswordManager to show them in the UI.
  std::vector<std::unique_ptr<autofill::PasswordForm>> federated_matches_;

  // Set of blacklisted forms from the PasswordStore that best match the current
  // form.
  std::vector<std::unique_ptr<autofill::PasswordForm>> blacklisted_matches_;

  // The PasswordForm from the page or dialog managed by |this|.
  const autofill::PasswordForm observed_form_;

  // Statistics for the current domain.
  std::vector<std::unique_ptr<InteractionsStats>> interactions_stats_;

  // Stores a submitted form.
  std::unique_ptr<const autofill::PasswordForm> provisionally_saved_form_;

  // Stores if for creating |pending_credentials_| other possible usernames
  // option should apply.
  OtherPossibleUsernamesAction other_possible_username_action_;

  // The origin url path of observed_form_ tokenized, for convenience when
  // scoring.
  const std::vector<std::string> form_path_segments_;

  // Stores updated credentials when the form was submitted but success is still
  // unknown. This variable contains credentials that are ready to be written
  // (saved or updated) to a password store. It is calculated based on
  // |provisionally_saved_form_| and |best_matches_|.
  autofill::PasswordForm pending_credentials_;

  // Stores the form with generated password till the user makes successful
  // login or removes the generated password.
  std::unique_ptr<autofill::PasswordForm> presaved_form_;

  // Whether pending_credentials_ stores a new login or is an update
  // to an existing one.
  bool is_new_login_;

  // Whether this form has an auto generated password.
  bool has_generated_password_;

  // Whether password generation was manually triggered.
  bool is_manual_generation_;

  // A password field name that is used for generation.
  base::string16 generation_element_;

  // Whether generation popup was shown at least once.
  bool generation_popup_was_shown_;

  // The outcome of HTML parsing based form classifier.
  FormClassifierOutcome form_classifier_outcome_;

  // If |form_classifier_outcome_| == kFoundGenerationElement, the field
  // contains the name of the detected generation element.
  base::string16 generation_element_detected_by_classifier_;

  // Whether the saved password was overridden.
  bool password_overridden_;

  // A form is considered to be "retry" password if it has only one field which
  // is a current password field.
  // This variable is true if the password passed through ProvisionallySave() is
  // a password that is not part of any password form stored for this origin
  // and it was entered on a retry password form.
  bool retry_password_form_password_update_;

  // Whether the user can choose to generate a password for this form.
  bool generation_available_;

  // Set if the user has selected one of the other possible usernames in
  // |pending_credentials_|.
  base::string16 selected_username_;

  // PasswordManager owning this.
  PasswordManager* const password_manager_;

  // Convenience pointer to entry in best_matches_ that is marked
  // as preferred. This is only allowed to be null if there are no best matches
  // at all, since there will always be one preferred login when there are
  // multiple matches (when first saved, a login is marked preferred).
  const autofill::PasswordForm* preferred_match_;

  // If the submitted form is for a change password form and the username
  // doesn't match saved credentials.
  bool is_ignorable_change_password_form_;

  // True if we consider this form to be a change password form without username
  // field. We use only client heuristics, so it could include signup forms.
  // The value of this variable is calculated based not only on information from
  // |observed_form_| but also on the credentials that the user submitted.
  bool is_possible_change_password_form_without_username_;

  // True if |provisionally_saved_form_| looks like SignUp form according to
  // local heuristics.
  bool does_look_like_signup_form_ = false;

  typedef enum {
    PRE_MATCHING_PHASE,  // Have not yet invoked a GetLogins query to find
                         // matching login information from password store.
    MATCHING_PHASE,      // We've made a GetLogins request, but
                         // haven't received or finished processing result.
    POST_MATCHING_PHASE  // We've queried the DB and processed matching
                         // login results.
  } PasswordFormManagerState;

  // State of matching process, used to verify that we don't call methods
  // assuming we've already processed the request for matching logins,
  // when we actually haven't.
  PasswordFormManagerState state_;

  // The client which implements embedder-specific PasswordManager operations.
  PasswordManagerClient* client_;

  // |this| is created for a form in some frame, which is represented by a
  // driver. Similar form can appear in more frames, represented with more
  // drivers. The drivers are needed to perform frame-specific operations
  // (filling etc.). These drivers are kept in |drivers_| to allow updating of
  // the filling information when needed.
  std::vector<base::WeakPtr<PasswordManagerDriver>> drivers_;

  // These three fields record the "ActionsTaken" by the browser and
  // the user with this form, and the result. They are combined and
  // recorded in UMA when the manager is destroyed.
  ManagerAction manager_action_;
  UserAction user_action_;
  SubmitResult submit_result_;

  // Form type of the form that |this| is managing. Set after SetSubmittedForm()
  // as our classification of the form can change depending on what data the
  // user has entered.
  FormType form_type_;

  // False unless FetchMatchingLoginsFromPasswordStore has been called again
  // without the password store returning results in the meantime.
  bool need_to_refetch_;

  // FormSaver instance used by |this| to all tasks related to storing
  // credentials.
  std::unique_ptr<FormSaver> form_saver_;

  DISALLOW_COPY_AND_ASSIGN(PasswordFormManager);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_MANAGER_H_
