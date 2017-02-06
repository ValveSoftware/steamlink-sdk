// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_autofill_manager.h"

#include <stddef.h>

#include <algorithm>
#include <vector>

#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/browser/suggestion.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/password_manager/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/strings/grit/components_strings.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace password_manager {

namespace {

// Returns |username| unless it is empty. For an empty |username| returns a
// localised string saying this username is empty. Use this for displaying the
// usernames to the user.
base::string16 ReplaceEmptyUsername(const base::string16& username) {
  if (username.empty())
    return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN);
  return username;
}

// Returns the prettified version of |signon_realm| to be displayed on the UI.
base::string16 GetHumanReadableRealm(const std::string& signon_realm) {
  // For Android application realms, remove the hash component. Otherwise, make
  // no changes.
  FacetURI maybe_facet_uri(FacetURI::FromPotentiallyInvalidSpec(signon_realm));
  if (maybe_facet_uri.IsValidAndroidFacetURI())
    return base::UTF8ToUTF16("android://" +
                             maybe_facet_uri.android_package_name() + "/");
  return base::UTF8ToUTF16(signon_realm);
}

// If |suggestion| was made for an empty username, then return the empty
// string, otherwise return |suggestion|.
base::string16 GetUsernameFromSuggestion(const base::string16& suggestion) {
  return suggestion ==
                 l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN)
             ? base::string16()
             : suggestion;
}

// If |field_suggestion| matches |field_content|, creates a Suggestion out of it
// and appends to |suggestions|.
void AppendSuggestionIfMatching(
    const base::string16& field_suggestion,
    const base::string16& field_contents,
    const std::string& signon_realm,
    bool show_all,
    std::vector<autofill::Suggestion>* suggestions) {
  base::string16 lower_suggestion = base::i18n::ToLower(field_suggestion);
  base::string16 lower_contents = base::i18n::ToLower(field_contents);
  bool prefix_matched_suggestion =
      show_all || base::StartsWith(lower_suggestion, lower_contents,
                                   base::CompareCase::SENSITIVE);
  if (prefix_matched_suggestion ||
      autofill::FieldIsSuggestionSubstringStartingOnTokenBoundary(
          lower_suggestion, lower_contents, true)) {
    autofill::Suggestion suggestion(ReplaceEmptyUsername(field_suggestion));
    suggestion.label = GetHumanReadableRealm(signon_realm);
    suggestion.frontend_id = autofill::POPUP_ITEM_ID_PASSWORD_ENTRY;
    suggestion.match = prefix_matched_suggestion
                           ? autofill::Suggestion::PREFIX_MATCH
                           : autofill::Suggestion::SUBSTRING_MATCH;
    suggestions->push_back(suggestion);
  }
}

// This function attempts to fill |suggestions| and |realms| form |fill_data|
// based on |current_username|. Unless |show_all| is true, it only picks
// suggestions where the username has |current_username| as a prefix.
void GetSuggestions(const autofill::PasswordFormFillData& fill_data,
                    const base::string16& current_username,
                    std::vector<autofill::Suggestion>* suggestions,
                    bool show_all) {
  AppendSuggestionIfMatching(fill_data.username_field.value, current_username,
                             fill_data.preferred_realm, show_all, suggestions);

  for (const auto& login : fill_data.additional_logins) {
    AppendSuggestionIfMatching(login.first, current_username,
                               login.second.realm, show_all, suggestions);
  }

  for (const auto& usernames : fill_data.other_possible_usernames) {
    for (size_t i = 0; i < usernames.second.size(); ++i) {
      AppendSuggestionIfMatching(usernames.second[i], current_username,
                                 usernames.first.realm, show_all, suggestions);
    }
  }

  // Prefix matches should precede other token matches.
  if (autofill::IsFeatureSubstringMatchEnabled()) {
    std::sort(suggestions->begin(), suggestions->end(),
              [](const autofill::Suggestion& a, const autofill::Suggestion& b) {
                return a.match < b.match;
              });
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillManager, public:

PasswordAutofillManager::PasswordAutofillManager(
    PasswordManagerDriver* password_manager_driver,
    autofill::AutofillClient* autofill_client)
    : password_manager_driver_(password_manager_driver),
      autofill_client_(autofill_client),
      weak_ptr_factory_(this) {
}

PasswordAutofillManager::~PasswordAutofillManager() {
}

bool PasswordAutofillManager::FillSuggestion(int key,
                                             const base::string16& username) {
  autofill::PasswordFormFillData fill_data;
  autofill::PasswordAndRealm password_and_realm;
  if (FindLoginInfo(key, &fill_data) &&
      GetPasswordAndRealmForUsername(
          username, fill_data, &password_and_realm)) {
    bool is_android_credential = FacetURI::FromPotentiallyInvalidSpec(
        password_and_realm.realm).IsValidAndroidFacetURI();
    metrics_util::LogFilledCredentialIsFromAndroidApp(is_android_credential);
    password_manager_driver_->FillSuggestion(
        username, password_and_realm.password);
    return true;
  }
  return false;
}

bool PasswordAutofillManager::PreviewSuggestion(
    int key,
    const base::string16& username) {
  autofill::PasswordFormFillData fill_data;
  autofill::PasswordAndRealm password_and_realm;
  if (FindLoginInfo(key, &fill_data) &&
      GetPasswordAndRealmForUsername(
          username, fill_data, &password_and_realm)) {
    password_manager_driver_->PreviewSuggestion(
        username, password_and_realm.password);
    return true;
  }
  return false;
}

void PasswordAutofillManager::OnAddPasswordFormMapping(
    int key,
    const autofill::PasswordFormFillData& fill_data) {
  if (!autofill::IsValidPasswordFormFillData(fill_data))
    return;

  login_to_password_info_[key] = fill_data;
}

void PasswordAutofillManager::OnShowPasswordSuggestions(
    int key,
    base::i18n::TextDirection text_direction,
    const base::string16& typed_username,
    int options,
    const gfx::RectF& bounds) {
  std::vector<autofill::Suggestion> suggestions;
  LoginToPasswordInfoMap::const_iterator fill_data_it =
      login_to_password_info_.find(key);
  if (fill_data_it == login_to_password_info_.end()) {
    // Probably a compromised renderer.
    NOTREACHED();
    return;
  }
  GetSuggestions(fill_data_it->second, typed_username, &suggestions,
                 options & autofill::SHOW_ALL);

  form_data_key_ = key;

  if (suggestions.empty()) {
    autofill_client_->HideAutofillPopup();
    return;
  }

  if (options & autofill::IS_PASSWORD_FIELD) {
    autofill::Suggestion password_field_suggestions(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_PASSWORD_FIELD_SUGGESTIONS_TITLE));
    password_field_suggestions.frontend_id = autofill::POPUP_ITEM_ID_TITLE;
    suggestions.insert(suggestions.begin(), password_field_suggestions);
  }
  autofill_client_->ShowAutofillPopup(bounds,
                                      text_direction,
                                      suggestions,
                                      weak_ptr_factory_.GetWeakPtr());
}

void PasswordAutofillManager::DidNavigateMainFrame() {
  login_to_password_info_.clear();
}

bool PasswordAutofillManager::FillSuggestionForTest(
    int key,
    const base::string16& username) {
  return FillSuggestion(key, username);
}

bool PasswordAutofillManager::PreviewSuggestionForTest(
    int key,
    const base::string16& username) {
  return PreviewSuggestion(key, username);
}

void PasswordAutofillManager::OnPopupShown() {
}

void PasswordAutofillManager::OnPopupHidden() {
}

void PasswordAutofillManager::DidSelectSuggestion(const base::string16& value,
                                                  int identifier) {
  ClearPreviewedForm();
  bool success =
      PreviewSuggestion(form_data_key_, GetUsernameFromSuggestion(value));
  DCHECK(success);
}

void PasswordAutofillManager::DidAcceptSuggestion(const base::string16& value,
                                                  int identifier,
                                                  int position) {
  bool success =
      FillSuggestion(form_data_key_, GetUsernameFromSuggestion(value));
  DCHECK(success);
  autofill_client_->HideAutofillPopup();
}

bool PasswordAutofillManager::GetDeletionConfirmationText(
    const base::string16& value,
    int identifier,
    base::string16* title,
    base::string16* body) {
  return false;
}

bool PasswordAutofillManager::RemoveSuggestion(const base::string16& value,
                                               int identifier) {
  // Password suggestions cannot be deleted this way.
  // See http://crbug.com/329038#c15
  return false;
}

void PasswordAutofillManager::ClearPreviewedForm() {
  password_manager_driver_->ClearPreviewedForm();
}

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillManager, private:

bool PasswordAutofillManager::GetPasswordAndRealmForUsername(
    const base::string16& current_username,
    const autofill::PasswordFormFillData& fill_data,
    autofill::PasswordAndRealm* password_and_realm) {
  // TODO(dubroy): When password access requires some kind of authentication
  // (e.g. Keychain access on Mac OS), use |password_manager_client_| here to
  // fetch the actual password. See crbug.com/178358 for more context.

  // Look for any suitable matches to current field text.
  if (fill_data.username_field.value == current_username) {
    password_and_realm->password = fill_data.password_field.value;
    password_and_realm->realm = fill_data.preferred_realm;
    return true;
  }

  // Scan additional logins for a match.
  for (autofill::PasswordFormFillData::LoginCollection::const_iterator iter =
           fill_data.additional_logins.begin();
       iter != fill_data.additional_logins.end(); ++iter) {
    if (iter->first == current_username) {
      *password_and_realm = iter->second;
      return true;
    }
  }

  for (autofill::PasswordFormFillData::UsernamesCollection::const_iterator
           usernames_iter = fill_data.other_possible_usernames.begin();
       usernames_iter != fill_data.other_possible_usernames.end();
       ++usernames_iter) {
    for (size_t i = 0; i < usernames_iter->second.size(); ++i) {
      if (usernames_iter->second[i] == current_username) {
        password_and_realm->password = usernames_iter->first.password;
        password_and_realm->realm = usernames_iter->first.realm;
        return true;
      }
    }
  }

  return false;
}

bool PasswordAutofillManager::FindLoginInfo(
    int key,
    autofill::PasswordFormFillData* found_password) {
  LoginToPasswordInfoMap::iterator iter = login_to_password_info_.find(key);
  if (iter == login_to_password_info_.end())
    return false;

  *found_password = iter->second;
  return true;
}

}  //  namespace password_manager
