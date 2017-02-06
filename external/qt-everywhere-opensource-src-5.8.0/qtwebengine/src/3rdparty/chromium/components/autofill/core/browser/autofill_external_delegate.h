// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXTERNAL_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXTERNAL_DELEGATE_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_popup_delegate.h"
#include "components/autofill/core/browser/suggestion.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "ui/gfx/geometry/rect_f.h"

namespace autofill {

class AutofillDriver;
class AutofillManager;

// TODO(csharp): A lot of the logic in this class is copied from autofillagent.
// Once Autofill is moved out of WebKit this class should be the only home for
// this logic. See http://crbug.com/51644

// Delegate for in-browser Autocomplete and Autofill display and selection.
class AutofillExternalDelegate : public AutofillPopupDelegate {
 public:
  // Creates an AutofillExternalDelegate for the specified AutofillManager and
  // AutofillDriver.
  AutofillExternalDelegate(AutofillManager* manager,
                           AutofillDriver* driver);
  virtual ~AutofillExternalDelegate();

  // AutofillPopupDelegate implementation.
  void OnPopupShown() override;
  void OnPopupHidden() override;
  void DidSelectSuggestion(const base::string16& value,
                           int identifier) override;
  void DidAcceptSuggestion(const base::string16& value,
                           int identifier,
                           int position) override;
  bool GetDeletionConfirmationText(const base::string16& value,
                                   int identifier,
                                   base::string16* title,
                                   base::string16* body) override;
  bool RemoveSuggestion(const base::string16& value, int identifier) override;
  void ClearPreviewedForm() override;

  // Records and associates a query_id with web form data.  Called
  // when the renderer posts an Autofill query to the browser. |bounds|
  // is window relative. We might not want to display the warning if a website
  // has disabled Autocomplete because they have their own popup, and showing
  // our popup on to of theirs would be a poor user experience.
  virtual void OnQuery(int query_id,
                       const FormData& form,
                       const FormFieldData& field,
                       const gfx::RectF& element_bounds);

  // Records query results and correctly formats them before sending them off
  // to be displayed.  Called when an Autofill query result is available.
  virtual void OnSuggestionsReturned(
      int query_id,
      const std::vector<Suggestion>& suggestions);

  // Set the data list value associated with the current field.
  void SetCurrentDataListValues(
      const std::vector<base::string16>& data_list_values,
      const std::vector<base::string16>& data_list_labels);

  // Inform the delegate that the text field editing has ended. This is
  // used to help record the metrics of when a new popup is shown.
  void DidEndTextFieldEditing();

  // Returns the delegate to its starting state by removing any page specific
  // values or settings.
  void Reset();

  // The renderer sent an IPC acknowledging an earlier ping IPC.
  void OnPingAck();

 protected:
  base::WeakPtr<AutofillExternalDelegate> GetWeakPtr();

 private:
  FRIEND_TEST_ALL_PREFIXES(AutofillExternalDelegateUnitTest,
                           FillCreditCardForm);

  // Called when a credit card is scanned using device camera.
  void OnCreditCardScanned(const base::string16& card_number,
                           int expiration_month,
                           int expiration_year);

  // Fills the form with the Autofill data corresponding to |unique_id|.
  // If |is_preview| is true then this is just a preview to show the user what
  // would be selected and if |is_preview| is false then the user has selected
  // this data.
  void FillAutofillFormData(int unique_id, bool is_preview);

  // Handle applying any Autofill warnings to the Autofill popup.
  void ApplyAutofillWarnings(std::vector<Suggestion>* suggestions);

  // Handle applying any Autofill option listings to the Autofill popup.
  // This function should only get called when there is at least one
  // multi-field suggestion in the list of suggestions.
  void ApplyAutofillOptions(std::vector<Suggestion>* suggestions);

  // Insert the data list values at the start of the given list, including
  // any required separators. Will also go through |suggestions| and remove
  // duplicate autocomplete (not Autofill) suggestions, keeping their datalist
  // version.
  void InsertDataListValues(std::vector<Suggestion>* suggestions);

  AutofillManager* manager_;  // weak.

  // Provides driver-level context to the shared code of the component. Must
  // outlive this object.
  AutofillDriver* driver_;  // weak

  // The ID of the last request sent for form field Autofill.  Used to ignore
  // out of date responses.
  int query_id_;

  // The current form and field selected by Autofill.
  FormData query_form_;
  FormFieldData query_field_;

  // The bounds of the form field that user is interacting with.
  gfx::RectF element_bounds_;

  // Does the popup include any Autofill profile or credit card suggestions?
  bool has_suggestion_;

  // Have we already shown Autofill suggestions for the field the user is
  // currently editing?  Used to keep track of state for metrics logging.
  bool has_shown_popup_for_current_edit_;

  // FIXME
  bool should_show_scan_credit_card_;

  // Whether the access Address Book prompt has ever been shown for the current
  // |query_form_|. This variable is only used on OSX.
  bool has_shown_address_book_prompt;

  // The current data list values.
  std::vector<base::string16> data_list_values_;
  std::vector<base::string16> data_list_labels_;

  base::WeakPtrFactory<AutofillExternalDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutofillExternalDelegate);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_EXTERNAL_DELEGATE_H_
