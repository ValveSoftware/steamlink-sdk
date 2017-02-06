// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_data_model.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_manager_test_delegate.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_names.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/phone_number.h"
#include "components/autofill/core/browser/phone_number_i18n.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/rappor/rappor_utils.h"
#include "google_apis/gaia/identity_provider.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

#if defined(OS_IOS)
#include "components/autofill/core/browser/autofill_field_trial_ios.h"
#include "components/autofill/core/browser/keyboard_accessory_metrics_logger.h"
#endif

namespace autofill {

using base::StartsWith;
using base::TimeTicks;

namespace {

const size_t kMaxRecentFormSignaturesToRemember = 3;

// Set a conservative upper bound on the number of forms we are willing to
// cache, simply to prevent unbounded memory consumption.
const size_t kMaxFormCacheSize = 100;

// Precondition: |form_structure| and |form| should correspond to the same
// logical form.  Returns true if any field in the given |section| within |form|
// is auto-filled.
bool SectionIsAutofilled(const FormStructure& form_structure,
                         const FormData& form,
                         const std::string& section) {
  DCHECK_EQ(form_structure.field_count(), form.fields.size());
  for (size_t i = 0; i < form_structure.field_count(); ++i) {
    if (form_structure.field(i)->section() == section &&
        form.fields[i].is_autofilled) {
      return true;
    }
  }

  return false;
}

// Returns the credit card field |value| trimmed from whitespace and with stop
// characters removed.
base::string16 SanitizeCreditCardFieldValue(const base::string16& value) {
  base::string16 sanitized;
  base::TrimWhitespace(value, base::TRIM_ALL, &sanitized);
  // Some sites have ____-____-____-____ in their credit card number fields, for
  // example.
  base::ReplaceChars(sanitized, base::ASCIIToUTF16("-_"),
                     base::ASCIIToUTF16(""), &sanitized);
  return sanitized;
}

// If |name| consists of three whitespace-separated parts and the second of the
// three parts is a single character or a single character followed by a period,
// returns the result of joining the first and third parts with a space.
// Otherwise, returns |name|.
//
// Note that a better way to do this would be to use SplitName from
// src/components/autofill/core/browser/contact_info.cc. However, for now we
// want the logic of which variations of names are considered to be the same to
// exactly match the logic applied on the Payments server.
base::string16 RemoveMiddleInitial(const base::string16& name) {
  std::vector<base::string16> parts =
      base::SplitString(name, base::kWhitespaceUTF16, base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  if (parts.size() == 3 && (parts[1].length() == 1 ||
                            (parts[1].length() == 2 &&
                             base::EndsWith(parts[1], base::ASCIIToUTF16("."),
                                            base::CompareCase::SENSITIVE)))) {
    parts.erase(parts.begin() + 1);
    return base::JoinString(parts, base::ASCIIToUTF16(" "));
  }
  return name;
}

// Returns whether the |field| is predicted as being any kind of name.
bool IsNameType(const AutofillField& field) {
  return field.Type().group() == NAME || field.Type().group() == NAME_BILLING ||
         field.Type().GetStorableType() == CREDIT_CARD_NAME_FULL ||
         field.Type().GetStorableType() == CREDIT_CARD_NAME_FIRST ||
         field.Type().GetStorableType() == CREDIT_CARD_NAME_LAST;
}

// Selects the right name type from the |old_types| to insert into the
// |new_types| based on |is_credit_card|.
void SelectRightNameType(const ServerFieldTypeSet& old_types,
                         ServerFieldTypeSet* new_types,
                         bool is_credit_card) {
  ServerFieldTypeSet upload_types;
  if (old_types.count(NAME_FIRST) && old_types.count(CREDIT_CARD_NAME_FIRST)) {
    if (is_credit_card) {
      new_types->insert(CREDIT_CARD_NAME_FIRST);
    } else {
      new_types->insert(NAME_FIRST);
    }
  } else if (old_types.count(NAME_LAST) &&
             old_types.count(CREDIT_CARD_NAME_LAST)) {
    if (is_credit_card) {
      new_types->insert(CREDIT_CARD_NAME_LAST);
    } else {
      new_types->insert(NAME_LAST);
    }
  } else if (old_types.count(NAME_FULL) &&
             old_types.count(CREDIT_CARD_NAME_FULL)) {
    if (is_credit_card) {
      new_types->insert(CREDIT_CARD_NAME_FULL);
    } else {
      new_types->insert(NAME_FULL);
    }
  } else {
    *new_types = old_types;
  }
}

bool IsCreditCardExpirationType(ServerFieldType type) {
  return type == CREDIT_CARD_EXP_MONTH ||
         type == CREDIT_CARD_EXP_2_DIGIT_YEAR ||
         type == CREDIT_CARD_EXP_4_DIGIT_YEAR ||
         type == CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR ||
         type == CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR;
}

}  // namespace

AutofillManager::AutofillManager(
    AutofillDriver* driver,
    AutofillClient* client,
    const std::string& app_locale,
    AutofillDownloadManagerState enable_download_manager)
    : driver_(driver),
      client_(client),
      payments_client_(
          new payments::PaymentsClient(driver->GetURLRequestContext(), this)),
      app_locale_(app_locale),
      personal_data_(client->GetPersonalDataManager()),
      autocomplete_history_manager_(
          new AutocompleteHistoryManager(driver, client)),
      address_form_event_logger_(
          new AutofillMetrics::FormEventLogger(false /* is_for_credit_card */)),
      credit_card_form_event_logger_(
          new AutofillMetrics::FormEventLogger(true /* is_for_credit_card */)),
      has_logged_autofill_enabled_(false),
      has_logged_address_suggestions_count_(false),
      did_show_suggestions_(false),
      user_did_type_(false),
      user_did_autofill_(false),
      user_did_edit_autofilled_field_(false),
      user_did_accept_upload_prompt_(false),
      external_delegate_(NULL),
      test_delegate_(NULL),
      weak_ptr_factory_(this) {
  if (enable_download_manager == ENABLE_AUTOFILL_DOWNLOAD_MANAGER) {
    download_manager_.reset(new AutofillDownloadManager(driver, this));
  }
  CountryNames::SetLocaleString(app_locale_);
  if (personal_data_ && client_)
    personal_data_->OnSyncServiceInitialized(client_->GetSyncService());
}

AutofillManager::~AutofillManager() {}

// static
void AutofillManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kAutofillEnabled,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAutofillProfileUseDatesFixed, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kAutofillLastVersionDeduped, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  // These choices are made on a per-device basis, so they're not syncable.
  registry->RegisterBooleanPref(prefs::kAutofillWalletImportEnabled, true);
  registry->RegisterBooleanPref(
      prefs::kAutofillWalletImportStorageCheckboxState, true);
}

void AutofillManager::SetExternalDelegate(AutofillExternalDelegate* delegate) {
  // TODO(jrg): consider passing delegate into the ctor.  That won't
  // work if the delegate has a pointer to the AutofillManager, but
  // future directions may not need such a pointer.
  external_delegate_ = delegate;
  autocomplete_history_manager_->SetExternalDelegate(delegate);
}

void AutofillManager::ShowAutofillSettings() {
  client_->ShowAutofillSettings();
}

bool AutofillManager::ShouldShowScanCreditCard(const FormData& form,
                                               const FormFieldData& field) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableCreditCardScan)) {
    return false;
  }

  if (!client_->HasCreditCardScanFeature())
    return false;

  AutofillField* autofill_field = GetAutofillField(form, field);
  if (!autofill_field ||
      autofill_field->Type().GetStorableType() != CREDIT_CARD_NUMBER) {
    return false;
  }

  static const int kShowScanCreditCardMaxValueLength = 6;
  return field.value.size() <= kShowScanCreditCardMaxValueLength &&
         base::ContainsOnlyChars(CreditCard::StripSeparators(field.value),
                                 base::ASCIIToUTF16("0123456789"));
}

void AutofillManager::OnFormsSeen(const std::vector<FormData>& forms,
                                  const TimeTicks& timestamp) {
  if (!IsValidFormDataVector(forms))
    return;

  if (!driver_->RendererIsAvailable())
    return;

  bool enabled = IsAutofillEnabled();
  if (!has_logged_autofill_enabled_) {
    AutofillMetrics::LogIsAutofillEnabledAtPageLoad(enabled);
    has_logged_autofill_enabled_ = true;
  }

  if (!enabled)
    return;

  for (const FormData& form : forms) {
    forms_loaded_timestamps_[form] = timestamp;
  }

  ParseForms(forms);
}

bool AutofillManager::OnWillSubmitForm(const FormData& form,
                                       const TimeTicks& timestamp) {
  if (!IsValidFormData(form))
    return false;

  // We will always give Autocomplete a chance to save the data.
  std::unique_ptr<FormStructure> submitted_form = ValidateSubmittedForm(form);
  if (!submitted_form) {
    autocomplete_history_manager_->OnWillSubmitForm(form);
    return false;
  }

  // However, if Autofill has recognized a field as CVC, that shouldn't be
  // saved.
  FormData form_for_autocomplete = submitted_form->ToFormData();
  for (size_t i = 0; i < submitted_form->field_count(); ++i) {
    if (submitted_form->field(i)->Type().GetStorableType() ==
        CREDIT_CARD_VERIFICATION_CODE) {
      form_for_autocomplete.fields[i].should_autocomplete = false;
    }
  }
  autocomplete_history_manager_->OnWillSubmitForm(form_for_autocomplete);

  address_form_event_logger_->OnWillSubmitForm();
  credit_card_form_event_logger_->OnWillSubmitForm();

  StartUploadProcess(std::move(submitted_form), timestamp, true);

  return true;
}

bool AutofillManager::OnFormSubmitted(const FormData& form) {
  if (!IsValidFormData(form))
    return false;

  // We will always give Autocomplete a chance to save the data.
  std::unique_ptr<FormStructure> submitted_form = ValidateSubmittedForm(form);
  if (!submitted_form) {
    return false;
  }

  address_form_event_logger_->OnFormSubmitted();
  credit_card_form_event_logger_->OnFormSubmitted();

  // Update Personal Data with the form's submitted data.
  if (submitted_form->IsAutofillable())
    ImportFormData(*submitted_form);

  return true;
}

void AutofillManager::StartUploadProcess(
    std::unique_ptr<FormStructure> form_structure,
    const TimeTicks& timestamp,
    bool observed_submission) {
  // It is possible for |personal_data_| to be null, such as when used in the
  // Android webview.
  if (!personal_data_)
    return;

  // Only upload server statistics and UMA metrics if at least some local data
  // is available to use as a baseline.
  const std::vector<AutofillProfile*>& profiles = personal_data_->GetProfiles();
  if (observed_submission && form_structure->IsAutofillable()) {
    AutofillMetrics::LogNumberOfProfilesAtAutofillableFormSubmission(
        personal_data_->GetProfiles().size());
  }
  const std::vector<CreditCard*>& credit_cards =
      personal_data_->GetCreditCards();
  if (!profiles.empty() || !credit_cards.empty()) {
    // Copy the profile and credit card data, so that it can be accessed on a
    // separate thread.
    std::vector<AutofillProfile> copied_profiles;
    copied_profiles.reserve(profiles.size());
    for (const AutofillProfile* profile : profiles)
      copied_profiles.push_back(*profile);

    std::vector<CreditCard> copied_credit_cards;
    copied_credit_cards.reserve(credit_cards.size());
    for (const CreditCard* card : credit_cards)
      copied_credit_cards.push_back(*card);

    // Note that ownership of |form_structure| is passed to the second task,
    // using |base::Owned|.
    FormStructure* raw_form = form_structure.get();
    TimeTicks loaded_timestamp =
        forms_loaded_timestamps_[raw_form->ToFormData()];
    driver_->GetBlockingPool()->PostTaskAndReply(
        FROM_HERE,
        base::Bind(&AutofillManager::DeterminePossibleFieldTypesForUpload,
                   copied_profiles, copied_credit_cards, app_locale_, raw_form),
        base::Bind(&AutofillManager::UploadFormDataAsyncCallback,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Owned(form_structure.release()), loaded_timestamp,
                   initial_interaction_timestamp_, timestamp,
                   observed_submission));
  }
}

void AutofillManager::UpdatePendingForm(const FormData& form) {
  // Process the current pending form if different than supplied |form|.
  if (pending_form_data_ && !pending_form_data_->SameFormAs(form)) {
    ProcessPendingFormForUpload();
  }
  // A new pending form is assigned.
  pending_form_data_.reset(new FormData(form));
}

void AutofillManager::ProcessPendingFormForUpload() {
  if (!pending_form_data_)
    return;

  // We get the FormStructure corresponding to |pending_form_data_|, used in the
  // upload process. |pending_form_data_| is reset.
  std::unique_ptr<FormStructure> upload_form =
      ValidateSubmittedForm(*pending_form_data_);
  pending_form_data_.reset();
  if (!upload_form)
    return;

  StartUploadProcess(std::move(upload_form), base::TimeTicks::Now(), false);
}

void AutofillManager::OnTextFieldDidChange(const FormData& form,
                                           const FormFieldData& field,
                                           const TimeTicks& timestamp) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  if (test_delegate_)
    test_delegate_->OnTextFieldChanged();

  FormStructure* form_structure = NULL;
  AutofillField* autofill_field = NULL;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return;

  UpdatePendingForm(form);

  if (!user_did_type_) {
    user_did_type_ = true;
    AutofillMetrics::LogUserHappinessMetric(AutofillMetrics::USER_DID_TYPE);
  }

  if (autofill_field->is_autofilled) {
    autofill_field->is_autofilled = false;
    autofill_field->set_previously_autofilled(true);
    AutofillMetrics::LogUserHappinessMetric(
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD);

    if (!user_did_edit_autofilled_field_) {
      user_did_edit_autofilled_field_ = true;
      AutofillMetrics::LogUserHappinessMetric(
          AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD_ONCE);
    }
  }

  UpdateInitialInteractionTimestamp(timestamp);
}

void AutofillManager::OnQueryFormFieldAutofill(int query_id,
                                               const FormData& form,
                                               const FormFieldData& field,
                                               const gfx::RectF& bounding_box) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  std::vector<Suggestion> suggestions;

  gfx::RectF transformed_box =
      driver_->TransformBoundingBoxToViewportCoordinates(bounding_box);
  external_delegate_->OnQuery(query_id, form, field, transformed_box);

  // Need to refresh models before using the form_event_loggers.
  bool is_autofill_possible = RefreshDataModels();

  FormStructure* form_structure = NULL;
  AutofillField* autofill_field = NULL;
  bool got_autofillable_form =
      GetCachedFormAndField(form, field, &form_structure, &autofill_field) &&
      // Don't send suggestions or track forms that should not be parsed.
      form_structure->ShouldBeParsed();

  // Logging interactions of forms that are autofillable.
  if (got_autofillable_form) {
    if (autofill_field->Type().group() == CREDIT_CARD)
      credit_card_form_event_logger_->OnDidInteractWithAutofillableForm();
    else
      address_form_event_logger_->OnDidInteractWithAutofillableForm();
  }

  if (is_autofill_possible &&
      driver_->RendererIsAvailable() &&
      got_autofillable_form) {
    AutofillType type = autofill_field->Type();
    bool is_filling_credit_card = (type.group() == CREDIT_CARD);
    // On desktop, don't return non credit card related suggestions for forms or
    // fields that have the "autocomplete" attribute set to off.
    if (IsDesktopPlatform() && !is_filling_credit_card &&
        !field.should_autocomplete) {
      return;
    }
    if (is_filling_credit_card) {
      suggestions = GetCreditCardSuggestions(field, type);
    } else {
      suggestions =
          GetProfileSuggestions(*form_structure, field, *autofill_field);
    }
    if (!suggestions.empty()) {
      // Don't provide credit card suggestions for non-secure pages, but do
      // provide them for secure pages with passive mixed content (see impl. of
      // IsContextSecure).
      if (is_filling_credit_card &&
          !client_->IsContextSecure(form_structure->source_url())) {
        Suggestion warning_suggestion(l10n_util::GetStringUTF16(
            IDS_AUTOFILL_WARNING_INSECURE_CONNECTION));
        warning_suggestion.frontend_id = POPUP_ITEM_ID_WARNING_MESSAGE;
        suggestions.assign(1, warning_suggestion);
      } else {
        bool section_is_autofilled =
            SectionIsAutofilled(*form_structure, form,
                                autofill_field->section());
        if (section_is_autofilled) {
          // If the relevant section is auto-filled and the renderer is querying
          // for suggestions, then the user is editing the value of a field.
          // In this case, mimic autocomplete: don't display labels or icons,
          // as that information is redundant. Moreover, filter out duplicate
          // suggestions.
          std::set<base::string16> seen_values;
          for (auto iter = suggestions.begin(); iter != suggestions.end();) {
            if (!seen_values.insert(iter->value).second) {
              // If we've seen this suggestion value before, remove it.
              iter = suggestions.erase(iter);
            } else {
              iter->label.clear();
              iter->icon.clear();
              ++iter;
            }
          }
        }

        // The first time we show suggestions on this page, log the number of
        // suggestions available.
        // TODO(mathp): Differentiate between number of suggestions available
        // (current metric) and number shown to the user.
        if (!has_logged_address_suggestions_count_ && !section_is_autofilled) {
          AutofillMetrics::LogAddressSuggestionsCount(suggestions.size());
          has_logged_address_suggestions_count_ = true;
        }
      }
    }
  }

  // If there are no Autofill suggestions, consider showing Autocomplete
  // suggestions. We will not show Autocomplete suggestions for a field that
  // specifies autocomplete=off or a field that we think is a credit card
  // expiration, cvc or number.
  if (suggestions.empty() && field.should_autocomplete &&
      !(autofill_field &&
        (IsCreditCardExpirationType(autofill_field->Type().GetStorableType()) ||
         autofill_field->Type().GetStorableType() == CREDIT_CARD_NUMBER ||
         autofill_field->Type().GetStorableType() ==
             CREDIT_CARD_VERIFICATION_CODE))) {
    // Suggestions come back asynchronously, so the Autocomplete manager will
    // handle sending the results back to the renderer.
    autocomplete_history_manager_->OnGetAutocompleteSuggestions(
        query_id, field.name, field.value, field.form_control_type);
    return;
  }

  // Send Autofill suggestions (could be an empty list).
  autocomplete_history_manager_->CancelPendingQuery();
  external_delegate_->OnSuggestionsReturned(query_id, suggestions);
}

bool AutofillManager::WillFillCreditCardNumber(const FormData& form,
                                               const FormFieldData& field) {
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return false;

  if (autofill_field->Type().GetStorableType() == CREDIT_CARD_NUMBER)
    return true;

#if defined(OS_IOS)
  // On iOS, we only fill out one field at a time (assuming the new full-form
  // feature isn't enabled). So we only need to check the current field.
  if (!AutofillFieldTrialIOS::IsFullFormAutofillEnabled())
    return false;
#endif

  // If the relevant section is already autofilled, the new fill operation will
  // only fill |autofill_field|.
  if (SectionIsAutofilled(*form_structure, form, autofill_field->section()))
    return false;

  DCHECK_EQ(form_structure->field_count(), form.fields.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    if (form_structure->field(i)->section() == autofill_field->section() &&
        form_structure->field(i)->Type().GetStorableType() ==
            CREDIT_CARD_NUMBER &&
        form.fields[i].value.empty()) {
      return true;
    }
  }

  return false;
}

void AutofillManager::FillOrPreviewCreditCardForm(
    AutofillDriver::RendererFormDataAction action,
    int query_id,
    const FormData& form,
    const FormFieldData& field,
    const CreditCard& credit_card) {
  if (action == AutofillDriver::FORM_DATA_ACTION_FILL) {
    if (credit_card.record_type() == CreditCard::MASKED_SERVER_CARD &&
        WillFillCreditCardNumber(form, field)) {
      unmasking_query_id_ = query_id;
      unmasking_form_ = form;
      unmasking_field_ = field;
      masked_card_ = credit_card;
      GetOrCreateFullCardRequest()->GetFullCard(
          masked_card_, AutofillClient::UNMASK_FOR_AUTOFILL,
          weak_ptr_factory_.GetWeakPtr());
      credit_card_form_event_logger_->OnDidSelectMaskedServerCardSuggestion();
      return;
    }
    credit_card_form_event_logger_->OnDidFillSuggestion(credit_card);
  }

  FillOrPreviewDataModelForm(action, query_id, form, field, credit_card,
                             true /* is_credit_card */,
                             base::string16() /* cvc */);
}

void AutofillManager::FillOrPreviewProfileForm(
    AutofillDriver::RendererFormDataAction action,
    int query_id,
    const FormData& form,
    const FormFieldData& field,
    const AutofillProfile& profile) {
  if (action == AutofillDriver::FORM_DATA_ACTION_FILL)
    address_form_event_logger_->OnDidFillSuggestion(profile);

  FillOrPreviewDataModelForm(action, query_id, form, field, profile,
                             false /* is_credit_card */,
                             base::string16() /* cvc */);
}

void AutofillManager::FillOrPreviewForm(
    AutofillDriver::RendererFormDataAction action,
    int query_id,
    const FormData& form,
    const FormFieldData& field,
    int unique_id) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  // NOTE: RefreshDataModels may invalidate |data_model| because it causes the
  // PersonalDataManager to reload Mac address book entries. Thus it must come
  // before GetProfile or GetCreditCard.
  if (!RefreshDataModels() || !driver_->RendererIsAvailable())
    return;

  const CreditCard* credit_card = nullptr;
  const AutofillProfile* profile = nullptr;
  if (GetCreditCard(unique_id, &credit_card))
    FillOrPreviewCreditCardForm(action, query_id, form, field, *credit_card);
  else if (GetProfile(unique_id, &profile))
    FillOrPreviewProfileForm(action, query_id, form, field, *profile);
}

void AutofillManager::FillCreditCardForm(int query_id,
                                         const FormData& form,
                                         const FormFieldData& field,
                                         const CreditCard& credit_card,
                                         const base::string16& cvc) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field) ||
      !driver_->RendererIsAvailable()) {
    return;
  }

  FillOrPreviewDataModelForm(AutofillDriver::FORM_DATA_ACTION_FILL, query_id,
                             form, field, credit_card, true, cvc);
}

void AutofillManager::OnFocusNoLongerOnForm() {
  ProcessPendingFormForUpload();
}

void AutofillManager::OnDidPreviewAutofillFormData() {
  if (test_delegate_)
    test_delegate_->DidPreviewFormData();
}

void AutofillManager::OnDidFillAutofillFormData(const FormData& form,
                                                const TimeTicks& timestamp) {
  if (test_delegate_)
    test_delegate_->DidFillFormData();

  UpdatePendingForm(form);

  AutofillMetrics::LogUserHappinessMetric(AutofillMetrics::USER_DID_AUTOFILL);
  if (!user_did_autofill_) {
    user_did_autofill_ = true;
    AutofillMetrics::LogUserHappinessMetric(
        AutofillMetrics::USER_DID_AUTOFILL_ONCE);
  }

  UpdateInitialInteractionTimestamp(timestamp);
}

void AutofillManager::DidShowSuggestions(bool is_new_popup,
                                         const FormData& form,
                                         const FormFieldData& field) {
  if (test_delegate_)
    test_delegate_->DidShowSuggestions();
  FormStructure* form_structure = NULL;
  AutofillField* autofill_field = NULL;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return;

  if (is_new_popup) {
    AutofillMetrics::LogUserHappinessMetric(AutofillMetrics::SUGGESTIONS_SHOWN);

    if (!did_show_suggestions_) {
      did_show_suggestions_ = true;
      AutofillMetrics::LogUserHappinessMetric(
          AutofillMetrics::SUGGESTIONS_SHOWN_ONCE);
    }

    if (autofill_field->Type().group() == CREDIT_CARD) {
      credit_card_form_event_logger_->OnDidShowSuggestions();
    } else {
      address_form_event_logger_->OnDidShowSuggestions();
    }
  }
}

void AutofillManager::OnHidePopup() {
  if (!IsAutofillEnabled())
    return;

  autocomplete_history_manager_->CancelPendingQuery();
  client_->HideAutofillPopup();
}

bool AutofillManager::GetDeletionConfirmationText(const base::string16& value,
                                                  int identifier,
                                                  base::string16* title,
                                                  base::string16* body) {
  if (identifier == POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY) {
    if (title)
      title->assign(value);
    if (body) {
      body->assign(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_DELETE_AUTOCOMPLETE_SUGGESTION_CONFIRMATION_BODY));
    }

    return true;
  }

  if (identifier < 0)
    return false;

  const CreditCard* credit_card = nullptr;
  const AutofillProfile* profile = nullptr;
  if (GetCreditCard(identifier, &credit_card)) {
    if (credit_card->record_type() != CreditCard::LOCAL_CARD)
      return false;

    if (title)
      title->assign(credit_card->TypeAndLastFourDigits());
    if (body) {
      body->assign(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_DELETE_CREDIT_CARD_SUGGESTION_CONFIRMATION_BODY));
    }

    return true;
  } else if (GetProfile(identifier, &profile)) {
    if (profile->record_type() != AutofillProfile::LOCAL_PROFILE)
      return false;

    if (title) {
      base::string16 street_address = profile->GetRawInfo(ADDRESS_HOME_CITY);
      if (!street_address.empty())
        title->swap(street_address);
      else
        title->assign(value);
    }
    if (body) {
      body->assign(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_DELETE_PROFILE_SUGGESTION_CONFIRMATION_BODY));
    }

    return true;
  }

  NOTREACHED();
  return false;
}

bool AutofillManager::RemoveAutofillProfileOrCreditCard(int unique_id) {
  std::string guid;
  const CreditCard* credit_card = nullptr;
  const AutofillProfile* profile = nullptr;
  if (GetCreditCard(unique_id, &credit_card)) {
    if (credit_card->record_type() != CreditCard::LOCAL_CARD)
      return false;

    guid = credit_card->guid();
  } else if (GetProfile(unique_id, &profile)) {
    if (profile->record_type() != AutofillProfile::LOCAL_PROFILE)
      return false;

    guid = profile->guid();
  } else {
    NOTREACHED();
    return false;
  }

  personal_data_->RemoveByGUID(guid);
  return true;
}

void AutofillManager::RemoveAutocompleteEntry(const base::string16& name,
                                              const base::string16& value) {
  autocomplete_history_manager_->OnRemoveAutocompleteEntry(name, value);
}

bool AutofillManager::IsShowingUnmaskPrompt() {
  return full_card_request_ && full_card_request_->IsGettingFullCard();
}

const std::vector<FormStructure*>& AutofillManager::GetFormStructures() {
  return form_structures_.get();
}

payments::FullCardRequest* AutofillManager::GetOrCreateFullCardRequest() {
  if (!full_card_request_) {
    full_card_request_.reset(new payments::FullCardRequest(
        client_, payments_client_.get(), personal_data_));
  }

  return full_card_request_.get();
}

void AutofillManager::SetTestDelegate(AutofillManagerTestDelegate* delegate) {
  test_delegate_ = delegate;
}

void AutofillManager::OnSetDataList(const std::vector<base::string16>& values,
                                    const std::vector<base::string16>& labels) {
  if (!IsValidString16Vector(values) ||
      !IsValidString16Vector(labels) ||
      values.size() != labels.size())
    return;

  external_delegate_->SetCurrentDataListValues(values, labels);
}

void AutofillManager::OnLoadedServerPredictions(
    std::string response,
    const std::vector<std::string>& form_signatures) {
  // We obtain the current valid FormStructures represented by
  // |form_signatures|. We invert both lists because most recent forms are at
  // the end of the list (and reverse the resulting pointer vector).
  std::vector<FormStructure*> queried_forms;
  for (const std::string& signature : base::Reversed(form_signatures)) {
    for (FormStructure* cur_form : base::Reversed(form_structures_)) {
      if (cur_form->FormSignature() == signature) {
        queried_forms.push_back(cur_form);
        break;
      }
    }
  }
  std::reverse(queried_forms.begin(), queried_forms.end());

  // If there are no current forms corresponding to the queried signatures, drop
  // the query response.
  if (queried_forms.empty())
    return;

  // Parse and store the server predictions.
  FormStructure::ParseQueryResponse(std::move(response), queried_forms,
                                    client_->GetRapporService());

  // Will log quality metrics for each FormStructure based on the presence of
  // autocomplete attributes, if available.
  for (FormStructure* cur_form : queried_forms)
    cur_form->LogQualityMetricsBasedOnAutocomplete();

  // Forward form structures to the password generation manager to detect
  // account creation forms.
  driver_->PropagateAutofillPredictions(queried_forms);

  // If the corresponding flag is set, annotate forms with the predicted types.
  driver_->SendAutofillTypePredictionsToRenderer(queried_forms);
}

IdentityProvider* AutofillManager::GetIdentityProvider() {
  return client_->GetIdentityProvider();
}

void AutofillManager::OnDidGetRealPan(AutofillClient::PaymentsRpcResult result,
                                      const std::string& real_pan) {
  DCHECK(full_card_request_);
  full_card_request_->OnDidGetRealPan(result, real_pan);
}

void AutofillManager::OnDidGetUploadDetails(
    AutofillClient::PaymentsRpcResult result,
    const base::string16& context_token,
    std::unique_ptr<base::DictionaryValue> legal_message) {
  // TODO(jdonnelly): Log duration.
  if (result == AutofillClient::SUCCESS) {
    // Do *not* call payments_client_->Prepare() here. We shouldn't send
    // credentials until the user has explicitly accepted a prompt to upload.
    upload_request_.context_token = context_token;
    user_did_accept_upload_prompt_ = false;
    client_->ConfirmSaveCreditCardToCloud(
        upload_request_.card, std::move(legal_message),
        base::Bind(&AutofillManager::OnUserDidAcceptUpload,
                   weak_ptr_factory_.GetWeakPtr()));
    client_->LoadRiskData(base::Bind(&AutofillManager::OnDidGetUploadRiskData,
                                     weak_ptr_factory_.GetWeakPtr()));
    AutofillMetrics::LogCardUploadDecisionMetric(
        AutofillMetrics::UPLOAD_OFFERED);
  } else {
    // If the upload details request failed, fall back to a local save. The
    // reasoning here is as follows:
    // - This will sometimes fail intermittently, in which case it might be
    // better to not fall back, because sometimes offering upload and sometimes
    // offering local save is a poor user experience.
    // - However, in some cases, our local configuration limiting the feature to
    // countries that Payments is known to support will not match Payments' own
    // determination of what country the user is located in. In these cases,
    // the upload details request will consistently fail and if we don't fall
    // back to a local save then the user will never be offered any kind of
    // credit card save.
    client_->ConfirmSaveCreditCardLocally(
        upload_request_.card,
        base::Bind(
            base::IgnoreResult(&PersonalDataManager::SaveImportedCreditCard),
            base::Unretained(personal_data_), upload_request_.card));
    AutofillMetrics::LogCardUploadDecisionMetric(
        AutofillMetrics::UPLOAD_NOT_OFFERED_GET_UPLOAD_DETAILS_FAILED);
  }
}

void AutofillManager::OnDidUploadCard(
    AutofillClient::PaymentsRpcResult result) {
  // We don't do anything user-visible if the upload attempt fails.
  // TODO(jdonnelly): Log duration.
}

void AutofillManager::OnFullCardDetails(const CreditCard& card,
                                        const base::string16& cvc) {
  credit_card_form_event_logger_->OnDidFillSuggestion(masked_card_);
  FillCreditCardForm(unmasking_query_id_, unmasking_form_, unmasking_field_,
                     card, cvc);
  masked_card_ = CreditCard();
}

void AutofillManager::OnFullCardError() {
  driver_->RendererShouldClearPreviewedForm();
}

void AutofillManager::OnUserDidAcceptUpload() {
  user_did_accept_upload_prompt_ = true;
  if (!upload_request_.risk_data.empty()) {
    upload_request_.app_locale = app_locale_;
    payments_client_->UploadCard(upload_request_);
  }
}

void AutofillManager::OnDidGetUploadRiskData(const std::string& risk_data) {
  upload_request_.risk_data = risk_data;
  if (user_did_accept_upload_prompt_) {
    upload_request_.app_locale = app_locale_;
    payments_client_->UploadCard(upload_request_);
  }
}

void AutofillManager::OnDidEndTextFieldEditing() {
  external_delegate_->DidEndTextFieldEditing();
}

bool AutofillManager::IsAutofillEnabled() const {
  return ::autofill::IsAutofillEnabled(client_->GetPrefs());
}

bool AutofillManager::IsCreditCardUploadEnabled() {
  return ::autofill::IsCreditCardUploadEnabled(
      client_->GetPrefs(), client_->GetSyncService(),
      GetIdentityProvider()->GetActiveUsername());
}

bool AutofillManager::ShouldUploadForm(const FormStructure& form) {
  return IsAutofillEnabled() && !driver_->IsOffTheRecord() &&
         form.ShouldBeParsed() &&
         (form.active_field_count() >= kRequiredFieldsForUpload ||
          (form.all_fields_are_passwords() &&
           form.active_field_count() >=
               kRequiredFieldsForFormsWithOnlyPasswordFields));
}

void AutofillManager::ImportFormData(const FormStructure& submitted_form) {
  std::unique_ptr<CreditCard> imported_credit_card;
  if (!personal_data_->ImportFormData(
          submitted_form, IsCreditCardUploadEnabled(), &imported_credit_card)) {
    return;
  }

#ifdef ENABLE_FORM_DEBUG_DUMP
  // Debug code for research on what autofill Chrome extracts from the last few
  // forms when submitting credit card data. See DumpAutofillData().
  bool dump_data = base::CommandLine::ForCurrentProcess()->HasSwitch(
      "dump-autofill-data");

  // Save the form data for future dumping.
  if (dump_data) {
    if (recently_autofilled_forms_.size() > 5)
      recently_autofilled_forms_.erase(recently_autofilled_forms_.begin());

    recently_autofilled_forms_.push_back(
        std::map<std::string, base::string16>());
    auto& map = recently_autofilled_forms_.back();
    for (const auto& field : submitted_form) {
      AutofillType type = field->Type();
      // Even though this is for development only, mask full credit card #'s.
      if (type.GetStorableType() == CREDIT_CARD_NUMBER &&
          field->value.size() > 4) {
        map[type.ToString()] = base::ASCIIToUTF16("...(omitted)...") +
                               field->value.substr(field->value.size() - 4, 4);
      } else {
        map[type.ToString()] = field->value;
      }
    }

    DumpAutofillData(!!imported_credit_card);
  }
#endif  // ENABLE_FORM_DEBUG_DUMP

  // No card available to offer save or upload.
  if (!imported_credit_card)
    return;

  if (!IsCreditCardUploadEnabled()) {
    // This block will only be reached if we have observed a new card. In this
    // case, ImportFormData will return false if the card matches one already
    // stored.
    client_->ConfirmSaveCreditCardLocally(
        *imported_credit_card,
        base::Bind(
            base::IgnoreResult(&PersonalDataManager::SaveImportedCreditCard),
            base::Unretained(personal_data_), *imported_credit_card));
  } else {
    // Whereas, because we pass IsCreditCardUploadEnabled() to ImportFormData,
    // this block can be reached on observing either a new card or one already
    // stored locally. We will offer to upload either kind.
    upload_request_ = payments::PaymentsClient::UploadRequestDetails();
    upload_request_.card = *imported_credit_card;

    // Check for a CVC to determine whether we can prompt the user to upload
    // their card. If no CVC is present, do nothing. We could fall back to a
    // local save but we believe that sometimes offering upload and sometimes
    // offering local save is a confusing user experience.
    int cvc;
    for (const AutofillField* field : submitted_form) {
      if (field->Type().GetStorableType() == CREDIT_CARD_VERIFICATION_CODE &&
          base::StringToInt(field->value, &cvc)) {
        upload_request_.cvc = field->value;
        break;
      }
    }
    if (upload_request_.cvc.empty()) {
      AutofillMetrics::LogCardUploadDecisionMetric(
          AutofillMetrics::UPLOAD_NOT_OFFERED_NO_CVC);
      CollectRapportSample(submitted_form.source_url(),
                           "Autofill.CardUploadNotOfferedNoCvc");
      return;
    }

    // Upload also requires recently used or modified addresses that meet the
    // client-side validation rules.
    if (!GetProfilesForCreditCardUpload(*imported_credit_card,
                                        &upload_request_.profiles,
                                        submitted_form.source_url())) {
      return;
    }

    // All required data is available, start the upload process.
    payments_client_->GetUploadDetails(app_locale_);
  }
}

bool AutofillManager::GetProfilesForCreditCardUpload(
    const CreditCard& card,
    std::vector<AutofillProfile>* profiles,
    const GURL& source_url) const {
  std::vector<AutofillProfile> candidate_profiles;
  const base::Time now = base::Time::Now();
  const base::TimeDelta fifteen_minutes = base::TimeDelta::FromMinutes(15);

  // First, collect all of the addresses used recently.
  for (AutofillProfile* profile : personal_data_->GetProfiles()) {
    if ((now - profile->use_date()) < fifteen_minutes ||
        (now - profile->modification_date()) < fifteen_minutes) {
      candidate_profiles.push_back(*profile);
    }
  }
  if (candidate_profiles.empty()) {
    AutofillMetrics::LogCardUploadDecisionMetric(
        AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ADDRESS);
    CollectRapportSample(source_url, "Autofill.CardUploadNotOfferedNoAddress");
    return false;
  }

  // If any of the names on the card or the addresses don't match (where
  // matching is case insensitive and ignores middle initials if present), the
  // candidate set is invalid. This matches the rules for name matching applied
  // server-side by Google Payments and ensures that we don't send upload
  // requests that are guaranteed to fail.
  base::string16 verified_name;
  base::string16 card_name =
      card.GetInfo(AutofillType(CREDIT_CARD_NAME_FULL), app_locale_);
  if (!card_name.empty()) {
    verified_name = RemoveMiddleInitial(card_name);
  }
  for (const AutofillProfile& profile : candidate_profiles) {
    base::string16 address_name =
        profile.GetInfo(AutofillType(NAME_FULL), app_locale_);
    if (!address_name.empty()) {
      if (verified_name.empty()) {
        verified_name = RemoveMiddleInitial(address_name);
      } else {
        // TODO(crbug.com/590307): We only use ASCII case insensitivity here
        // because the feature is launching US-only and middle initials only
        // make sense for Western-style names anyway. As we launch in more
        // countries, we'll need to make the name comparison more sophisticated.
        if (!base::EqualsCaseInsensitiveASCII(
                verified_name, RemoveMiddleInitial(address_name))) {
          AutofillMetrics::LogCardUploadDecisionMetric(
              AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES);
          CollectRapportSample(source_url,
                               "Autofill.CardUploadNotOfferedConflictingNames");
          return false;
        }
      }
    }
  }
  // If neither the card nor any of the addresses have a name associated with
  // them, the candidate set is invalid.
  if (verified_name.empty()) {
    AutofillMetrics::LogCardUploadDecisionMetric(
        AutofillMetrics::UPLOAD_NOT_OFFERED_NO_NAME);
    CollectRapportSample(source_url, "Autofill.CardUploadNotOfferedNoName");
    return false;
  }

  // If any of the candidate addresses have a non-empty zip that doesn't match
  // any other non-empty zip, then the candidate set is invalid.
  base::string16 verified_zip;
  for (const AutofillProfile& profile : candidate_profiles) {
    // TODO(jdonnelly): Use GetInfo instead of GetRawInfo once zip codes are
    // canonicalized. See http://crbug.com/587465.
    base::string16 zip = profile.GetRawInfo(ADDRESS_HOME_ZIP);
    if (!zip.empty()) {
      if (verified_zip.empty()) {
        verified_zip = zip;
      } else {
        // To compare two zips, we check to see if either is a prefix of the
        // other. This allows us to consider a 5-digit zip and a zip+4 to be a
        // match if the first 5 digits are the same without hardcoding any
        // specifics of how postal codes are represented. (They can be numeric
        // or alphanumeric and vary from 3 to 10 digits long by country. See
        // https://en.wikipedia.org/wiki/Postal_code#Presentation.) The Payments
        // backend will apply a more sophisticated address-matching procedure.
        // This check is simply meant to avoid offering upload in cases that are
        // likely to fail.
        if (!(StartsWith(verified_zip, zip, base::CompareCase::SENSITIVE) ||
              StartsWith(zip, verified_zip, base::CompareCase::SENSITIVE))) {
          AutofillMetrics::LogCardUploadDecisionMetric(
              AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS);
          return false;
        }
      }
    }
  }

  // If none of the candidate addresses have a zip, the candidate set is
  // invalid.
  if (verified_zip.empty()) {
    AutofillMetrics::LogCardUploadDecisionMetric(
        AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ZIP_CODE);
    return false;
  }

  profiles->assign(candidate_profiles.begin(), candidate_profiles.end());
  return true;
}

void AutofillManager::CollectRapportSample(const GURL& source_url,
                                           const char* metric_name) const {
  if (source_url.is_valid() && client_->GetRapporService()) {
    rappor::SampleDomainAndRegistryFromGURL(client_->GetRapporService(),
                                            metric_name, source_url);
  }
}

// Note that |submitted_form| is passed as a pointer rather than as a reference
// so that we can get memory management right across threads.  Note also that we
// explicitly pass in all the time stamps of interest, as the cached ones might
// get reset before this method executes.
void AutofillManager::UploadFormDataAsyncCallback(
    const FormStructure* submitted_form,
    const TimeTicks& load_time,
    const TimeTicks& interaction_time,
    const TimeTicks& submission_time,
    bool observed_submission) {
  submitted_form->LogQualityMetrics(
      load_time, interaction_time, submission_time, client_->GetRapporService(),
      did_show_suggestions_, observed_submission);

  if (submitted_form->ShouldBeCrowdsourced())
    UploadFormData(*submitted_form, observed_submission);
}

void AutofillManager::UploadFormData(const FormStructure& submitted_form,
                                     bool observed_submission) {
  if (!download_manager_)
    return;

  // Check if the form is among the forms that were recently auto-filled.
  bool was_autofilled = false;
  std::string form_signature = submitted_form.FormSignature();
  for (const std::string& cur_sig : autofilled_form_signatures_) {
    if (cur_sig == form_signature) {
      was_autofilled = true;
      break;
    }
  }

  ServerFieldTypeSet non_empty_types;
  personal_data_->GetNonEmptyTypes(&non_empty_types);

  download_manager_->StartUploadRequest(
      submitted_form, was_autofilled, non_empty_types,
      std::string() /* login_form_signature */, observed_submission);
}

void AutofillManager::Reset() {
  // Note that upload_request_ is not reset here because the prompt to
  // save a card is shown after page navigation.
  ProcessPendingFormForUpload();
  DCHECK(!pending_form_data_);
  form_structures_.clear();
  address_form_event_logger_.reset(
      new AutofillMetrics::FormEventLogger(false /* is_for_credit_card */));
  credit_card_form_event_logger_.reset(
      new AutofillMetrics::FormEventLogger(true /* is_for_credit_card */));
  has_logged_autofill_enabled_ = false;
  has_logged_address_suggestions_count_ = false;
  did_show_suggestions_ = false;
  user_did_type_ = false;
  user_did_autofill_ = false;
  user_did_edit_autofilled_field_ = false;
  masked_card_ = CreditCard();
  unmasking_query_id_ = -1;
  unmasking_form_ = FormData();
  unmasking_field_ = FormFieldData();
  forms_loaded_timestamps_.clear();
  initial_interaction_timestamp_ = TimeTicks();
  external_delegate_->Reset();
}

AutofillManager::AutofillManager(AutofillDriver* driver,
                                 AutofillClient* client,
                                 PersonalDataManager* personal_data)
    : driver_(driver),
      client_(client),
      payments_client_(
          new payments::PaymentsClient(driver->GetURLRequestContext(), this)),
      app_locale_("en-US"),
      personal_data_(personal_data),
      autocomplete_history_manager_(
          new AutocompleteHistoryManager(driver, client)),
      address_form_event_logger_(
          new AutofillMetrics::FormEventLogger(false /* is_for_credit_card */)),
      credit_card_form_event_logger_(
          new AutofillMetrics::FormEventLogger(true /* is_for_credit_card */)),
      has_logged_autofill_enabled_(false),
      has_logged_address_suggestions_count_(false),
      did_show_suggestions_(false),
      user_did_type_(false),
      user_did_autofill_(false),
      user_did_edit_autofilled_field_(false),
      unmasking_query_id_(-1),
      external_delegate_(NULL),
      test_delegate_(NULL),
      weak_ptr_factory_(this) {
  DCHECK(driver_);
  DCHECK(client_);
  CountryNames::SetLocaleString(app_locale_);
}

bool AutofillManager::RefreshDataModels() {
  if (!IsAutofillEnabled())
    return false;

  // No autofill data to return if the profiles are empty.
  const std::vector<AutofillProfile*>& profiles =
      personal_data_->GetProfiles();
  const std::vector<CreditCard*>& credit_cards =
      personal_data_->GetCreditCards();

  // Updating the FormEventLoggers for addresses and credit cards.
  {
    bool is_server_data_available = false;
    bool is_local_data_available = false;
    for (CreditCard* credit_card : credit_cards) {
      if (credit_card->record_type() == CreditCard::LOCAL_CARD)
        is_local_data_available = true;
      else
        is_server_data_available = true;
    }
    credit_card_form_event_logger_->set_is_server_data_available(
        is_server_data_available);
    credit_card_form_event_logger_->set_is_local_data_available(
        is_local_data_available);
  }
  {
    bool is_server_data_available = false;
    bool is_local_data_available = false;
    for (AutofillProfile* profile : profiles) {
      if (profile->record_type() == AutofillProfile::LOCAL_PROFILE)
        is_local_data_available = true;
      else if (profile->record_type() == AutofillProfile::SERVER_PROFILE)
        is_server_data_available = true;
    }
    address_form_event_logger_->set_is_server_data_available(
        is_server_data_available);
    address_form_event_logger_->set_is_local_data_available(
        is_local_data_available);
  }

  if (profiles.empty() && credit_cards.empty())
    return false;

  return true;
}

bool AutofillManager::GetProfile(int unique_id,
                                 const AutofillProfile** profile) {
  // Unpack the |unique_id| into component parts.
  std::string credit_card_id;
  std::string profile_id;
  SplitFrontendID(unique_id, &credit_card_id, &profile_id);
  *profile = NULL;
  if (base::IsValidGUID(profile_id))
    *profile = personal_data_->GetProfileByGUID(profile_id);
  return !!*profile;
}

bool AutofillManager::GetCreditCard(int unique_id,
                                    const CreditCard** credit_card) {
  // Unpack the |unique_id| into component parts.
  std::string credit_card_id;
  std::string profile_id;
  SplitFrontendID(unique_id, &credit_card_id, &profile_id);
  *credit_card = NULL;
  if (base::IsValidGUID(credit_card_id))
    *credit_card = personal_data_->GetCreditCardByGUID(credit_card_id);
  return !!*credit_card;
}

void AutofillManager::FillOrPreviewDataModelForm(
    AutofillDriver::RendererFormDataAction action,
    int query_id,
    const FormData& form,
    const FormFieldData& field,
    const AutofillDataModel& data_model,
    bool is_credit_card,
    const base::string16& cvc) {
  FormStructure* form_structure = NULL;
  AutofillField* autofill_field = NULL;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return;

  DCHECK(form_structure);
  DCHECK(autofill_field);

  FormData result = form;

  base::string16 profile_full_name;
  std::string profile_language_code;
  if (!is_credit_card) {
    profile_full_name = data_model.GetInfo(
        AutofillType(NAME_FULL), app_locale_);
    profile_language_code =
        static_cast<const AutofillProfile*>(&data_model)->language_code();
  }

  // If the relevant section is auto-filled, we should fill |field| but not the
  // rest of the form.
  if (SectionIsAutofilled(*form_structure, form, autofill_field->section())) {
    for (FormFieldData& iter : result.fields) {
      if (iter.SameFieldAs(field)) {
        base::string16 value =
            data_model.GetInfo(autofill_field->Type(), app_locale_);
        if (AutofillField::FillFormField(*autofill_field,
                                         value,
                                         profile_language_code,
                                         app_locale_,
                                         &iter)) {
          // Mark the cached field as autofilled, so that we can detect when a
          // user edits an autofilled field (for metrics).
          autofill_field->is_autofilled = true;

          // Mark the field as autofilled when a non-empty value is assigned to
          // it. This allows the renderer to distinguish autofilled fields from
          // fields with non-empty values, such as select-one fields.
          iter.is_autofilled = true;

          if (!is_credit_card && !value.empty())
            client_->DidFillOrPreviewField(value, profile_full_name);
        }
        break;
      }
    }

    // Note that this may invalidate |data_model|, particularly if it is a Mac
    // address book entry.
    if (action == AutofillDriver::FORM_DATA_ACTION_FILL)
      personal_data_->RecordUseOf(data_model);

    driver_->SendFormDataToRenderer(query_id, action, result);
    return;
  }

  DCHECK_EQ(form_structure->field_count(), form.fields.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    if (form_structure->field(i)->section() != autofill_field->section())
      continue;

    DCHECK(form_structure->field(i)->SameFieldAs(result.fields[i]));

    const AutofillField* cached_field = form_structure->field(i);
    FieldTypeGroup field_group_type = cached_field->Type().group();

    if (field_group_type == NO_GROUP)
      continue;

    base::string16 value =
        data_model.GetInfo(cached_field->Type(), app_locale_);
    if (is_credit_card &&
        cached_field->Type().GetStorableType() ==
            CREDIT_CARD_VERIFICATION_CODE) {
      value = cvc;
    } else if (is_credit_card && IsCreditCardExpirationType(
                                     cached_field->Type().GetStorableType()) &&
               static_cast<const CreditCard*>(&data_model)
                   ->IsExpired(base::Time::Now())) {
      // Don't fill expired cards expiration date.
      value = base::string16();
    }

    // Must match ForEachMatchingFormField() in form_autofill_util.cc.
    // Only notify autofilling of empty fields and the field that initiated
    // the filling (note that "select-one" controls may not be empty but will
    // still be autofilled).
    bool should_notify =
        !is_credit_card &&
        !value.empty() &&
        (result.fields[i].SameFieldAs(field) ||
         result.fields[i].form_control_type == "select-one" ||
         result.fields[i].value.empty());
    if (AutofillField::FillFormField(*cached_field,
                                     value,
                                     profile_language_code,
                                     app_locale_,
                                     &result.fields[i])) {
      // Mark the cached field as autofilled, so that we can detect when a
      // user edits an autofilled field (for metrics).
      form_structure->field(i)->is_autofilled = true;

      // Mark the field as autofilled when a non-empty value is assigned to
      // it. This allows the renderer to distinguish autofilled fields from
      // fields with non-empty values, such as select-one fields.
      result.fields[i].is_autofilled = true;

      if (should_notify)
        client_->DidFillOrPreviewField(value, profile_full_name);
    }
  }

  autofilled_form_signatures_.push_front(form_structure->FormSignature());
  // Only remember the last few forms that we've seen, both to avoid false
  // positives and to avoid wasting memory.
  if (autofilled_form_signatures_.size() > kMaxRecentFormSignaturesToRemember)
    autofilled_form_signatures_.pop_back();

  // Note that this may invalidate |data_model|, particularly if it is a Mac
  // address book entry.
  if (action == AutofillDriver::FORM_DATA_ACTION_FILL)
    personal_data_->RecordUseOf(data_model);

  driver_->SendFormDataToRenderer(query_id, action, result);
}

std::unique_ptr<FormStructure> AutofillManager::ValidateSubmittedForm(
    const FormData& form) {
  std::unique_ptr<FormStructure> submitted_form(new FormStructure(form));
  if (!ShouldUploadForm(*submitted_form))
    return std::unique_ptr<FormStructure>();

  // Ignore forms not present in our cache.  These are typically forms with
  // wonky JavaScript that also makes them not auto-fillable.
  FormStructure* cached_submitted_form;
  if (!FindCachedForm(form, &cached_submitted_form))
    return std::unique_ptr<FormStructure>();

  submitted_form->UpdateFromCache(*cached_submitted_form);
  return submitted_form;
}

bool AutofillManager::FindCachedForm(const FormData& form,
                                     FormStructure** form_structure) const {
  // Find the FormStructure that corresponds to |form|.
  // Scan backward through the cached |form_structures_|, as updated versions of
  // forms are added to the back of the list, whereas original versions of these
  // forms might appear toward the beginning of the list.  The communication
  // protocol with the crowdsourcing server does not permit us to discard the
  // original versions of the forms.
  *form_structure = NULL;
  for (FormStructure* cur_form : base::Reversed(form_structures_)) {
    if (*cur_form == form) {
      *form_structure = cur_form;

      // The same form might be cached with multiple field counts: in some
      // cases, non-autofillable fields are filtered out, whereas in other cases
      // they are not.  To avoid thrashing the cache, keep scanning until we
      // find a cached version with the same number of fields, if there is one.
      if (cur_form->field_count() == form.fields.size())
        break;
    }
  }

  if (!(*form_structure))
    return false;

  return true;
}

bool AutofillManager::GetCachedFormAndField(const FormData& form,
                                            const FormFieldData& field,
                                            FormStructure** form_structure,
                                            AutofillField** autofill_field) {
  // Find the FormStructure that corresponds to |form|.
  // If we do not have this form in our cache but it is parseable, we'll add it
  // in the call to |UpdateCachedForm()|.
  if (!FindCachedForm(form, form_structure) &&
      !FormStructure(form).ShouldBeParsed()) {
    return false;
  }

  // Update the cached form to reflect any dynamic changes to the form data, if
  // necessary.
  if (!UpdateCachedForm(form, *form_structure, form_structure))
    return false;

  // No data to return if there are no auto-fillable fields.
  if (!(*form_structure)->autofill_count())
    return false;

  // Find the AutofillField that corresponds to |field|.
  *autofill_field = NULL;
  for (AutofillField* current : **form_structure) {
    if (current->SameFieldAs(field)) {
      *autofill_field = current;
      break;
    }
  }

  // Even though we always update the cache, the field might not exist if the
  // website disables autocomplete while the user is interacting with the form.
  // See http://crbug.com/160476
  return *autofill_field != NULL;
}

AutofillField* AutofillManager::GetAutofillField(const FormData& form,
                                                 const FormFieldData& field) {
  if (!personal_data_)
    return NULL;

  FormStructure* form_structure = NULL;
  AutofillField* autofill_field = NULL;
  if (!GetCachedFormAndField(form, field, &form_structure, &autofill_field))
    return NULL;

  if (!form_structure->IsAutofillable())
    return NULL;

  return autofill_field;
}

bool AutofillManager::UpdateCachedForm(const FormData& live_form,
                                       const FormStructure* cached_form,
                                       FormStructure** updated_form) {
  bool needs_update =
      (!cached_form ||
       live_form.fields.size() != cached_form->field_count());
  for (size_t i = 0; !needs_update && i < cached_form->field_count(); ++i)
    needs_update = !cached_form->field(i)->SameFieldAs(live_form.fields[i]);

  if (!needs_update)
    return true;

  if (form_structures_.size() >= kMaxFormCacheSize)
    return false;

  // Add the new or updated form to our cache.
  form_structures_.push_back(new FormStructure(live_form));
  *updated_form = *form_structures_.rbegin();
  (*updated_form)->DetermineHeuristicTypes();

  // If we have cached data, propagate it to the updated form.
  if (cached_form) {
    std::map<base::string16, const AutofillField*> cached_fields;
    for (size_t i = 0; i < cached_form->field_count(); ++i) {
      const AutofillField* field = cached_form->field(i);
      cached_fields[field->unique_name()] = field;
    }

    for (size_t i = 0; i < (*updated_form)->field_count(); ++i) {
      AutofillField* field = (*updated_form)->field(i);
      auto cached_field = cached_fields.find(field->unique_name());
      if (cached_field != cached_fields.end()) {
        field->set_server_type(cached_field->second->server_type());
        field->is_autofilled = cached_field->second->is_autofilled;
        field->set_previously_autofilled(
            cached_field->second->previously_autofilled());
      }
    }

    // Note: We _must not_ remove the original version of the cached form from
    // the list of |form_structures_|.  Otherwise, we break parsing of the
    // crowdsourcing server's response to our query.
  }

  // Annotate the updated form with its predicted types.
  std::vector<FormStructure*> forms(1, *updated_form);
  driver_->SendAutofillTypePredictionsToRenderer(forms);

  return true;
}

std::vector<Suggestion> AutofillManager::GetProfileSuggestions(
    const FormStructure& form,
    const FormFieldData& field,
    const AutofillField& autofill_field) const {
  address_form_event_logger_->OnDidPollSuggestions(field);

  std::vector<ServerFieldType> field_types(form.field_count());
  for (size_t i = 0; i < form.field_count(); ++i) {
    field_types.push_back(form.field(i)->Type().GetStorableType());
  }

  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      autofill_field.Type(), field.value, field.is_autofilled, field_types);

  // Adjust phone number to display in prefix/suffix case.
  if (autofill_field.Type().GetStorableType() == PHONE_HOME_NUMBER) {
    for (size_t i = 0; i < suggestions.size(); ++i) {
      suggestions[i].value = AutofillField::GetPhoneNumberValue(
          autofill_field, suggestions[i].value, field);
    }
  }

  for (size_t i = 0; i < suggestions.size(); ++i) {
    suggestions[i].frontend_id =
        MakeFrontendID(std::string(), suggestions[i].backend_id);
  }
  return suggestions;
}

std::vector<Suggestion> AutofillManager::GetCreditCardSuggestions(
    const FormFieldData& field,
    const AutofillType& type) const {
  credit_card_form_event_logger_->OnDidPollSuggestions(field);

  // The field value is sanitized before attempting to match it to the user's
  // data.
  std::vector<Suggestion> suggestions =
      personal_data_->GetCreditCardSuggestions(
          type, SanitizeCreditCardFieldValue(field.value));
  for (size_t i = 0; i < suggestions.size(); i++) {
    suggestions[i].frontend_id =
        MakeFrontendID(suggestions[i].backend_id, std::string());
  }
  return suggestions;
}

void AutofillManager::ParseForms(const std::vector<FormData>& forms) {
  if (forms.empty())
    return;

  std::vector<FormStructure*> non_queryable_forms;
  std::vector<FormStructure*> queryable_forms;
  for (const FormData& form : forms) {
    const auto parse_form_start_time = base::TimeTicks::Now();

    std::unique_ptr<FormStructure> form_structure(new FormStructure(form));
    form_structure->ParseFieldTypesFromAutocompleteAttributes();
    if (!form_structure->ShouldBeParsed())
      continue;

    form_structure->DetermineHeuristicTypes();

    // Ownership is transferred to |form_structures_| which maintains it until
    // the manager is Reset() or destroyed. It is safe to use references below
    // as long as receivers don't take ownership.
    form_structures_.push_back(std::move(form_structure));

    if (form_structures_.back()->ShouldBeCrowdsourced())
      queryable_forms.push_back(form_structures_.back());
    else
      non_queryable_forms.push_back(form_structures_.back());

    AutofillMetrics::LogParseFormTiming(base::TimeTicks::Now() -
                                        parse_form_start_time);
  }

  if (!queryable_forms.empty() && download_manager_) {
    // Query the server if at least one of the forms was parsed.
    download_manager_->StartQueryRequest(queryable_forms);
  }

  if (!queryable_forms.empty() || !non_queryable_forms.empty()) {
    AutofillMetrics::LogUserHappinessMetric(AutofillMetrics::FORMS_LOADED);
#if defined(OS_IOS)
    // Log this from same location as AutofillMetrics::FORMS_LOADED to ensure
    // that KeyboardAccessoryButtonsIOS and UserHappiness UMA metrics will be
    // directly comparable.
    KeyboardAccessoryMetricsLogger::OnFormsLoaded();
#endif
  }

  // For the |non_queryable_forms|, we have all the field type info we're ever
  // going to get about them.  For the other forms, we'll wait until we get a
  // response from the server.
  driver_->SendAutofillTypePredictionsToRenderer(non_queryable_forms);
}

int AutofillManager::BackendIDToInt(const std::string& backend_id) const {
  if (!base::IsValidGUID(backend_id))
    return 0;

  const auto found = backend_to_int_map_.find(backend_id);
  if (found == backend_to_int_map_.end()) {
    // Unknown one, make a new entry.
    int int_id = backend_to_int_map_.size() + 1;
    backend_to_int_map_[backend_id] = int_id;
    int_to_backend_map_[int_id] = backend_id;
    return int_id;
  }
  return found->second;
}

std::string AutofillManager::IntToBackendID(int int_id) const {
  if (int_id == 0)
    return std::string();

  const auto found = int_to_backend_map_.find(int_id);
  if (found == int_to_backend_map_.end()) {
    NOTREACHED();
    return std::string();
  }
  return found->second;
}

// When sending IDs (across processes) to the renderer we pack credit card and
// profile IDs into a single integer.  Credit card IDs are sent in the high
// word and profile IDs are sent in the low word.
int AutofillManager::MakeFrontendID(
    const std::string& cc_backend_id,
    const std::string& profile_backend_id) const {
  int cc_int_id = BackendIDToInt(cc_backend_id);
  int profile_int_id = BackendIDToInt(profile_backend_id);

  // Should fit in signed 16-bit integers. We use 16-bits each when combining
  // below, and negative frontend IDs have special meaning so we can never use
  // the high bit.
  DCHECK(cc_int_id <= std::numeric_limits<int16_t>::max());
  DCHECK(profile_int_id <= std::numeric_limits<int16_t>::max());

  // Put CC in the high half of the bits.
  return (cc_int_id << std::numeric_limits<uint16_t>::digits) | profile_int_id;
}

// When receiving IDs (across processes) from the renderer we unpack credit card
// and profile IDs from a single integer.  Credit card IDs are stored in the
// high word and profile IDs are stored in the low word.
void AutofillManager::SplitFrontendID(int frontend_id,
                                      std::string* cc_backend_id,
                                      std::string* profile_backend_id) const {
  int cc_int_id = (frontend_id >> std::numeric_limits<uint16_t>::digits) &
      std::numeric_limits<uint16_t>::max();
  int profile_int_id = frontend_id & std::numeric_limits<uint16_t>::max();

  *cc_backend_id = IntToBackendID(cc_int_id);
  *profile_backend_id = IntToBackendID(profile_int_id);
}

void AutofillManager::UpdateInitialInteractionTimestamp(
    const TimeTicks& interaction_timestamp) {
  if (initial_interaction_timestamp_.is_null() ||
      interaction_timestamp < initial_interaction_timestamp_) {
    initial_interaction_timestamp_ = interaction_timestamp;
  }
}

// static
void AutofillManager::DeterminePossibleFieldTypesForUpload(
    const std::vector<AutofillProfile>& profiles,
    const std::vector<CreditCard>& credit_cards,
    const std::string& app_locale,
    FormStructure* submitted_form) {
  // For each field in the |submitted_form|, extract the value.  Then for each
  // profile or credit card, identify any stored types that match the value.
  for (size_t i = 0; i < submitted_form->field_count(); ++i) {
    AutofillField* field = submitted_form->field(i);
    ServerFieldTypeSet matching_types;

    base::string16 value;
    base::TrimWhitespace(field->value, base::TRIM_ALL, &value);

    for (const AutofillProfile& profile : profiles)
      profile.GetMatchingTypes(value, app_locale, &matching_types);
    for (const CreditCard& card : credit_cards)
      card.GetMatchingTypes(value, app_locale, &matching_types);

    if (matching_types.empty())
      matching_types.insert(UNKNOWN_TYPE);

    field->set_possible_types(matching_types);
  }

  AutofillManager::DisambiguateUploadTypes(submitted_form);
}

// static
void AutofillManager::DisambiguateUploadTypes(FormStructure* form) {
  for (size_t i = 0; i < form->field_count(); ++i) {
    AutofillField* field = form->field(i);
    const ServerFieldTypeSet& upload_types = field->possible_types();

    if (upload_types.size() == 2) {
      if (upload_types.count(ADDRESS_HOME_LINE1) &&
          upload_types.count(ADDRESS_HOME_STREET_ADDRESS)) {
        AutofillManager::DisambiguateAddressUploadTypes(form, i);
      } else if (upload_types.count(PHONE_HOME_CITY_AND_NUMBER) &&
                 upload_types.count(PHONE_HOME_WHOLE_NUMBER)) {
        AutofillManager::DisambiguatePhoneUploadTypes(form, i);
      } else if ((upload_types.count(NAME_FULL) &&
                  upload_types.count(CREDIT_CARD_NAME_FULL)) ||
                 (upload_types.count(NAME_FIRST) &&
                  upload_types.count(CREDIT_CARD_NAME_FIRST)) ||
                 (upload_types.count(NAME_LAST) &&
                  upload_types.count(CREDIT_CARD_NAME_LAST))) {
        AutofillManager::DisambiguateNameUploadTypes(form, i, upload_types);
      }
    }
  }
}

// static
void AutofillManager::DisambiguateAddressUploadTypes(FormStructure* form,
                                                     size_t current_index) {
  // This case happens when the profile has only one address line.
  // Therefore the address line one and the street address (the whole
  // address) have the same value and match.

  // If the field is followed by a field that is predicted to be an
  // address line two and is empty, we can safely assume that this field
  // is an address line one field. Otherwise it's a whole address field.
  ServerFieldTypeSet matching_types;
  size_t next_index = current_index + 1;
  if (next_index < form->field_count() &&
      form->field(next_index)->Type().GetStorableType() == ADDRESS_HOME_LINE2 &&
      form->field(next_index)->possible_types().count(EMPTY_TYPE)) {
    matching_types.insert(ADDRESS_HOME_LINE1);
  } else {
    matching_types.insert(ADDRESS_HOME_STREET_ADDRESS);
  }

  AutofillField* field = form->field(current_index);
  field->set_possible_types(matching_types);
}

// static
void AutofillManager::DisambiguatePhoneUploadTypes(FormStructure* form,
                                                   size_t current_index) {
  // This case happens for profiles that have no country code saved.
  // Therefore, both the whole number and the city code and number have
  // the same value and match.

  // Since the form was submitted, it is safe to assume that the form
  // didn't require a country code. Thus, only PHONE_HOME_CITY_AND_NUMBER
  // needs to be uploaded.
  ServerFieldTypeSet matching_types;
  matching_types.insert(PHONE_HOME_CITY_AND_NUMBER);

  AutofillField* field = form->field(current_index);
  field->set_possible_types(matching_types);
}

// static
void AutofillManager::DisambiguateNameUploadTypes(
    FormStructure* form,
    size_t current_index,
    const ServerFieldTypeSet& upload_types) {
  // This case happens when both a profile and a credit card have the same
  // name.

  // If the ambiguous field has either a previous or next field that is
  // not name related, use that information to determine whether the field
  // is a name or a credit card name.
  // If the ambiguous field has both a previous or next field that is not
  // name related, if they are both from the same group, use that group to
  // decide this field's type. Otherwise, there is no safe way to
  // disambiguate.

  // Look for a previous non name related field.
  bool has_found_previous_type = false;
  bool is_previous_credit_card = false;
  size_t index = current_index;
  while (index != 0 && !has_found_previous_type) {
    --index;
    AutofillField* prev_field = form->field(index);
    if (!IsNameType(*prev_field)) {
      has_found_previous_type = true;
      is_previous_credit_card = prev_field->Type().group() == CREDIT_CARD;
    }
  }

  // Look for a next non name related field.
  bool has_found_next_type = false;
  bool is_next_credit_card = false;
  index = current_index;
  while (++index < form->field_count() && !has_found_next_type) {
    AutofillField* next_field = form->field(index);
    if (!IsNameType(*next_field)) {
      has_found_next_type = true;
      is_next_credit_card = next_field->Type().group() == CREDIT_CARD;
    }
  }

  // At least a previous or next field type must have been found in order to
  // disambiguate this field.
  if (has_found_previous_type || has_found_next_type) {
    // If both a previous type and a next type are found and not from the same
    // name group there is no sure way to disambiguate.
    if (has_found_previous_type && has_found_next_type &&
        (is_previous_credit_card != is_next_credit_card)) {
      return;
    }

    // Otherwise, use the previous (if it was found) or next field group to
    // decide whether the field is a name or a credit card name.
    ServerFieldTypeSet matching_types;
    if (has_found_previous_type) {
      SelectRightNameType(upload_types, &matching_types,
                          is_previous_credit_card);
    } else {
      SelectRightNameType(upload_types, &matching_types, is_next_credit_card);
    }

    AutofillField* field = form->field(current_index);
    field->set_possible_types(matching_types);
  }
}

#ifdef ENABLE_FORM_DEBUG_DUMP
void AutofillManager::DumpAutofillData(bool imported_cc) const {
  base::ThreadRestrictions::ScopedAllowIO allow_id;

  // This code dumps the last few forms seen on the current tab to a file on
  // the desktop. This is only enabled when a specific command line flag is
  // passed for manual analysis of the address context information available
  // when offering to save credit cards in a checkout session. This is to
  // help developers experimenting with better card saving features.
  base::FilePath path;
  if (!PathService::Get(base::DIR_USER_DESKTOP, &path))
    return;
  path = path.Append(FILE_PATH_LITERAL("autofill_debug_dump.txt"));
  FILE* file = base::OpenFile(path, "a");
  if (!file)
    return;

  fputs("------------------------------------------------------\n", file);
  if (imported_cc)
    fputs("Got a new credit card on CC form:\n", file);
  else
    fputs("Submitted form:\n", file);
  for (int i = static_cast<int>(recently_autofilled_forms_.size()) - 1;
       i >= 0; i--) {
    for (const auto& pair : recently_autofilled_forms_[i]) {
      fputs("  ", file);
      fputs(pair.first.c_str(), file);
      fputs(" = ", file);
      fputs(base::UTF16ToUTF8(pair.second).c_str(), file);
      fputs("\n", file);
    }
    if (i > 0)
      fputs("Next oldest form:\n", file);
  }
  fputs("\n", file);

  fclose(file);
}
#endif  // ENABLE_FORM_DEBUG_DUMP

}  // namespace autofill
