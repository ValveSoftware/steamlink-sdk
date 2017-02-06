// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_external_delegate.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/i18n/case_conversion.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/common/autofill_util.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

AutofillExternalDelegate::AutofillExternalDelegate(AutofillManager* manager,
                                                   AutofillDriver* driver)
    : manager_(manager),
      driver_(driver),
      query_id_(0),
      has_suggestion_(false),
      has_shown_popup_for_current_edit_(false),
      should_show_scan_credit_card_(false),
      has_shown_address_book_prompt(false),
      weak_ptr_factory_(this) {
  DCHECK(manager);
}

AutofillExternalDelegate::~AutofillExternalDelegate() {}

void AutofillExternalDelegate::OnQuery(int query_id,
                                       const FormData& form,
                                       const FormFieldData& field,
                                       const gfx::RectF& element_bounds) {
  if (!query_form_.SameFormAs(form))
    has_shown_address_book_prompt = false;

  query_form_ = form;
  query_field_ = field;
  query_id_ = query_id;
  element_bounds_ = element_bounds;
  should_show_scan_credit_card_ =
      manager_->ShouldShowScanCreditCard(query_form_, query_field_);
}

void AutofillExternalDelegate::OnSuggestionsReturned(
    int query_id,
    const std::vector<Suggestion>& input_suggestions) {
  if (query_id != query_id_)
    return;

  std::vector<Suggestion> suggestions(input_suggestions);

  // Add or hide warnings as appropriate.
  ApplyAutofillWarnings(&suggestions);

#if !defined(OS_ANDROID)
  // Add a separator to go between the values and menu items.
  suggestions.push_back(Suggestion());
  suggestions.back().frontend_id = POPUP_ITEM_ID_SEPARATOR;
#endif

  if (should_show_scan_credit_card_) {
    Suggestion scan_credit_card(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_SCAN_CREDIT_CARD));
    scan_credit_card.frontend_id = POPUP_ITEM_ID_SCAN_CREDIT_CARD;
    scan_credit_card.icon = base::ASCIIToUTF16("scanCreditCardIcon");
    suggestions.push_back(scan_credit_card);

    if (!has_shown_popup_for_current_edit_) {
      AutofillMetrics::LogScanCreditCardPromptMetric(
          AutofillMetrics::SCAN_CARD_ITEM_SHOWN);
    }
  }

  // Only include "Autofill Options" special menu item if we have Autofill
  // suggestions.
  has_suggestion_ = false;
  for (size_t i = 0; i < suggestions.size(); ++i) {
    if (suggestions[i].frontend_id > 0) {
      has_suggestion_ = true;
      break;
    }
  }

  if (has_suggestion_)
    ApplyAutofillOptions(&suggestions);

#if !defined(OS_ANDROID)
  // Remove the separator if it is the last element.
  DCHECK_GT(suggestions.size(), 0U);
  if (suggestions.back().frontend_id == POPUP_ITEM_ID_SEPARATOR)
    suggestions.pop_back();
#endif

  // If anything else is added to modify the values after inserting the data
  // list, AutofillPopupControllerImpl::UpdateDataListValues will need to be
  // updated to match.
  InsertDataListValues(&suggestions);

  if (suggestions.empty()) {
    // No suggestions, any popup currently showing is obsolete.
    manager_->client()->HideAutofillPopup();
    return;
  }

  // Send to display.
  if (query_field_.is_focusable) {
    manager_->client()->ShowAutofillPopup(element_bounds_,
                                          query_field_.text_direction,
                                          suggestions,
                                          GetWeakPtr());
  }
}

void AutofillExternalDelegate::SetCurrentDataListValues(
    const std::vector<base::string16>& data_list_values,
    const std::vector<base::string16>& data_list_labels) {
  data_list_values_ = data_list_values;
  data_list_labels_ = data_list_labels;

  manager_->client()->UpdateAutofillPopupDataListValues(data_list_values,
                                                        data_list_labels);
}

void AutofillExternalDelegate::OnPopupShown() {
  manager_->DidShowSuggestions(
      has_suggestion_ && !has_shown_popup_for_current_edit_,
      query_form_,
      query_field_);
  has_shown_popup_for_current_edit_ |= has_suggestion_;
}

void AutofillExternalDelegate::OnPopupHidden() {
  driver_->PopupHidden();
}

void AutofillExternalDelegate::DidSelectSuggestion(
    const base::string16& value,
    int identifier) {
  ClearPreviewedForm();

  // Only preview the data if it is a profile.
  if (identifier > 0)
    FillAutofillFormData(identifier, true);
  else if (identifier == POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY)
    driver_->RendererShouldPreviewFieldWithValue(value);
}

void AutofillExternalDelegate::DidAcceptSuggestion(const base::string16& value,
                                                   int identifier,
                                                   int position) {
  if (identifier == POPUP_ITEM_ID_AUTOFILL_OPTIONS) {
    // User selected 'Autofill Options'.
    manager_->ShowAutofillSettings();
  } else if (identifier == POPUP_ITEM_ID_CLEAR_FORM) {
    // User selected 'Clear form'.
    driver_->RendererShouldClearFilledForm();
  } else if (identifier == POPUP_ITEM_ID_PASSWORD_ENTRY) {
    NOTREACHED();  // Should be handled elsewhere.
  } else if (identifier == POPUP_ITEM_ID_DATALIST_ENTRY) {
    driver_->RendererShouldAcceptDataListSuggestion(value);
  } else if (identifier == POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY) {
    // User selected an Autocomplete, so we fill directly.
    driver_->RendererShouldFillFieldWithValue(value);
    AutofillMetrics::LogAutocompleteSuggestionAcceptedIndex(position);
  } else if (identifier == POPUP_ITEM_ID_SCAN_CREDIT_CARD) {
    manager_->client()->ScanCreditCard(base::Bind(
        &AutofillExternalDelegate::OnCreditCardScanned, GetWeakPtr()));
  } else {
    if (identifier > 0)  // Denotes an Autofill suggestion.
      AutofillMetrics::LogAutofillSuggestionAcceptedIndex(position);

    FillAutofillFormData(identifier, false);
  }

  if (should_show_scan_credit_card_) {
    AutofillMetrics::LogScanCreditCardPromptMetric(
        identifier == POPUP_ITEM_ID_SCAN_CREDIT_CARD
            ? AutofillMetrics::SCAN_CARD_ITEM_SELECTED
            : AutofillMetrics::SCAN_CARD_OTHER_ITEM_SELECTED);
  }

  manager_->client()->HideAutofillPopup();
}

bool AutofillExternalDelegate::GetDeletionConfirmationText(
    const base::string16& value,
    int identifier,
    base::string16* title,
    base::string16* body) {
  return manager_->GetDeletionConfirmationText(value, identifier, title, body);
}

bool AutofillExternalDelegate::RemoveSuggestion(const base::string16& value,
                                                int identifier) {
  if (identifier > 0)
    return manager_->RemoveAutofillProfileOrCreditCard(identifier);

  if (identifier == POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY) {
    manager_->RemoveAutocompleteEntry(query_field_.name, value);
    return true;
  }

  return false;
}

void AutofillExternalDelegate::DidEndTextFieldEditing() {
  manager_->client()->HideAutofillPopup();

  has_shown_popup_for_current_edit_ = false;
}

void AutofillExternalDelegate::ClearPreviewedForm() {
  driver_->RendererShouldClearPreviewedForm();
}

void AutofillExternalDelegate::Reset() {
  manager_->client()->HideAutofillPopup();
}

void AutofillExternalDelegate::OnPingAck() {
  // Reissue the most recent query, which will reopen the Autofill popup.
  manager_->OnQueryFormFieldAutofill(query_id_, query_form_, query_field_,
                                     element_bounds_);
}

base::WeakPtr<AutofillExternalDelegate> AutofillExternalDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AutofillExternalDelegate::OnCreditCardScanned(
    const base::string16& card_number,
    int expiration_month,
    int expiration_year) {
  manager_->FillCreditCardForm(
      query_id_, query_form_, query_field_,
      CreditCard(card_number, expiration_month, expiration_year),
      base::string16());
}

void AutofillExternalDelegate::FillAutofillFormData(int unique_id,
                                                    bool is_preview) {
  // If the selected element is a warning we don't want to do anything.
  if (unique_id == POPUP_ITEM_ID_WARNING_MESSAGE)
    return;

  AutofillDriver::RendererFormDataAction renderer_action = is_preview ?
      AutofillDriver::FORM_DATA_ACTION_PREVIEW :
      AutofillDriver::FORM_DATA_ACTION_FILL;

  DCHECK(driver_->RendererIsAvailable());
  // Fill the values for the whole form.
  manager_->FillOrPreviewForm(renderer_action,
                              query_id_,
                              query_form_,
                              query_field_,
                              unique_id);
}

void AutofillExternalDelegate::ApplyAutofillWarnings(
    std::vector<Suggestion>* suggestions) {
  if (suggestions->size() > 1 &&
      (*suggestions)[0].frontend_id == POPUP_ITEM_ID_WARNING_MESSAGE) {
    // If we received a warning instead of suggestions from Autofill but regular
    // suggestions from autocomplete, don't show the Autofill warning.
    suggestions->erase(suggestions->begin());
  }
}

void AutofillExternalDelegate::ApplyAutofillOptions(
    std::vector<Suggestion>* suggestions) {
  // The form has been auto-filled, so give the user the chance to clear the
  // form.  Append the 'Clear form' menu item.
  if (query_field_.is_autofilled) {
    base::string16 value =
        l10n_util::GetStringUTF16(IDS_AUTOFILL_CLEAR_FORM_MENU_ITEM);
    // TODO(rouslan): Remove manual upper-casing when keyboard accessory becomes
    // default on Android.
    if (IsKeyboardAccessoryEnabled())
      value = base::i18n::ToUpper(value);

    suggestions->push_back(Suggestion(value));
    suggestions->back().frontend_id = POPUP_ITEM_ID_CLEAR_FORM;
  }

  // Append the 'Chrome Autofill options' menu item;
  // TODO(rouslan): Switch on the platform in the GRD file when keyboard
  // accessory becomes default on Android.
  suggestions->push_back(Suggestion(l10n_util::GetStringUTF16(
      IsKeyboardAccessoryEnabled() ? IDS_AUTOFILL_OPTIONS_CONTENT_DESCRIPTION
                                   : IDS_AUTOFILL_OPTIONS_POPUP)));
  suggestions->back().frontend_id = POPUP_ITEM_ID_AUTOFILL_OPTIONS;
  if (IsKeyboardAccessoryEnabled())
    suggestions->back().icon = base::ASCIIToUTF16("settings");
}

void AutofillExternalDelegate::InsertDataListValues(
    std::vector<Suggestion>* suggestions) {
  if (data_list_values_.empty())
    return;

  // Go through the list of autocomplete values and remove them if they are in
  // the list of datalist values.
  std::set<base::string16> data_list_set(data_list_values_.begin(),
                                         data_list_values_.end());
  suggestions->erase(
      std::remove_if(suggestions->begin(), suggestions->end(),
                     [&data_list_set](const Suggestion& suggestion) {
                       return suggestion.frontend_id ==
                                  POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY &&
                              ContainsKey(data_list_set, suggestion.value);
                     }),
      suggestions->end());

#if !defined(OS_ANDROID)
  // Insert the separator between the datalist and Autofill/Autocomplete values
  // (if there are any).
  if (!suggestions->empty()) {
    suggestions->insert(suggestions->begin(), Suggestion());
    (*suggestions)[0].frontend_id = POPUP_ITEM_ID_SEPARATOR;
  }
#endif

  // Insert the datalist elements at the beginning.
  suggestions->insert(suggestions->begin(), data_list_values_.size(),
                      Suggestion());
  for (size_t i = 0; i < data_list_values_.size(); i++) {
    (*suggestions)[i].value = data_list_values_[i];
    (*suggestions)[i].label = data_list_labels_[i];
    (*suggestions)[i].frontend_id = POPUP_ITEM_ID_DATALIST_ENTRY;
  }
}

}  // namespace autofill
