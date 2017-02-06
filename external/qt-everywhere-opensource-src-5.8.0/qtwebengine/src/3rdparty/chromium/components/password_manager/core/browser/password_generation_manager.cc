// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_generation_manager.h"

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"

namespace password_manager {

PasswordGenerationManager::PasswordGenerationManager(
    PasswordManagerClient* client,
    PasswordManagerDriver* driver)
    : client_(client), driver_(driver) {
}

PasswordGenerationManager::~PasswordGenerationManager() {
}

void PasswordGenerationManager::DetectFormsEligibleForGeneration(
    const std::vector<autofill::FormStructure*>& forms) {
  if (!IsGenerationEnabled())
    return;

  std::vector<autofill::PasswordFormGenerationData>
      forms_eligible_for_generation;
  for (std::vector<autofill::FormStructure*>::const_iterator form_it =
           forms.begin();
       form_it != forms.end(); ++form_it) {
    autofill::FormStructure* form = *form_it;
    for (std::vector<autofill::AutofillField*>::const_iterator field_it =
             form->begin();
         field_it != form->end(); ++field_it) {
      autofill::AutofillField* field = *field_it;
      if (field->server_type() == autofill::ACCOUNT_CREATION_PASSWORD ||
          field->server_type() == autofill::NEW_PASSWORD) {
        forms_eligible_for_generation.push_back(
            autofill::PasswordFormGenerationData{form->form_name(),
                                                 form->target_url(), *field});
        break;
      }
    }
  }
  if (!forms_eligible_for_generation.empty())
    driver_->FormsEligibleForGenerationFound(forms_eligible_for_generation);
}

// In order for password generation to be enabled, we need to make sure:
// (1) Password sync is enabled, and
// (2) Password saving is enabled.
bool PasswordGenerationManager::IsGenerationEnabled() const {
  if (!client_->IsSavingAndFillingEnabledForCurrentPage()) {
    VLOG(2) << "Generation disabled because password saving is disabled";
    return false;
  }

  // Don't consider sync enabled if the user has a custom passphrase. See
  // http://crbug.com/358998 for more details.
  if (client_->GetPasswordSyncState() != SYNCING_NORMAL_ENCRYPTION) {
    VLOG(2) << "Generation disabled because passwords are not being synced or"
             << " custom passphrase is used.";
    return false;
  }

  return true;
}

void PasswordGenerationManager::CheckIfFormClassifierShouldRun() {
  if (autofill::FormStructure::IsAutofillFieldMetadataEnabled())
    driver_->AllowToRunFormClassifier();
}

}  // namespace password_manager
