// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/stub_password_manager_driver.h"

namespace password_manager {

StubPasswordManagerDriver::StubPasswordManagerDriver() {
}

StubPasswordManagerDriver::~StubPasswordManagerDriver() {
}

void StubPasswordManagerDriver::FillPasswordForm(
    const autofill::PasswordFormFillData& form_data) {
}

void StubPasswordManagerDriver::AllowPasswordGenerationForForm(
    const autofill::PasswordForm& form) {
}

void StubPasswordManagerDriver::FormsEligibleForGenerationFound(
    const std::vector<autofill::PasswordFormGenerationData>& forms) {}

void StubPasswordManagerDriver::GeneratedPasswordAccepted(
    const base::string16& password) {
}

void StubPasswordManagerDriver::FillSuggestion(const base::string16& username,
                                               const base::string16& password) {
}

void StubPasswordManagerDriver::PreviewSuggestion(
    const base::string16& username,
    const base::string16& password) {
}

void StubPasswordManagerDriver::ShowInitialPasswordAccountSuggestions(
    const autofill::PasswordFormFillData& form_data) {}

void StubPasswordManagerDriver::ClearPreviewedForm() {
}

PasswordGenerationManager*
StubPasswordManagerDriver::GetPasswordGenerationManager() {
  return nullptr;
}

PasswordManager* StubPasswordManagerDriver::GetPasswordManager() {
  return nullptr;
}

PasswordAutofillManager*
StubPasswordManagerDriver::GetPasswordAutofillManager() {
  return nullptr;
}

}  // namespace password_manager
