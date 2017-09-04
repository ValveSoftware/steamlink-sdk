// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/log_manager.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager.h"

using autofill::AutofillUploadContents;

namespace password_manager {

namespace {

// Replaces all non-digits in |str| by spaces.
std::string ScrubNonDigit(std::string str) {
  std::replace_if(str.begin(), str.end(),
                  [](char c) { return !base::IsAsciiDigit(c); }, ' ');
  return str;
}

std::string GenerationTypeToString(
    AutofillUploadContents::Field::PasswordGenerationType type) {
  switch (type) {
    case AutofillUploadContents::Field::NO_GENERATION:
      return std::string();
    case AutofillUploadContents::Field::
        AUTOMATICALLY_TRIGGERED_GENERATION_ON_SIGN_UP_FORM:
      return "Generation on sign-up";
    case AutofillUploadContents::Field::
        AUTOMATICALLY_TRIGGERED_GENERATION_ON_CHANGE_PASSWORD_FORM:
      return "Generation on change password";
    case AutofillUploadContents::Field::
        MANUALLY_TRIGGERED_GENERATION_ON_SIGN_UP_FORM:
      return "Manual generation on sign-up";
    case AutofillUploadContents::Field::
        MANUALLY_TRIGGERED_GENERATION_ON_CHANGE_PASSWORD_FORM:
      return "Manual generation on change password";
    case AutofillUploadContents::Field::IGNORED_GENERATION_POPUP:
      return "Generation ignored";
    default:
      NOTREACHED();
  }
  return std::string();
}

std::string ClassifierOutcomeToString(
    AutofillUploadContents::Field::FormClassifierOutcome outcome) {
  switch (outcome) {
    case AutofillUploadContents::Field::NO_OUTCOME:
      return std::string();
    case AutofillUploadContents::Field::NON_GENERATION_ELEMENT:
      return "Non generation element";
    case AutofillUploadContents::Field::GENERATION_ELEMENT:
      return "Generation element";
  }
  NOTREACHED();
  return std::string();
}

}  // namespace

BrowserSavePasswordProgressLogger::BrowserSavePasswordProgressLogger(
    const LogManager* log_manager)
    : log_manager_(log_manager) {
  DCHECK(log_manager_);
}

void BrowserSavePasswordProgressLogger::LogFormSignatures(
    SavePasswordProgressLogger::StringID label,
    const autofill::PasswordForm& form) {
  autofill::FormStructure form_structure(form.form_data);
  std::string message = GetStringFromID(label) + ": {\n";
  message += GetStringFromID(STRING_FORM_SIGNATURE) + ": " +
             ScrubNonDigit(form_structure.FormSignatureAsStr()) + "\n";
  message += GetStringFromID(STRING_SIGNON_REALM) + ": " +
             ScrubURL(GURL(form.signon_realm)) + "\n";
  message +=
      GetStringFromID(STRING_ORIGIN) + ": " + ScrubURL(form.origin) + "\n";
  message +=
      GetStringFromID(STRING_ACTION) + ": " + ScrubURL(form.action) + "\n";
  message += GetStringFromID(STRING_FORM_NAME) + ": " +
             ScrubElementID(form.form_data.name) + "\n";
  message += FormStructureToFieldsLogString(form_structure);
  message += "}";
  SendLog(message);
}

void BrowserSavePasswordProgressLogger::LogFormStructure(
    StringID label,
    const autofill::FormStructure& form_structure) {
  std::string message = GetStringFromID(label) + ": {\n";
  message += GetStringFromID(STRING_FORM_SIGNATURE) + ": " +
             ScrubNonDigit(form_structure.FormSignatureAsStr()) + "\n";
  message += GetStringFromID(STRING_ORIGIN) + ": " +
             ScrubURL(form_structure.source_url()) + "\n";
  message += GetStringFromID(STRING_ACTION) + ": " +
             ScrubURL(form_structure.target_url()) + "\n";
  message += FormStructureToFieldsLogString(form_structure);
  message += "}";
  SendLog(message);
}

std::string BrowserSavePasswordProgressLogger::FormStructureToFieldsLogString(
    const autofill::FormStructure& form_structure) {
  std::string result;
  result += GetStringFromID(STRING_FIELDS) + ": " + "\n";
  for (const autofill::AutofillField* field : form_structure) {
    std::string field_info = ScrubElementID(field->name) + ": " +
                             ScrubNonDigit(field->FieldSignatureAsStr()) +
                             ", " + ScrubElementID(field->form_control_type);

    if (!field->autocomplete_attribute.empty())
      field_info += ", " + ScrubElementID(field->autocomplete_attribute);

    if (!field->Type().IsUnknown())
      field_info += ", SERVER_PREDICTION: " + field->Type().ToString();

    for (autofill::ServerFieldType type : field->possible_types())
      field_info +=
          ", VOTE: " + autofill::AutofillType::ServerFieldTypeToString(type);

    std::string generation = GenerationTypeToString(field->generation_type());
    if (!generation.empty())
      field_info += ", GENERATION_EVENT: " + generation;

    std::string classifier_outcome =
        ClassifierOutcomeToString(field->form_classifier_outcome());
    if (!classifier_outcome.empty())
      field_info += ", CLIENT_SIDE_CLASSIFIER: " + classifier_outcome;

    result += field_info + "\n";
  }

  return result;
}

BrowserSavePasswordProgressLogger::~BrowserSavePasswordProgressLogger() {}

void BrowserSavePasswordProgressLogger::SendLog(const std::string& log) {
  log_manager_->LogSavePasswordProgress(log);
}

}  // namespace password_manager
