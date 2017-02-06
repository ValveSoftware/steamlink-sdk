// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_manager.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/form_saver.h"
#include "components/password_manager/core/browser/log_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/common/password_manager_features.h"

using autofill::FormStructure;
using autofill::PasswordForm;
using autofill::PasswordFormMap;
using base::Time;

// Shorten the name to spare line breaks. The code provides enough context
// already.
typedef autofill::SavePasswordProgressLogger Logger;

namespace password_manager {

namespace {

PasswordForm CopyAndModifySSLValidity(const PasswordForm& orig,
                                      bool ssl_valid) {
  PasswordForm result(orig);
  result.ssl_valid = ssl_valid;
  return result;
}

// Returns true if user-typed username and password field values match with one
// of the password form within |credentials| map; otherwise false.
bool DoesUsenameAndPasswordMatchCredentials(
    const base::string16& typed_username,
    const base::string16& typed_password,
    const autofill::PasswordFormMap& credentials) {
  for (const auto& match : credentials) {
    if (match.second->username_value == typed_username &&
        match.second->password_value == typed_password)
      return true;
  }
  return false;
}

std::vector<std::string> SplitPathToSegments(const std::string& path) {
  return base::SplitString(path, "/", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_ALL);
}

// Return false iff the strings are neither empty nor equal.
bool AreStringsEqualOrEmpty(const base::string16& s1,
                            const base::string16& s2) {
  return s1.empty() || s2.empty() || s1 == s2;
}

bool DoesStringContainOnlyDigits(const base::string16& s) {
  for (auto c : s) {
    if (!base::IsAsciiDigit(c))
      return false;
  }
  return true;
}

// Heuristics to determine that a string is very unlikely to be a username.
bool IsProbablyNotUsername(const base::string16& s) {
  return !s.empty() && DoesStringContainOnlyDigits(s) && s.size() < 3;
}

// Splits federated matches from |store_results| into a separate vector and
// returns that.
std::vector<std::unique_ptr<autofill::PasswordForm>> SplitFederatedMatches(
    ScopedVector<PasswordForm>* store_results) {
  auto first_federated =
      std::partition(store_results->begin(), store_results->end(),
                     [](const PasswordForm* form) {
                       return form->federation_origin.unique();
                     });

  std::vector<std::unique_ptr<autofill::PasswordForm>> federated_matches;
  federated_matches.reserve(store_results->end() - first_federated);
  for (auto federated = first_federated; federated != store_results->end();
       ++federated) {
    federated_matches.push_back(base::WrapUnique(*federated));
    *federated = nullptr;
  }
  store_results->weak_erase(first_federated, store_results->end());
  return federated_matches;
}

bool ShouldShowInitialPasswordAccountSuggestions() {
  return base::FeatureList::IsEnabled(
      password_manager::features::kFillOnAccountSelect);
}

// Update |credential| to reflect usage.
void UpdateMetadataForUsage(PasswordForm* credential) {
  ++credential->times_used;

  // Remove alternate usernames. At this point we assume that we have found
  // the right username.
  credential->other_possible_usernames.clear();
}

// Returns true iff |best_matches| contain a preferred credential with a
// username other than |preferred_username|.
bool DidPreferenceChange(const autofill::PasswordFormMap& best_matches,
                         const base::string16& preferred_username) {
  for (const auto& key_value_pair : best_matches) {
    const auto& form = key_value_pair.second;
    if (form->preferred && !form->is_public_suffix_match &&
        form->username_value != preferred_username) {
      return true;
    }
  }
  return false;
}

// Filter sensitive information, duplicates and |username_value| out from
// |form->other_possible_usernames|.
void SanitizePossibleUsernames(PasswordForm* form) {
  auto& usernames = form->other_possible_usernames;

  // Deduplicate.
  std::sort(usernames.begin(), usernames.end());
  auto new_end = std::unique(usernames.begin(), usernames.end());

  // Filter out |form->username_value|.
  new_end = std::remove(usernames.begin(), new_end, form->username_value);

  // Filter out sensitive information.
  new_end = std::remove_if(usernames.begin(), new_end,
                           autofill::IsValidCreditCardNumber);
  new_end = std::remove_if(usernames.begin(), new_end, autofill::IsSSN);
  usernames.erase(new_end, usernames.end());
}

}  // namespace

PasswordFormManager::PasswordFormManager(
    PasswordManager* password_manager,
    PasswordManagerClient* client,
    const base::WeakPtr<PasswordManagerDriver>& driver,
    const PasswordForm& observed_form,
    bool ssl_valid,
    std::unique_ptr<FormSaver> form_saver)
    : observed_form_(CopyAndModifySSLValidity(observed_form, ssl_valid)),
      provisionally_saved_form_(nullptr),
      other_possible_username_action_(
          PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES),
      form_path_segments_(
          observed_form_.origin.is_valid()
              ? SplitPathToSegments(observed_form_.origin.path())
              : std::vector<std::string>()),
      is_new_login_(true),
      has_generated_password_(false),
      is_manual_generation_(false),
      generation_popup_was_shown_(false),
      form_classifier_outcome_(kNoOutcome),
      password_overridden_(false),
      retry_password_form_password_update_(false),
      generation_available_(false),
      password_manager_(password_manager),
      preferred_match_(nullptr),
      is_ignorable_change_password_form_(false),
      is_possible_change_password_form_without_username_(
          observed_form.IsPossibleChangePasswordFormWithoutUsername()),
      state_(PRE_MATCHING_PHASE),
      client_(client),
      manager_action_(kManagerActionNone),
      user_action_(kUserActionNone),
      submit_result_(kSubmitResultNotSubmitted),
      form_type_(kFormTypeUnspecified),
      need_to_refetch_(false),
      form_saver_(std::move(form_saver)) {
  DCHECK_EQ(observed_form.scheme == PasswordForm::SCHEME_HTML,
            driver != nullptr);
  if (driver)
    drivers_.push_back(driver);
}

PasswordFormManager::~PasswordFormManager() {
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.ActionsTakenV3", GetActionsTaken(), kMaxNumActionsTaken);
  if (submit_result_ == kSubmitResultNotSubmitted) {
    if (has_generated_password_)
      metrics_util::LogPasswordGenerationSubmissionEvent(
          metrics_util::PASSWORD_NOT_SUBMITTED);
    else if (generation_available_)
      metrics_util::LogPasswordGenerationAvailableSubmissionEvent(
          metrics_util::PASSWORD_NOT_SUBMITTED);
  }
  if (form_type_ != kFormTypeUnspecified) {
    UMA_HISTOGRAM_ENUMERATION("PasswordManager.SubmittedFormType", form_type_,
                              kFormTypeMax);
  }
}

int PasswordFormManager::GetActionsTaken() const {
  return user_action_ +
         kUserActionMax *
             (manager_action_ + kManagerActionMax * submit_result_);
}

// static
base::string16 PasswordFormManager::PasswordToSave(const PasswordForm& form) {
  if (form.new_password_element.empty() || form.new_password_value.empty())
    return form.password_value;
  return form.new_password_value;
}

// TODO(timsteele): use a hash of some sort in the future?
PasswordFormManager::MatchResultMask PasswordFormManager::DoesManage(
    const PasswordForm& form) const {
  // Non-HTML form case.
  if (observed_form_.scheme != PasswordForm::SCHEME_HTML ||
      form.scheme != PasswordForm::SCHEME_HTML) {
    const bool forms_match = observed_form_.signon_realm == form.signon_realm &&
                             observed_form_.scheme == form.scheme;
    return forms_match ? RESULT_COMPLETE_MATCH : RESULT_NO_MATCH;
  }

  // HTML form case.
  MatchResultMask result = RESULT_NO_MATCH;

  // Easiest case of matching origins.
  bool origins_match = form.origin == observed_form_.origin;
  // If this is a replay of the same form in the case a user entered an invalid
  // password, the origin of the new form may equal the action of the "first"
  // form instead.
  origins_match = origins_match || (form.origin == observed_form_.action);
  // Otherwise, if action hosts are the same, the old URL scheme is HTTP while
  // the new one is HTTPS, and the new path equals to or extends the old path,
  // we also consider the actions a match. This is to accommodate cases where
  // the original login form is on an HTTP page, but a failed login attempt
  // redirects to HTTPS (as in http://example.org -> https://example.org/auth).
  if (!origins_match && !observed_form_.origin.SchemeIsCryptographic() &&
      form.origin.SchemeIsCryptographic()) {
    const std::string& old_path = observed_form_.origin.path();
    const std::string& new_path = form.origin.path();
    origins_match =
        observed_form_.origin.host() == form.origin.host() &&
        observed_form_.origin.port() == form.origin.port() &&
        base::StartsWith(new_path, old_path, base::CompareCase::SENSITIVE);
  }

  if (!origins_match)
    return result;

  result |= RESULT_ORIGINS_MATCH;

  // Autofill predictions can overwrite our default username selection so
  // if this form was parsed with autofill predictions then allow the username
  // element to be different.
  if ((form.was_parsed_using_autofill_predictions ||
       form.username_element == observed_form_.username_element) &&
      form.password_element == observed_form_.password_element) {
    result |= RESULT_HTML_ATTRIBUTES_MATCH;
  }

  // Note: although saved password forms might actually have an empty action
  // URL if they were imported (see bug 1107719), the |form| we see here comes
  // never from the password store, and should have an exactly matching action.
  if (form.action == observed_form_.action)
    result |= RESULT_ACTION_MATCH;

  return result;
}

bool PasswordFormManager::IsBlacklisted() const {
  DCHECK_EQ(state_, POST_MATCHING_PHASE);
  return !blacklisted_matches_.empty();
}

void PasswordFormManager::PermanentlyBlacklist() {
  DCHECK_EQ(state_, POST_MATCHING_PHASE);
  DCHECK(!client_->IsOffTheRecord());

  blacklisted_matches_.push_back(
      base::WrapUnique(new autofill::PasswordForm(observed_form_)));
  form_saver_->PermanentlyBlacklist(blacklisted_matches_.back().get());
}

bool PasswordFormManager::IsNewLogin() const {
  DCHECK_EQ(state_, POST_MATCHING_PHASE);
  return is_new_login_;
}

bool PasswordFormManager::IsPendingCredentialsPublicSuffixMatch() const {
  return pending_credentials_.is_public_suffix_match;
}

void PasswordFormManager::ProvisionallySave(
    const PasswordForm& credentials,
    OtherPossibleUsernamesAction action) {
  DCHECK(state_ == MATCHING_PHASE || state_ == POST_MATCHING_PHASE) << state_;
  DCHECK_NE(RESULT_NO_MATCH, DoesManage(credentials));

  std::unique_ptr<autofill::PasswordForm> mutable_provisionally_saved_form(
      new PasswordForm(credentials));
  if (credentials.IsPossibleChangePasswordForm() &&
      !credentials.username_value.empty() &&
      IsProbablyNotUsername(credentials.username_value)) {
    mutable_provisionally_saved_form->username_value.clear();
    mutable_provisionally_saved_form->username_element.clear();
    is_possible_change_password_form_without_username_ = true;
  }
  provisionally_saved_form_ = std::move(mutable_provisionally_saved_form);
  other_possible_username_action_ = action;
  does_look_like_signup_form_ = credentials.does_look_like_signup_form;

  if (HasCompletedMatching())
    CreatePendingCredentials();
}

void PasswordFormManager::Save() {
  DCHECK_EQ(state_, POST_MATCHING_PHASE);
  DCHECK(!client_->IsOffTheRecord());

  if ((user_action_ == kUserActionNone) &&
      DidPreferenceChange(best_matches_, pending_credentials_.username_value)) {
    user_action_ = kUserActionChoose;
  }
  base::Optional<PasswordForm> old_primary_key;
  if (is_new_login_) {
    SanitizePossibleUsernames(&pending_credentials_);
    pending_credentials_.date_created = base::Time::Now();
    SendVotesOnSave();
    form_saver_->Save(pending_credentials_, best_matches_,
                      old_primary_key ? &old_primary_key.value() : nullptr);
  } else {
    ProcessUpdate();
    std::vector<const PasswordForm*> credentials_to_update;
    old_primary_key = UpdatePendingAndGetOldKey(&credentials_to_update);
    form_saver_->Update(pending_credentials_, best_matches_,
                        &credentials_to_update,
                        old_primary_key ? &old_primary_key.value() : nullptr);
  }

  // This is not in ProcessUpdate() to catch PSL matched credentials.
  if (pending_credentials_.times_used != 0 &&
      pending_credentials_.type == PasswordForm::TYPE_GENERATED) {
    metrics_util::LogPasswordGenerationSubmissionEvent(
        metrics_util::PASSWORD_USED);
  }

  password_manager_->UpdateFormManagers();
}

void PasswordFormManager::Update(
    const autofill::PasswordForm& credentials_to_update) {
  if (observed_form_.IsPossibleChangePasswordForm()) {
    FormStructure form_structure(credentials_to_update.form_data);
    UploadChangePasswordForm(autofill::NEW_PASSWORD,
                             form_structure.FormSignature());
  }
  base::string16 password_to_save = pending_credentials_.password_value;
  bool skip_zero_click = pending_credentials_.skip_zero_click;
  pending_credentials_ = credentials_to_update;
  pending_credentials_.password_value = password_to_save;
  pending_credentials_.skip_zero_click = skip_zero_click;
  pending_credentials_.preferred = true;
  is_new_login_ = false;
  ProcessUpdate();
  std::vector<const PasswordForm*> more_credentials_to_update;
  base::Optional<PasswordForm> old_primary_key =
      UpdatePendingAndGetOldKey(&more_credentials_to_update);
  form_saver_->Update(pending_credentials_, best_matches_,
                      &more_credentials_to_update,
                      old_primary_key ? &old_primary_key.value() : nullptr);
}

void PasswordFormManager::FetchDataFromPasswordStore() {
  if (state_ == MATCHING_PHASE) {
    // There is currently a password store query in progress, need to re-fetch
    // store results later.
    need_to_refetch_ = true;
    return;
  }

  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogMessage(Logger::STRING_FETCH_LOGINS_METHOD);
    logger->LogNumber(Logger::STRING_FORM_MANAGER_STATE, state_);
  }

  provisionally_saved_form_.reset();
  state_ = MATCHING_PHASE;

  PasswordStore* password_store = client_->GetPasswordStore();
  if (!password_store) {
    if (logger)
      logger->LogMessage(Logger::STRING_NO_STORE);
    NOTREACHED();
    return;
  }
  password_store->GetLogins(observed_form_, this);

// The statistics isn't needed on mobile, only on desktop. Let's save some
// processor cycles.
#if !defined(OS_IOS) && !defined(OS_ANDROID)
  // The statistics is needed for the "Save password?" bubble.
  password_store->GetSiteStats(observed_form_.origin.GetOrigin(), this);
#endif
}

bool PasswordFormManager::HasCompletedMatching() const {
  return state_ == POST_MATCHING_PHASE;
}

void PasswordFormManager::SetSubmittedForm(const autofill::PasswordForm& form) {
  bool is_change_password_form =
      !form.new_password_value.empty() && !form.password_value.empty();
  is_ignorable_change_password_form_ =
      is_change_password_form && !form.username_marked_by_site &&
      !DoesUsenameAndPasswordMatchCredentials(
          form.username_value, form.password_value, best_matches_) &&
      !client_->IsUpdatePasswordUIEnabled();
  bool is_signup_form =
      !form.new_password_value.empty() && form.password_value.empty();
  bool no_username = form.username_element.empty();

  if (form.layout == PasswordForm::Layout::LAYOUT_LOGIN_AND_SIGNUP) {
    form_type_ = kFormTypeLoginAndSignup;
  } else if (is_ignorable_change_password_form_) {
    if (no_username)
      form_type_ = kFormTypeChangePasswordNoUsername;
    else
      form_type_ = kFormTypeChangePasswordDisabled;
  } else if (is_change_password_form) {
    form_type_ = kFormTypeChangePasswordEnabled;
  } else if (is_signup_form) {
    if (no_username)
      form_type_ = kFormTypeSignupNoUsername;
    else
      form_type_ = kFormTypeSignup;
  } else if (no_username) {
    form_type_ = kFormTypeLoginNoUsername;
  } else {
    form_type_ = kFormTypeLogin;
  }
}

void PasswordFormManager::OnRequestDone(
    ScopedVector<PasswordForm> logins_result) {
  preferred_match_ = nullptr;
  best_matches_.clear();
  blacklisted_matches_.clear();
  const size_t logins_result_size = logins_result.size();

  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogMessage(Logger::STRING_ON_REQUEST_DONE_METHOD);
  }

  // Remove credentials which need to be ignored from |logins_result|.
  if (!observed_form_.ssl_valid) {
    logins_result.erase(
        std::partition(logins_result.begin(), logins_result.end(),
                       [](PasswordForm* form) { return !form->ssl_valid; }),
        logins_result.end());
  }
  logins_result =
      client_->GetStoreResultFilter()->FilterResults(std::move(logins_result));

  // Deal with blacklisted forms.
  auto begin_blacklisted = std::partition(
      logins_result.begin(), logins_result.end(),
      [](PasswordForm* form) { return !form->blacklisted_by_user; });
  for (auto it = begin_blacklisted; it != logins_result.end(); ++it) {
    if (IsBlacklistMatch(**it)) {
      blacklisted_matches_.push_back(base::WrapUnique(*it));
      *it = nullptr;
    }
  }
  logins_result.erase(begin_blacklisted, logins_result.end());
  if (logins_result.empty())
    return;

  // Now compute scores for the remaining credentials in |login_result|.
  std::vector<uint32_t> credential_scores;
  credential_scores.reserve(logins_result.size());
  uint32_t best_score = 0;
  std::map<base::string16, uint32_t> best_scores;
  for (const PasswordForm* login : logins_result) {
    uint32_t current_score = ScoreResult(*login);
    best_score = std::max(best_score, current_score);
    best_scores[login->username_value] =
        std::max(best_scores[login->username_value], current_score);
    credential_scores.push_back(current_score);
  }

  // Fill |best_matches_| with the best-scoring credentials for each username.
  for (size_t i = 0; i < logins_result.size(); ++i) {
    // Take ownership of the PasswordForm from the ScopedVector.
    std::unique_ptr<PasswordForm> login(logins_result[i]);
    logins_result[i] = nullptr;
    DCHECK(!login->blacklisted_by_user);
    const base::string16& username = login->username_value;

    if (credential_scores[i] < best_scores[username]) {
      not_best_matches_.push_back(std::move(login));
      continue;
    }

    if (!preferred_match_ && credential_scores[i] == best_score)
      preferred_match_ = login.get();

    // If there is another best-score match for the same username then leave it
    // and add the current form to |not_best_matches_|.
    auto best_match_username = best_matches_.find(username);
    if (best_match_username == best_matches_.end()) {
      best_matches_.insert(std::make_pair(username, std::move(login)));
    } else {
      not_best_matches_.push_back(std::move(login));
    }
  }

  UMA_HISTOGRAM_COUNTS("PasswordManager.NumPasswordsNotShown",
                       logins_result_size - best_matches_.size());
}

void PasswordFormManager::ProcessFrame(
    const base::WeakPtr<PasswordManagerDriver>& driver) {
  DCHECK_EQ(PasswordForm::SCHEME_HTML, observed_form_.scheme);
  if (state_ == POST_MATCHING_PHASE)
    ProcessFrameInternal(driver);

  for (auto const& old_driver : drivers_) {
    // |drivers_| is not a set because WeakPtr has no good candidate for a key
    // (the address may change to null). So let's weed out duplicates in O(N).
    if (old_driver.get() == driver.get())
      return;
  }

  drivers_.push_back(driver);
}

void PasswordFormManager::ProcessFrameInternal(
    const base::WeakPtr<PasswordManagerDriver>& driver) {
  DCHECK_EQ(PasswordForm::SCHEME_HTML, observed_form_.scheme);
  if (!driver || manager_action_ == kManagerActionBlacklisted)
    return;

  // Allow generation for any non-blacklisted form.
  driver->AllowPasswordGenerationForForm(observed_form_);

  if (best_matches_.empty())
    return;

  // Proceed to autofill.
  // Note that we provide the choices but don't actually prefill a value if:
  // (1) we are in Incognito mode, (2) the ACTION paths don't match,
  // (3) if it matched using public suffix domain matching, or
  // (4) the form is change password form.
  // However, 2 and 3 should not apply to Android-based credentials found
  // via affiliation-based matching (we want to autofill them).
  // TODO(engedy): Clean this up. See: https://crbug.com/476519.
  bool wait_for_username =
      client_->IsOffTheRecord() ||
      (!IsValidAndroidFacetURI(preferred_match_->signon_realm) &&
       (observed_form_.action.GetWithEmptyPath() !=
            preferred_match_->action.GetWithEmptyPath() ||
        preferred_match_->is_public_suffix_match ||
        observed_form_.IsPossibleChangePasswordForm()));
  if (wait_for_username)
    manager_action_ = kManagerActionNone;
  else
    manager_action_ = kManagerActionAutofilled;
  if (ShouldShowInitialPasswordAccountSuggestions()) {
    // This is for the fill-on-account-select experiment. Instead of autofilling
    // found usernames and passwords on load, this instructs the renderer to
    // return with any found password forms so a list of password account
    // suggestions can be drawn.
    password_manager_->ShowInitialPasswordAccountSuggestions(
        driver.get(), observed_form_, best_matches_, federated_matches_,
        *preferred_match_, wait_for_username);
  } else {
    // If fill-on-account-select is not enabled, continue with autofilling any
    // password forms as traditionally has been done.
    password_manager_->Autofill(driver.get(), observed_form_, best_matches_,
                                federated_matches_, *preferred_match_,
                                wait_for_username);
  }
}

void PasswordFormManager::ProcessLoginPrompt() {
  DCHECK_NE(PasswordForm::SCHEME_HTML, observed_form_.scheme);
  if (!preferred_match_)
    return;

  manager_action_ = kManagerActionAutofilled;
  password_manager_->AutofillHttpAuth(best_matches_, *preferred_match_);
}

void PasswordFormManager::OnGetPasswordStoreResults(
    ScopedVector<PasswordForm> results) {
  DCHECK_EQ(state_, MATCHING_PHASE);

  if (need_to_refetch_) {
    // The received results are no longer up to date, need to re-request.
    state_ = PRE_MATCHING_PHASE;
    FetchDataFromPasswordStore();
    need_to_refetch_ = false;
    return;
  }

  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogMessage(Logger::STRING_ON_GET_STORE_RESULTS_METHOD);
    logger->LogNumber(Logger::STRING_NUMBER_RESULTS, results.size());
  }

  federated_matches_ = SplitFederatedMatches(&results);
  if (!results.empty())
    OnRequestDone(std::move(results));
  state_ = POST_MATCHING_PHASE;

  // If password store was slow and provisionally saved form is already here
  // then create pending credentials (see http://crbug.com/470322).
  if (provisionally_saved_form_)
    CreatePendingCredentials();

  if (manager_action_ != kManagerActionBlacklisted) {
    for (auto const& driver : drivers_)
      ProcessFrameInternal(driver);
    if (observed_form_.scheme != PasswordForm::SCHEME_HTML)
      ProcessLoginPrompt();
  }
}

void PasswordFormManager::OnGetSiteStatistics(
    std::unique_ptr<std::vector<std::unique_ptr<InteractionsStats>>> stats) {
  // On Windows the password request may be resolved after the statistics due to
  // importing from IE.
  DCHECK(state_ == MATCHING_PHASE || state_ == POST_MATCHING_PHASE) << state_;
  interactions_stats_.swap(*stats);
}

void PasswordFormManager::ProcessUpdate() {
  DCHECK_EQ(state_, POST_MATCHING_PHASE);
  DCHECK(preferred_match_ || !pending_credentials_.federation_origin.unique());
  // If we're doing an Update, we either autofilled correctly and need to
  // update the stats, or the user typed in a new password for autofilled
  // username, or the user selected one of the non-preferred matches,
  // thus requiring a swap of preferred bits.
  DCHECK(!IsNewLogin() && pending_credentials_.preferred);
  DCHECK(!client_->IsOffTheRecord());

  UpdateMetadataForUsage(&pending_credentials_);

  client_->GetStoreResultFilter()->ReportFormUsed(pending_credentials_);

  // Check to see if this form is a candidate for password generation.
  // Do not send votes on change password forms, since they were already sent in
  // Update() method.
  if (!observed_form_.IsPossibleChangePasswordForm())
    SendAutofillVotes(observed_form_, &pending_credentials_);
}

bool PasswordFormManager::UpdatePendingCredentialsIfOtherPossibleUsername(
    const base::string16& username) {
  for (PasswordFormMap::const_iterator it = best_matches_.begin();
       it != best_matches_.end(); ++it) {
    for (size_t i = 0; i < it->second->other_possible_usernames.size(); ++i) {
      if (it->second->other_possible_usernames[i] == username) {
        pending_credentials_ = *it->second;
        return true;
      }
    }
  }
  return false;
}

void PasswordFormManager::SendAutofillVotes(
    const PasswordForm& observed,
    PasswordForm* pending) {
  if (pending->form_data.fields.empty())
    return;

  FormStructure pending_structure(pending->form_data);
  FormStructure observed_structure(observed.form_data);

  // Ignore |pending_structure| if its FormData has no fields. This is to
  // weed out those credentials that were saved before FormData was added
  // to PasswordForm. Even without this check, these FormStructure's won't
  // be uploaded, but it makes it hard to see if we are encountering
  // unexpected errors.
  if (pending_structure.FormSignature() != observed_structure.FormSignature()) {
    // Only upload if this is the first time the password has been used.
    // Otherwise the credentials have been used on the same field before so
    // they aren't from an account creation form.
    // Also bypass uploading if the username was edited. Offering generation
    // in cases where we currently save the wrong username isn't great.
    // TODO(gcasto): Determine if generation should be offered in this case.
    if (pending->times_used == 1 && selected_username_.empty()) {
      if (UploadPasswordForm(pending->form_data, pending->username_element,
                             autofill::ACCOUNT_CREATION_PASSWORD,
                             observed_structure.FormSignature())) {
        pending->generation_upload_status =
            autofill::PasswordForm::POSITIVE_SIGNAL_SENT;
      }
    }
  } else if (pending->generation_upload_status ==
             autofill::PasswordForm::POSITIVE_SIGNAL_SENT) {
    // A signal was sent that this was an account creation form, but the
    // credential is now being used on the same form again. This cancels out
    // the previous vote.
    if (UploadPasswordForm(pending->form_data, base::string16(),
                           autofill::NOT_ACCOUNT_CREATION_PASSWORD,
                           std::string())) {
      pending->generation_upload_status =
          autofill::PasswordForm::NEGATIVE_SIGNAL_SENT;
    }
  }
}

bool PasswordFormManager::UploadPasswordForm(
    const autofill::FormData& form_data,
    const base::string16& username_field,
    const autofill::ServerFieldType& password_type,
    const std::string& login_form_signature) {
  DCHECK(password_type == autofill::PASSWORD ||
         password_type == autofill::PROBABLY_ACCOUNT_CREATION_PASSWORD ||
         password_type == autofill::ACCOUNT_CREATION_PASSWORD ||
         password_type == autofill::NOT_ACCOUNT_CREATION_PASSWORD);
  autofill::AutofillManager* autofill_manager =
      client_->GetAutofillManagerForMainFrame();
  if (!autofill_manager || !autofill_manager->download_manager())
    return false;

  FormStructure form_structure(form_data);
  if (!autofill_manager->ShouldUploadForm(form_structure) ||
      !form_structure.ShouldBeCrowdsourced()) {
    UMA_HISTOGRAM_BOOLEAN("PasswordGeneration.UploadStarted", false);
    return false;
  }

  // Find the first password field to label. If the provided username field name
  // is not empty, then also find the first field with that name to label.
  // We don't try to label anything else.
  bool found_password_field = false;
  bool should_find_username_field = !username_field.empty();
  for (size_t i = 0; i < form_structure.field_count(); ++i) {
    autofill::AutofillField* field = form_structure.field(i);

    autofill::ServerFieldType type = autofill::UNKNOWN_TYPE;
    if (!found_password_field && field->form_control_type == "password") {
      type = password_type;
      found_password_field = true;
    } else if (should_find_username_field && field->name == username_field) {
      type = autofill::USERNAME;
      should_find_username_field = false;
    }

    autofill::ServerFieldTypeSet types;
    types.insert(type);
    field->set_possible_types(types);
  }
  DCHECK(found_password_field);
  DCHECK(!should_find_username_field);

  autofill::ServerFieldTypeSet available_field_types;
  available_field_types.insert(password_type);
  available_field_types.insert(autofill::USERNAME);

  if (generation_popup_was_shown_)
    AddGeneratedVote(&form_structure);
  if (form_classifier_outcome_ != kNoOutcome)
    AddFormClassifierVote(&form_structure);

  // Force uploading as these events are relatively rare and we want to make
  // sure to receive them.
  form_structure.set_upload_required(UPLOAD_REQUIRED);

  bool success = autofill_manager->download_manager()->StartUploadRequest(
      form_structure, false /* was_autofilled */, available_field_types,
      login_form_signature, true /* observed_submission */);

  UMA_HISTOGRAM_BOOLEAN("PasswordGeneration.UploadStarted", success);
  return success;
}

bool PasswordFormManager::UploadChangePasswordForm(
    const autofill::ServerFieldType& password_type,
    const std::string& login_form_signature) {
  DCHECK(password_type == autofill::NEW_PASSWORD ||
         password_type == autofill::PROBABLY_NEW_PASSWORD ||
         password_type == autofill::NOT_NEW_PASSWORD);
  if (!provisionally_saved_form_ ||
      provisionally_saved_form_->new_password_element.empty()) {
    // |new_password_element| is empty for non change password forms, for
    // example when the password was overriden.
    return false;
  }
  autofill::AutofillManager* autofill_manager =
      client_->GetAutofillManagerForMainFrame();
  if (!autofill_manager || !autofill_manager->download_manager())
    return false;

  // Create a map from field names to field types.
  std::map<base::string16, autofill::ServerFieldType> field_types;
  if (!pending_credentials_.username_element.empty())
    field_types[provisionally_saved_form_->username_element] =
        autofill::USERNAME;
  if (!pending_credentials_.password_element.empty())
    field_types[provisionally_saved_form_->password_element] =
        autofill::PASSWORD;
  field_types[provisionally_saved_form_->new_password_element] = password_type;
  // Find all password fields after |new_password_element| and set their type to
  // |password_type|. They are considered to be confirmation fields.
  const autofill::FormData& form_data = observed_form_.form_data;
  bool is_new_password_field_found = false;
  for (size_t i = 0; i < form_data.fields.size(); ++i) {
    const autofill::FormFieldData& field = form_data.fields[i];
    if (field.form_control_type != "password")
      continue;
    if (is_new_password_field_found) {
      field_types[field.name] = password_type;
      // We don't care about password fields after a confirmation field.
      break;
    } else if (field.name == provisionally_saved_form_->new_password_element) {
      is_new_password_field_found = true;
    }
  }
  DCHECK(is_new_password_field_found);

  // Create FormStructure with field type information for uploading a vote.
  FormStructure form_structure(form_data);
  if (!autofill_manager->ShouldUploadForm(form_structure) ||
      !form_structure.ShouldBeCrowdsourced())
    return false;

  autofill::ServerFieldTypeSet available_field_types;
  for (size_t i = 0; i < form_structure.field_count(); ++i) {
    autofill::AutofillField* field = form_structure.field(i);
    autofill::ServerFieldType type = autofill::UNKNOWN_TYPE;
    auto iter = field_types.find(field->name);
    if (iter != field_types.end()) {
      type = iter->second;
      available_field_types.insert(type);
    }

    autofill::ServerFieldTypeSet types;
    types.insert(type);
    field->set_possible_types(types);
  }

  if (generation_popup_was_shown_)
    AddGeneratedVote(&form_structure);
  if (form_classifier_outcome_ != kNoOutcome)
    AddFormClassifierVote(&form_structure);

  // Force uploading as these events are relatively rare and we want to make
  // sure to receive them. It also makes testing easier if these requests
  // always pass.
  form_structure.set_upload_required(UPLOAD_REQUIRED);

  return autofill_manager->download_manager()->StartUploadRequest(
      form_structure, false /* was_autofilled */, available_field_types,
      login_form_signature, true /* observed_submission */);
}

void PasswordFormManager::AddGeneratedVote(
    autofill::FormStructure* form_structure) {
  DCHECK(form_structure);
  DCHECK(generation_popup_was_shown_);

  if (generation_element_.empty())
    return;

  autofill::AutofillUploadContents::Field::PasswordGenerationType type =
      autofill::AutofillUploadContents::Field::NO_GENERATION;
  if (has_generated_password_) {
    if (is_manual_generation_) {
      type = observed_form_.IsPossibleChangePasswordForm()
                 ? autofill::AutofillUploadContents::Field::
                       MANUALLY_TRIGGERED_GENERATION_ON_CHANGE_PASSWORD_FORM
                 : autofill::AutofillUploadContents::Field::
                       MANUALLY_TRIGGERED_GENERATION_ON_SIGN_UP_FORM;
    } else {
      type =
          observed_form_.IsPossibleChangePasswordForm()
              ? autofill::AutofillUploadContents::Field::
                    AUTOMATICALLY_TRIGGERED_GENERATION_ON_CHANGE_PASSWORD_FORM
              : autofill::AutofillUploadContents::Field::
                    AUTOMATICALLY_TRIGGERED_GENERATION_ON_SIGN_UP_FORM;
    }
  } else
    type = autofill::AutofillUploadContents::Field::IGNORED_GENERATION_POPUP;

  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    autofill::AutofillField* field = form_structure->field(i);
    if (field->name == generation_element_) {
      field->set_generation_type(type);
      break;
    }
  }
}

void PasswordFormManager::AddFormClassifierVote(
    autofill::FormStructure* form_structure) {
  DCHECK(form_structure);
  DCHECK(form_classifier_outcome_ != kNoOutcome);

  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    autofill::AutofillField* field = form_structure->field(i);
    if (form_classifier_outcome_ == kFoundGenerationElement &&
        field->name == generation_element_detected_by_classifier_) {
      field->set_form_classifier_outcome(
          autofill::AutofillUploadContents::Field::GENERATION_ELEMENT);
    } else {
      field->set_form_classifier_outcome(
          autofill::AutofillUploadContents::Field::NON_GENERATION_ELEMENT);
    }
  }
}

void PasswordFormManager::CreatePendingCredentials() {
  DCHECK(provisionally_saved_form_);
  base::string16 password_to_save(PasswordToSave(*provisionally_saved_form_));

  // Make sure the important fields stay the same as the initially observed or
  // autofilled ones, as they may have changed if the user experienced a login
  // failure.
  // Look for these credentials in the list containing auto-fill entries.
  PasswordForm* saved_form =
      FindBestSavedMatch(provisionally_saved_form_.get());
  if (saved_form != nullptr) {
    // The user signed in with a login we autofilled.
    pending_credentials_ = *saved_form;
    password_overridden_ =
        pending_credentials_.password_value != password_to_save;
    if (IsPendingCredentialsPublicSuffixMatch()) {
      // If the autofilled credentials were a PSL match or credentials stored
      // from Android apps store a copy with the current origin and signon
      // realm. This ensures that on the next visit, a precise match is found.
      is_new_login_ = true;
      user_action_ = password_overridden_ ? kUserActionOverridePassword
                                          : kUserActionChoosePslMatch;

      // Since this credential will not overwrite a previously saved credential,
      // username_value can be updated now.
      if (!selected_username_.empty())
        pending_credentials_.username_value = selected_username_;

      // Update credential to reflect that it has been used for submission.
      // If this isn't updated, then password generation uploads are off for
      // sites where PSL matching is required to fill the login form, as two
      // PASSWORD votes are uploaded per saved password instead of one.
      //
      // TODO(gcasto): It would be nice if other state were shared such that if
      // say a password was updated on one match it would update on all related
      // passwords. This is a much larger change.
      UpdateMetadataForUsage(&pending_credentials_);

      // Update |pending_credentials_| in order to be able correctly save it.
      pending_credentials_.origin = provisionally_saved_form_->origin;
      pending_credentials_.signon_realm =
          provisionally_saved_form_->signon_realm;

      // Normally, the copy of the PSL matched credentials, adapted for the
      // current domain, is saved automatically without asking the user, because
      // the copy likely represents the same account, i.e., the one for which
      // the user already agreed to store a password.
      //
      // However, if the user changes the suggested password, it might indicate
      // that the autofilled credentials and |provisionally_saved_form_|
      // actually correspond  to two different accounts (see
      // http://crbug.com/385619). In that case the user should be asked again
      // before saving the password. This is ensured by setting
      // |password_overriden_| on |pending_credentials_| to false and setting
      // |origin| and |signon_realm| to correct values.
      //
      // There is still the edge case when the autofilled credentials represent
      // the same account as |provisionally_saved_form_| but the stored password
      // was out of date. In that case, the user just had to manually enter the
      // new password, which is now in |provisionally_saved_form_|. The best
      // thing would be to save automatically, and also update the original
      // credentials. However, we have no way to tell if this is the case.
      // This will likely happen infrequently, and the inconvenience put on the
      // user by asking them is not significant, so we are fine with asking
      // here again.
      if (password_overridden_) {
        pending_credentials_.is_public_suffix_match = false;
        password_overridden_ = false;
      }
    } else {  // Not a PSL match.
      is_new_login_ = false;
      if (password_overridden_)
        user_action_ = kUserActionOverridePassword;
    }
  } else if (other_possible_username_action_ ==
                 ALLOW_OTHER_POSSIBLE_USERNAMES &&
             UpdatePendingCredentialsIfOtherPossibleUsername(
                 provisionally_saved_form_->username_value)) {
    // |pending_credentials_| is now set. Note we don't update
    // |pending_credentials_.username_value| to |credentials.username_value|
    // yet because we need to keep the original username to modify the stored
    // credential.
    selected_username_ = provisionally_saved_form_->username_value;
    is_new_login_ = false;
  } else if (client_->IsUpdatePasswordUIEnabled() && !best_matches_.empty() &&
             provisionally_saved_form_->type !=
                 autofill::PasswordForm::TYPE_API &&
             (provisionally_saved_form_
                  ->IsPossibleChangePasswordFormWithoutUsername() ||
              provisionally_saved_form_->username_element.empty())) {
    PasswordForm* best_update_match = FindBestMatchForUpdatePassword(
        provisionally_saved_form_->password_value);

    retry_password_form_password_update_ =
        provisionally_saved_form_->username_element.empty() &&
        provisionally_saved_form_->new_password_element.empty();

    is_new_login_ = false;
    if (best_update_match) {
      pending_credentials_ = *best_update_match;
    } else if (has_generated_password_) {
      // If a password was generated and we didn't find match we have to save it
      // in separate entry since we have to store it but we don't know where.
      CreatePendingCredentialsForNewCredentials();
      is_new_login_ = true;
    } else {
      // We don't care about |pending_credentials_| if we didn't find the best
      // match, since the user will select the correct one.
      pending_credentials_.origin = provisionally_saved_form_->origin;
    }
  } else {
    CreatePendingCredentialsForNewCredentials();
  }

  if (!IsValidAndroidFacetURI(pending_credentials_.signon_realm)) {
    pending_credentials_.action = provisionally_saved_form_->action;
    // If the user selected credentials we autofilled from a PasswordForm
    // that contained no action URL (IE6/7 imported passwords, for example),
    // bless it with the action URL from the observed form. See bug 1107719.
    if (pending_credentials_.action.is_empty())
      pending_credentials_.action = observed_form_.action;
  }

  pending_credentials_.password_value = password_to_save;
  pending_credentials_.preferred = provisionally_saved_form_->preferred;

  // If we're dealing with an API-driven provisionally saved form, then take
  // the server provided values. We don't do this for non-API forms, as
  // those will never have those members set.
  if (provisionally_saved_form_->type == autofill::PasswordForm::TYPE_API) {
    pending_credentials_.skip_zero_click =
        provisionally_saved_form_->skip_zero_click;
    pending_credentials_.display_name = provisionally_saved_form_->display_name;
    pending_credentials_.federation_origin =
        provisionally_saved_form_->federation_origin;
    pending_credentials_.icon_url = provisionally_saved_form_->icon_url;
    // Take the correct signon_realm for federated credentials.
    pending_credentials_.signon_realm = provisionally_saved_form_->signon_realm;
  }

  if (user_action_ == kUserActionOverridePassword &&
      pending_credentials_.type == PasswordForm::TYPE_GENERATED &&
      !has_generated_password_) {
    metrics_util::LogPasswordGenerationSubmissionEvent(
        metrics_util::PASSWORD_OVERRIDDEN);
    pending_credentials_.type = PasswordForm::TYPE_MANUAL;
  }

  if (has_generated_password_)
    pending_credentials_.type = PasswordForm::TYPE_GENERATED;

  // In case of change password forms we need to leave
  // |provisionally_saved_form_| in order to be able to determine which field is
  // password and which is a new password during sending a vote in other cases
  // we can reset |provisionally_saved_form_|.
  if (!client_->IsUpdatePasswordUIEnabled() ||
      !provisionally_saved_form_->IsPossibleChangePasswordForm())
    provisionally_saved_form_.reset();
}

uint32_t PasswordFormManager::ScoreResult(const PasswordForm& candidate) const {
  DCHECK_EQ(state_, MATCHING_PHASE);
  DCHECK(!candidate.blacklisted_by_user);
  // For scoring of candidate login data:
  // The most important element that should match is the signon_realm followed
  // by the origin, the action, the password name, the submit button name, and
  // finally the username input field name.
  // If public suffix origin match was not used, it gives an addition of
  // 128 (1 << 7).
  // Exact origin match gives an addition of 64 (1 << 6) + # of matching url
  // dirs.
  // Partial match gives an addition of 32 (1 << 5) + # matching url dirs
  // That way, a partial match cannot trump an exact match even if
  // the partial one matches all other attributes (action, elements) (and
  // regardless of the matching depth in the URL path).

  // When comparing path segments, only consider at most 63 of them, so that the
  // potential gain from shared path prefix is not more than from an exact
  // origin match.
  const size_t kSegmentCountCap = 63;
  const size_t capped_form_path_segment_count =
      std::min(form_path_segments_.size(), kSegmentCountCap);

  uint32_t score = 0u;
  if (!candidate.is_public_suffix_match) {
    score += 1u << 8;
  }

  if (candidate.preferred)
    score += 1u << 7;

  if (candidate.origin == observed_form_.origin) {
    // This check is here for the most common case which
    // is we have a single match in the db for the given host,
    // so we don't generally need to walk the entire URL path (the else
    // clause).
    score += (1u << 6) + static_cast<uint32_t>(capped_form_path_segment_count);
  } else {
    // Walk the origin URL paths one directory at a time to see how
    // deep the two match.
    std::vector<std::string> candidate_path_segments =
        SplitPathToSegments(candidate.origin.path());
    size_t depth = 0u;
    const size_t max_dirs = std::min(capped_form_path_segment_count,
                                     candidate_path_segments.size());
    while ((depth < max_dirs) &&
           (form_path_segments_[depth] == candidate_path_segments[depth])) {
      depth++;
      score++;
    }
    // do we have a partial match?
    score += (depth > 0u) ? 1u << 5 : 0u;
  }
  if (observed_form_.scheme == PasswordForm::SCHEME_HTML) {
    if (candidate.action == observed_form_.action)
      score += 1u << 3;
    if (candidate.password_element == observed_form_.password_element)
      score += 1u << 2;
    if (candidate.submit_element == observed_form_.submit_element)
      score += 1u << 1;
    if (candidate.username_element == observed_form_.username_element)
      score += 1u << 0;
  }

  return score;
}

bool PasswordFormManager::IsBlacklistMatch(
    const autofill::PasswordForm& blacklisted_form) const {
  DCHECK(blacklisted_form.blacklisted_by_user);

  if (blacklisted_form.is_public_suffix_match)
    return false;
  if (blacklisted_form.origin.GetOrigin() != observed_form_.origin.GetOrigin())
    return false;
  if (observed_form_.scheme == PasswordForm::SCHEME_HTML) {
    return (blacklisted_form.origin.path() == observed_form_.origin.path()) ||
           (AreStringsEqualOrEmpty(blacklisted_form.submit_element,
                                   observed_form_.submit_element) &&
            AreStringsEqualOrEmpty(blacklisted_form.password_element,
                                   observed_form_.password_element) &&
            AreStringsEqualOrEmpty(blacklisted_form.username_element,
                                   observed_form_.username_element));
  }
  return true;
}

PasswordForm* PasswordFormManager::FindBestMatchForUpdatePassword(
    const base::string16& password) const {
  if (best_matches_.size() == 1 && !has_generated_password_) {
    // In case when the user has only one credential and the current password is
    // not generated, consider it the same as is being saved.
    return best_matches_.begin()->second.get();
  }
  if (password.empty())
    return nullptr;

  for (auto it = best_matches_.begin(); it != best_matches_.end(); ++it) {
    if (it->second->password_value == password)
      return it->second.get();
  }
  return nullptr;
}

PasswordForm* PasswordFormManager::FindBestSavedMatch(
    const PasswordForm* form) const {
  PasswordFormMap::const_iterator it = best_matches_.find(form->username_value);
  if (it != best_matches_.end())
    return it->second.get();
  if (form->type == autofill::PasswordForm::TYPE_API)
    // Match Credential API forms only by username.
    return nullptr;
  if (!form->username_element.empty() || !form->new_password_element.empty())
    return nullptr;
  for (const auto& stored_match : best_matches_) {
    if (stored_match.second->password_value == form->password_value)
      return stored_match.second.get();
  }
  return nullptr;
}

void PasswordFormManager::CreatePendingCredentialsForNewCredentials() {
  // User typed in a new, unknown username.
  user_action_ = kUserActionOverrideUsernameAndPassword;
  pending_credentials_ = observed_form_;
  if (provisionally_saved_form_->was_parsed_using_autofill_predictions)
    pending_credentials_.username_element =
        provisionally_saved_form_->username_element;
  pending_credentials_.username_value =
      provisionally_saved_form_->username_value;
  pending_credentials_.other_possible_usernames =
      provisionally_saved_form_->other_possible_usernames;

  // The password value will be filled in later, remove any garbage for now.
  pending_credentials_.password_value.clear();
  pending_credentials_.new_password_value.clear();

  // If this was a sign-up or change password form, the names of the elements
  // are likely different than those on a login form, so do not bother saving
  // them. We will fill them with meaningful values during update when the user
  // goes onto a real login form for the first time.
  if (!provisionally_saved_form_->new_password_element.empty()) {
    pending_credentials_.password_element.clear();
  }
}

void PasswordFormManager::OnNopeUpdateClicked() {
  UploadChangePasswordForm(autofill::NOT_NEW_PASSWORD, std::string());
}

void PasswordFormManager::OnNoInteractionOnUpdate() {
  UploadChangePasswordForm(autofill::PROBABLY_NEW_PASSWORD, std::string());
}

void PasswordFormManager::LogSubmitPassed() {
  if (submit_result_ != kSubmitResultFailed) {
    if (has_generated_password_) {
      metrics_util::LogPasswordGenerationSubmissionEvent(
          metrics_util::PASSWORD_SUBMITTED);
    } else if (generation_available_) {
      metrics_util::LogPasswordGenerationAvailableSubmissionEvent(
          metrics_util::PASSWORD_SUBMITTED);
    }
  }
  submit_result_ = kSubmitResultPassed;
}

void PasswordFormManager::LogSubmitFailed() {
  if (has_generated_password_) {
    metrics_util::LogPasswordGenerationSubmissionEvent(
        metrics_util::GENERATED_PASSWORD_FORCE_SAVED);
  } else if (generation_available_) {
    metrics_util::LogPasswordGenerationAvailableSubmissionEvent(
        metrics_util::PASSWORD_SUBMISSION_FAILED);
  }
  submit_result_ = kSubmitResultFailed;
}

void PasswordFormManager::WipeStoreCopyIfOutdated() {
  DCHECK_NE(PRE_MATCHING_PHASE, state_);

  UMA_HISTOGRAM_BOOLEAN("PasswordManager.StoreReadyWhenWiping",
                        HasCompletedMatching());

  form_saver_->WipeOutdatedCopies(pending_credentials_, &best_matches_,
                                  &preferred_match_);
}

void PasswordFormManager::SaveGenerationFieldDetectedByClassifier(
    const base::string16& generation_field) {
  form_classifier_outcome_ =
      generation_field.empty() ? kNoGenerationElement : kFoundGenerationElement;
  generation_element_detected_by_classifier_ = generation_field;
}

void PasswordFormManager::SendVotesOnSave() {
  // Upload credentials the first time they are saved. This data is used
  // by password generation to help determine account creation sites.
  // Credentials that have been previously used (e.g., PSL matches) are checked
  // to see if they are valid account creation forms.
  if (pending_credentials_.times_used == 0) {
    if (!observed_form_.IsPossibleChangePasswordFormWithoutUsername()) {
      base::string16 username_field;
      autofill::ServerFieldType password_type = autofill::PASSWORD;
      if (does_look_like_signup_form_) {
        username_field = pending_credentials_.username_element;
        password_type = autofill::PROBABLY_ACCOUNT_CREATION_PASSWORD;
      }
      UploadPasswordForm(pending_credentials_.form_data, username_field,
                         password_type, std::string());
    }
  } else {
    if (!observed_form_.IsPossibleChangePasswordFormWithoutUsername())
      SendAutofillVotes(observed_form_, &pending_credentials_);
  }
}

base::Optional<PasswordForm> PasswordFormManager::UpdatePendingAndGetOldKey(
    std::vector<const PasswordForm*>* credentials_to_update) {
  base::Optional<PasswordForm> old_primary_key;
  bool update_related_credentials = false;

  if (!selected_username_.empty()) {
    // Username has changed. We set this selected username as the real
    // username. Given that |username_value| is part of the Sync and
    // PasswordStore primary key, the old primary key must be supplied.
    old_primary_key = pending_credentials_;
    pending_credentials_.username_value = selected_username_;
    // TODO(crbug.com/188908) This branch currently never executes (bound to
    // the other usernames experiment). Updating related credentials would be
    // complicated, so we skip that, given it influences no users.
    update_related_credentials = false;
  } else if (observed_form_.new_password_element.empty() &&
             pending_credentials_.federation_origin.unique() &&
             !IsValidAndroidFacetURI(pending_credentials_.signon_realm) &&
             (pending_credentials_.password_element.empty() ||
              pending_credentials_.username_element.empty() ||
              pending_credentials_.submit_element.empty())) {
    // If |observed_form_| is a sign-in form and some of the element names are
    // empty, it is likely the first time a credential saved on a
    // sign-up/change password form is used.  Given that |password_element| and
    // |username_element| are part of Sync and PasswordStore primary key, the
    // old primary key must be used if the new names shal be saved.
    old_primary_key = pending_credentials_;
    pending_credentials_.password_element = observed_form_.password_element;
    pending_credentials_.username_element = observed_form_.username_element;
    pending_credentials_.submit_element = observed_form_.submit_element;
    update_related_credentials = true;
  } else {
    update_related_credentials =
        pending_credentials_.federation_origin.unique();
  }

  // If this was a password update, then update all non-best matches entries
  // with the same username and the same old password.
  if (update_related_credentials) {
    PasswordFormMap::const_iterator updated_password_it =
        best_matches_.find(pending_credentials_.username_value);
    DCHECK(best_matches_.end() != updated_password_it);
    const base::string16& old_password =
        updated_password_it->second->password_value;
    for (const auto& not_best_match : not_best_matches_) {
      if (not_best_match->username_value ==
              pending_credentials_.username_value &&
          not_best_match->password_value == old_password) {
        not_best_match->password_value = pending_credentials_.password_value;
        credentials_to_update->push_back(not_best_match.get());
      }
    }
  }

  return old_primary_key;
}

}  // namespace password_manager
