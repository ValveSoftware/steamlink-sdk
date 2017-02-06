// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/save_password_progress_logger.h"

#include <algorithm>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/common/password_form.h"

using base::checked_cast;
using base::Value;
using base::DictionaryValue;
using base::FundamentalValue;
using base::StringValue;

namespace autofill {

namespace {

// Removes privacy sensitive parts of |url| (currently all but host and scheme).
std::string ScrubURL(const GURL& url) {
  if (url.is_valid())
    return url.GetWithEmptyPath().spec();
  return std::string();
}

// Returns true for all characters which we don't want to see in the logged IDs
// or names of HTML elements.
bool IsUnwantedInElementID(char c) {
  return !(c == '_' || c == '-' ||
           base::IsAsciiAlpha(c) || base::IsAsciiDigit(c));
}

// The UTF-8 version of SavePasswordProgressLogger::ScrubElementID.
std::string ScrubElementID8Bit(std::string element_id) {
  std::replace_if(
      element_id.begin(), element_id.end(), IsUnwantedInElementID, ' ');
  return base::ToLowerASCII(element_id);
}

SavePasswordProgressLogger::StringID FormSchemeToStringID(
    PasswordForm::Scheme scheme) {
  switch (scheme) {
    case PasswordForm::SCHEME_HTML:
      return SavePasswordProgressLogger::STRING_SCHEME_HTML;
    case PasswordForm::SCHEME_BASIC:
      return SavePasswordProgressLogger::STRING_SCHEME_BASIC;
    case PasswordForm::SCHEME_DIGEST:
      return SavePasswordProgressLogger::STRING_SCHEME_DIGEST;
    case PasswordForm::SCHEME_OTHER:
      return SavePasswordProgressLogger::STRING_OTHER;
    case PasswordForm::SCHEME_USERNAME_ONLY:
      return SavePasswordProgressLogger::STRING_SCHEME_USERNAME_ONLY;
  }
  NOTREACHED();
  return SavePasswordProgressLogger::STRING_INVALID;
}

}  // namespace

SavePasswordProgressLogger::SavePasswordProgressLogger() {
}

SavePasswordProgressLogger::~SavePasswordProgressLogger() {
}

void SavePasswordProgressLogger::LogPasswordForm(
    SavePasswordProgressLogger::StringID label,
    const PasswordForm& form) {
  DictionaryValue log;
  log.SetString(GetStringFromID(STRING_SCHEME_MESSAGE),
                GetStringFromID(FormSchemeToStringID(form.scheme)));
  log.SetString(GetStringFromID(STRING_SCHEME_MESSAGE),
                GetStringFromID(FormSchemeToStringID(form.scheme)));
  log.SetString(GetStringFromID(STRING_SIGNON_REALM),
                ScrubURL(GURL(form.signon_realm)));
  log.SetString(GetStringFromID(STRING_ORIGIN), ScrubURL(form.origin));
  log.SetString(GetStringFromID(STRING_ACTION), ScrubURL(form.action));
  log.SetString(GetStringFromID(STRING_USERNAME_ELEMENT),
                ScrubElementID(form.username_element));
  log.SetString(GetStringFromID(STRING_PASSWORD_ELEMENT),
                ScrubElementID(form.password_element));
  log.SetString(GetStringFromID(STRING_NEW_PASSWORD_ELEMENT),
                ScrubElementID(form.new_password_element));
  log.SetBoolean(GetStringFromID(STRING_SSL_VALID), form.ssl_valid);
  log.SetBoolean(GetStringFromID(STRING_PASSWORD_GENERATED),
                 form.type == PasswordForm::TYPE_GENERATED);
  log.SetInteger(GetStringFromID(STRING_TIMES_USED), form.times_used);
  log.SetBoolean(GetStringFromID(STRING_PSL_MATCH),
                 form.is_public_suffix_match);
  LogValue(label, log);
}

void SavePasswordProgressLogger::LogHTMLForm(
    SavePasswordProgressLogger::StringID label,
    const std::string& name_or_id,
    const GURL& action) {
  DictionaryValue log;
  log.SetString(GetStringFromID(STRING_NAME_OR_ID),
                ScrubElementID8Bit(name_or_id));
  log.SetString(GetStringFromID(STRING_ACTION), ScrubURL(action));
  LogValue(label, log);
}

void SavePasswordProgressLogger::LogURL(
    SavePasswordProgressLogger::StringID label,
    const GURL& url) {
  LogValue(label, StringValue(ScrubURL(url)));
}

void SavePasswordProgressLogger::LogBoolean(
    SavePasswordProgressLogger::StringID label,
    bool truth_value) {
  LogValue(label, FundamentalValue(truth_value));
}

void SavePasswordProgressLogger::LogNumber(
    SavePasswordProgressLogger::StringID label,
    int signed_number) {
  LogValue(label, FundamentalValue(signed_number));
}

void SavePasswordProgressLogger::LogNumber(
    SavePasswordProgressLogger::StringID label,
    size_t unsigned_number) {
  int signed_number = checked_cast<int, size_t>(unsigned_number);
  LogNumber(label, signed_number);
}

void SavePasswordProgressLogger::LogMessage(
    SavePasswordProgressLogger::StringID message) {
  LogValue(STRING_MESSAGE, StringValue(GetStringFromID(message)));
}

void SavePasswordProgressLogger::LogValue(StringID label, const Value& log) {
  std::string log_string;
  bool conversion_to_string_successful = base::JSONWriter::WriteWithOptions(
      log, base::JSONWriter::OPTIONS_PRETTY_PRINT, &log_string);
  DCHECK(conversion_to_string_successful);
  SendLog(GetStringFromID(label) + ": " + log_string);
}

// static
std::string SavePasswordProgressLogger::ScrubElementID(
    const base::string16& element_id) {
  return ScrubElementID8Bit(base::UTF16ToUTF8(element_id));
}

// Note 1: Caching the ID->string map in an array would be probably faster, but
// the switch statement is (a) robust against re-ordering, and (b) checks in
// compile-time, that all IDs get a string assigned. The expected frequency of
// calls is low enough (in particular, zero if password manager internals page
// is not open), that optimizing for code robustness is preferred against speed.
// Note 2: Do not use '.' in the message strings -- the ending of the log items
// should be controlled by the logger. Also, some of the messages below can be
// used as dictionary keys.
//
// static
std::string SavePasswordProgressLogger::GetStringFromID(
    SavePasswordProgressLogger::StringID id) {
  switch (id) {
    case SavePasswordProgressLogger::STRING_DECISION_ASK:
      return "Decision: ASK the user";
    case SavePasswordProgressLogger::STRING_DECISION_DROP:
      return "Decision: DROP the password";
    case SavePasswordProgressLogger::STRING_DECISION_SAVE:
      return "Decision: SAVE the password";
    case SavePasswordProgressLogger::STRING_OTHER:
      return "(other)";
    case SavePasswordProgressLogger::STRING_SCHEME_HTML:
      return "HTML";
    case SavePasswordProgressLogger::STRING_SCHEME_BASIC:
      return "Basic";
    case SavePasswordProgressLogger::STRING_SCHEME_DIGEST:
      return "Digest";
    case SavePasswordProgressLogger::STRING_SCHEME_USERNAME_ONLY:
      return "Username only";
    case SavePasswordProgressLogger::STRING_SCHEME_MESSAGE:
      return "Scheme";
    case SavePasswordProgressLogger::STRING_SIGNON_REALM:
      return "Signon realm";
    case SavePasswordProgressLogger::STRING_ORIGIN:
      return "Origin";
    case SavePasswordProgressLogger::STRING_ACTION:
      return "Action";
    case SavePasswordProgressLogger::STRING_USERNAME_ELEMENT:
      return "Username element";
    case SavePasswordProgressLogger::STRING_PASSWORD_ELEMENT:
      return "Password element";
    case SavePasswordProgressLogger::STRING_NEW_PASSWORD_ELEMENT:
      return "New password element";
    case SavePasswordProgressLogger::STRING_SSL_VALID:
      return "SSL valid";
    case SavePasswordProgressLogger::STRING_PASSWORD_GENERATED:
      return "Password generated";
    case SavePasswordProgressLogger::STRING_TIMES_USED:
      return "Times used";
    case SavePasswordProgressLogger::STRING_PSL_MATCH:
      return "PSL match";
    case SavePasswordProgressLogger::STRING_NAME_OR_ID:
      return "Form name or ID";
    case SavePasswordProgressLogger::STRING_MESSAGE:
      return "Message";
    case SavePasswordProgressLogger::STRING_SET_AUTH_METHOD:
      return "LoginHandler::SetAuth";
    case SavePasswordProgressLogger::STRING_AUTHENTICATION_HANDLED:
      return "Authentication already handled";
    case SavePasswordProgressLogger::STRING_LOGINHANDLER_FORM:
      return "LoginHandler reports this form";
    case SavePasswordProgressLogger::STRING_SEND_PASSWORD_FORMS_METHOD:
      return "PasswordAutofillAgent::SendPasswordForms";
    case SavePasswordProgressLogger::STRING_SECURITY_ORIGIN:
      return "Security origin";
    case SavePasswordProgressLogger::STRING_SECURITY_ORIGIN_FAILURE:
      return "Security origin cannot access password manager";
    case SavePasswordProgressLogger::STRING_WEBPAGE_EMPTY:
      return "Webpage is empty";
    case SavePasswordProgressLogger::STRING_NUMBER_OF_ALL_FORMS:
      return "Number of all forms";
    case SavePasswordProgressLogger::STRING_FORM_FOUND_ON_PAGE:
      return "Form found on page";
    case SavePasswordProgressLogger::STRING_FORM_IS_VISIBLE:
      return "Form is visible";
    case SavePasswordProgressLogger::STRING_FORM_IS_PASSWORD:
      return "Form is a password form";
    case SavePasswordProgressLogger::STRING_FORM_IS_NOT_PASSWORD:
      return "Form isn't a password form";
    case SavePasswordProgressLogger::STRING_WILL_SUBMIT_FORM_METHOD:
      return "PasswordAutofillAgent::WillSubmitForm";
    case SavePasswordProgressLogger::STRING_HTML_FORM_FOR_SUBMIT:
      return "HTML form for submit";
    case SavePasswordProgressLogger::STRING_CREATED_PASSWORD_FORM:
      return "Created PasswordForm";
    case SavePasswordProgressLogger::STRING_SUBMITTED_PASSWORD_REPLACED:
      return "Submitted password replaced with the provisionally saved one";
    case SavePasswordProgressLogger::STRING_DID_START_PROVISIONAL_LOAD_METHOD:
      return "PasswordAutofillAgent::DidStartProvisionalLoad";
    case SavePasswordProgressLogger::STRING_FRAME_NOT_MAIN_FRAME:
      return "|frame| is not the main frame";
    case SavePasswordProgressLogger::STRING_PROVISIONALLY_SAVED_FORM_FOR_FRAME:
      return "provisionally_saved_forms_[form_frame]";
    case SavePasswordProgressLogger::STRING_PASSWORD_FORM_FOUND_ON_PAGE:
      return "A password form found on the page";
    case SavePasswordProgressLogger::STRING_PASSWORD_FORM_NOT_FOUND_ON_PAGE:
      return "No password form found on the page";
    case SavePasswordProgressLogger::STRING_PROVISIONALLY_SAVE_PASSWORD_METHOD:
      return "PasswordManager::ProvisionallySavePassword";
    case SavePasswordProgressLogger::STRING_PROVISIONALLY_SAVE_PASSWORD_FORM:
      return "ProvisionallySavePassword form";
    case SavePasswordProgressLogger::STRING_IS_SAVING_ENABLED:
      return "IsSavingAndFillingEnabledForCurrentPage";
    case SavePasswordProgressLogger::STRING_EMPTY_PASSWORD:
      return "Empty password";
    case SavePasswordProgressLogger::STRING_EXACT_MATCH:
      return "Form manager found, exact match";
    case SavePasswordProgressLogger::STRING_MATCH_WITHOUT_ACTION:
      return "Form manager found, match except for action";
    case SavePasswordProgressLogger::STRING_ORIGINS_MATCH:
      return "Form manager found, only origins match";
    case SavePasswordProgressLogger::STRING_MATCHING_NOT_COMPLETE:
      return "No form manager has completed matching";
    case SavePasswordProgressLogger::STRING_FORM_BLACKLISTED:
      return "Form blacklisted";
    case SavePasswordProgressLogger::STRING_INVALID_FORM:
      return "Invalid form";
    case SavePasswordProgressLogger::STRING_SYNC_CREDENTIAL:
      return "Credential is used for syncing passwords";
    case SavePasswordProgressLogger::STRING_PROVISIONALLY_SAVED_FORM:
      return "provisionally_saved_form";
    case SavePasswordProgressLogger::STRING_IGNORE_POSSIBLE_USERNAMES:
      return "Ignore other possible usernames";
    case SavePasswordProgressLogger::STRING_ON_PASSWORD_FORMS_RENDERED_METHOD:
      return "PasswordManager::OnPasswordFormsRendered";
    case SavePasswordProgressLogger::STRING_ON_IN_PAGE_NAVIGATION:
      return "PasswordManager::OnInPageNavigation";
    case SavePasswordProgressLogger::STRING_ON_ASK_USER_OR_SAVE_PASSWORD:
      return "PasswordManager::AskUserOrSavePassword";
    case SavePasswordProgressLogger::STRING_CAN_PROVISIONAL_MANAGER_SAVE_METHOD:
      return "PasswordManager::CanProvisionalManagerSave";
    case SavePasswordProgressLogger::STRING_NO_PROVISIONAL_SAVE_MANAGER:
      return "No provisional save manager";
    case SavePasswordProgressLogger::STRING_NUMBER_OF_VISIBLE_FORMS:
      return "Number of visible forms";
    case SavePasswordProgressLogger::STRING_PASSWORD_FORM_REAPPEARED:
      return "Password form re-appeared";
    case SavePasswordProgressLogger::STRING_SAVING_DISABLED:
      return "Saving disabled";
    case SavePasswordProgressLogger::STRING_NO_MATCHING_FORM:
      return "No matching form";
    case SavePasswordProgressLogger::STRING_SSL_ERRORS_PRESENT:
      return "SSL errors present";
    case SavePasswordProgressLogger::STRING_ONLY_VISIBLE:
      return "only_visible";
    case SavePasswordProgressLogger::STRING_SHOW_PASSWORD_PROMPT:
      return "Show password prompt";
    case SavePasswordProgressLogger::STRING_PASSWORDMANAGER_AUTOFILL:
      return "PasswordManager::Autofill";
    case SavePasswordProgressLogger::STRING_PASSWORDMANAGER_AUTOFILLHTTPAUTH:
      return "PasswordManager::AutofillHttpAuth";
    case SavePasswordProgressLogger::
        STRING_PASSWORDMANAGER_SHOW_INITIAL_PASSWORD_ACCOUNT_SUGGESTIONS:
      return "PasswordManager::ShowInitialPasswordAccountSuggestions";
    case SavePasswordProgressLogger::STRING_WAIT_FOR_USERNAME:
      return "wait_for_username";
    case SavePasswordProgressLogger::STRING_LOGINMODELOBSERVER_PRESENT:
      return "Instances of LoginModelObserver may be present";
    case
      SavePasswordProgressLogger::STRING_WAS_LAST_NAVIGATION_HTTP_ERROR_METHOD:
      return "ChromePasswordManagerClient::WasLastNavigationHTTPError";
    case SavePasswordProgressLogger::STRING_HTTP_STATUS_CODE:
      return "HTTP status code for landing page";
    case
        SavePasswordProgressLogger::STRING_PROVISIONALLY_SAVED_FORM_IS_NOT_HTML:
      return "Provisionally saved form is not HTML";
    case SavePasswordProgressLogger::STRING_ON_REQUEST_DONE_METHOD:
      return "PasswordFormManager::OnRequestDone";
    case SavePasswordProgressLogger::STRING_BEST_SCORE:
      return "best_score";
    case SavePasswordProgressLogger::STRING_ON_GET_STORE_RESULTS_METHOD:
      return "PasswordFormManager::OnGetPasswordStoreResults";
    case SavePasswordProgressLogger::STRING_NUMBER_RESULTS:
      return "Number of results from the password store";
    case SavePasswordProgressLogger::STRING_FETCH_LOGINS_METHOD:
      return "PasswordFormManager::FetchMatchingLoginsFromPasswordStore";
    case SavePasswordProgressLogger::STRING_NO_STORE:
      return "PasswordStore is not available";
    case SavePasswordProgressLogger::STRING_CREATE_LOGIN_MANAGERS_METHOD:
      return "PasswordManager::CreatePendingLoginManagers";
    case SavePasswordProgressLogger::STRING_OLD_NUMBER_LOGIN_MANAGERS:
      return "Number of pending login managers (before)";
    case SavePasswordProgressLogger::STRING_NEW_NUMBER_LOGIN_MANAGERS:
      return "Number of pending login managers (after)";
    case SavePasswordProgressLogger::
        STRING_PASSWORD_MANAGEMENT_ENABLED_FOR_CURRENT_PAGE:
      return "IsPasswordManagementEnabledForCurrentPage";
    case SavePasswordProgressLogger::STRING_SHOW_LOGIN_PROMPT_METHOD:
      return "ShowLoginPrompt";
    case SavePasswordProgressLogger::STRING_NEW_UI_STATE:
      return "The new state of the UI";
    case SavePasswordProgressLogger::STRING_FORM_NOT_AUTOFILLED:
      return "The observed form will not be autofilled";
    case SavePasswordProgressLogger::STRING_CHANGE_PASSWORD_FORM:
      return "Not saving password for a change password form";
    case SavePasswordProgressLogger::STRING_PROCESS_FRAME_METHOD:
      return "PasswordFormManager::ProcessFrame";
    case SavePasswordProgressLogger::STRING_FORM_SIGNATURE:
      return "Signature of form, followed by field signatures";
    case SavePasswordProgressLogger::STRING_FORM_MANAGER_STATE:
      return "PasswordFormManager::state_";
    case SavePasswordProgressLogger::STRING_ADDING_SIGNATURE:
      return "Adding manager for form with this signature";
    case SavePasswordProgressLogger::STRING_UNOWNED_INPUTS_VISIBLE:
      return "Some control elements not associated to a form element are "
             "visible";
    case SavePasswordProgressLogger::STRING_ON_FILL_PASSWORD_FORM_METHOD:
      return "PasswordAutofillAgent::OnFillPasswordForm";
    case SavePasswordProgressLogger::
        STRING_ON_SHOW_INITIAL_PASSWORD_ACCOUNT_SUGGESTIONS:
      return "AutofillAgent::OnShowInitialPasswordAccountSuggestions";
    case SavePasswordProgressLogger::STRING_AMBIGUOUS_OR_EMPTY_NAMES:
      return "ambiguous_or_empty_names";
    case SavePasswordProgressLogger::STRING_NUMBER_OF_POTENTIAL_FORMS_TO_FILL:
      return "Number of potential forms to fill";
    case SavePasswordProgressLogger::STRING_CONTAINS_FILLABLE_USERNAME_FIELD:
      return "form_contains_fillable_username_field";
    case SavePasswordProgressLogger::STRING_USERNAME_FIELD_NAME_EMPTY:
      return "username_field_name empty";
    case SavePasswordProgressLogger::STRING_PASSWORD_FIELD_NAME_EMPTY:
      return "password_field_name empty";
    case SavePasswordProgressLogger::STRING_FORM_DATA_WAIT:
      return "form_data's wait_for_username";
    case SavePasswordProgressLogger::STRING_FILL_USERNAME_AND_PASSWORD_METHOD:
      return "FillUserNameAndPassword in PasswordAutofillAgent";
    case SavePasswordProgressLogger::STRING_USERNAMES_MATCH:
      return "Username to fill matches that on the page";
    case SavePasswordProgressLogger::STRING_MATCH_IN_ADDITIONAL:
      return "Match found in additional logins";
    case SavePasswordProgressLogger::STRING_USERNAME_FILLED:
      return "Filled username element named";
    case SavePasswordProgressLogger::STRING_PASSWORD_FILLED:
      return "Filled password element named";
    case SavePasswordProgressLogger::STRING_INVALID:
      return "INVALID";
      // Intentionally no default: clause here -- all IDs need to get covered.
  }
  NOTREACHED();  // Win compilers don't believe this is unreachable.
  return std::string();
}

}  // namespace autofill
