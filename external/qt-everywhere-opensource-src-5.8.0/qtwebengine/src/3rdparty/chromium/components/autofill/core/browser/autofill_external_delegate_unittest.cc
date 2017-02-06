// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/browser/suggestion_test_helpers.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_autofill_external_delegate.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

using base::ASCIIToUTF16;
using testing::_;

namespace autofill {

namespace {

// A constant value to use as the Autofill query ID.
const int kQueryId = 5;

// A constant value to use as an Autofill profile ID.
const int kAutofillProfileId = 1;

class MockAutofillDriver : public TestAutofillDriver {
 public:
  MockAutofillDriver() {}
  // Mock methods to enable testability.
  MOCK_METHOD1(RendererShouldAcceptDataListSuggestion,
               void(const base::string16&));
  MOCK_METHOD0(RendererShouldClearFilledForm, void());
  MOCK_METHOD0(RendererShouldClearPreviewedForm, void());
  MOCK_METHOD1(RendererShouldFillFieldWithValue, void(const base::string16&));
  MOCK_METHOD1(RendererShouldPreviewFieldWithValue,
               void(const base::string16&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillDriver);
};

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() {}

  MOCK_METHOD1(ScanCreditCard,
               void(const CreditCardScanCallback& callbacK));

  MOCK_METHOD4(ShowAutofillPopup,
               void(const gfx::RectF& element_bounds,
                    base::i18n::TextDirection text_direction,
                    const std::vector<Suggestion>& suggestions,
                    base::WeakPtr<AutofillPopupDelegate> delegate));

  MOCK_METHOD2(UpdateAutofillPopupDataListValues,
               void(const std::vector<base::string16>& values,
                    const std::vector<base::string16>& lables));

  MOCK_METHOD0(HideAutofillPopup, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillClient);
};

class MockAutofillManager : public AutofillManager {
 public:
  MockAutofillManager(AutofillDriver* driver, MockAutofillClient* client)
      // Force to use the constructor designated for unit test, but we don't
      // really need personal_data in this test so we pass a NULL pointer.
      : AutofillManager(driver, client, NULL) {}
  virtual ~MockAutofillManager() {}

  MOCK_METHOD2(ShouldShowScanCreditCard,
               bool(const FormData& form, const FormFieldData& field));

  MOCK_METHOD5(FillOrPreviewForm,
               void(AutofillDriver::RendererFormDataAction action,
                    int query_id,
                    const FormData& form,
                    const FormFieldData& field,
                    int unique_id));

  MOCK_METHOD5(FillCreditCardForm,
               void(int query_id,
                    const FormData& form,
                    const FormFieldData& field,
                    const CreditCard& credit_card,
                    const base::string16& cvc));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillManager);
};

}  // namespace

class AutofillExternalDelegateUnitTest : public testing::Test {
 protected:
  void SetUp() override {
    autofill_driver_.reset(new testing::NiceMock<MockAutofillDriver>());
    autofill_manager_.reset(
        new MockAutofillManager(autofill_driver_.get(), &autofill_client_));
    external_delegate_.reset(
        new AutofillExternalDelegate(
            autofill_manager_.get(), autofill_driver_.get()));
  }

  void TearDown() override {
    // Order of destruction is important as AutofillManager relies on
    // PersonalDataManager to be around when it gets destroyed.
    autofill_manager_.reset();
    external_delegate_.reset();
    autofill_driver_.reset();
  }

  // Issue an OnQuery call with the given |query_id|.
  void IssueOnQuery(int query_id) {
    const FormData form;
    FormFieldData field;
    field.is_focusable = true;
    field.should_autocomplete = true;

    external_delegate_->OnQuery(query_id, form, field, gfx::RectF());
  }

  void IssueOnSuggestionsReturned() {
    std::vector<Suggestion> suggestions;
    suggestions.push_back(Suggestion());
    suggestions[0].frontend_id = kAutofillProfileId;
    external_delegate_->OnSuggestionsReturned(kQueryId, suggestions);
  }

  testing::NiceMock<MockAutofillClient> autofill_client_;
  std::unique_ptr<testing::NiceMock<MockAutofillDriver>> autofill_driver_;
  std::unique_ptr<MockAutofillManager> autofill_manager_;
  std::unique_ptr<AutofillExternalDelegate> external_delegate_;

  base::MessageLoop message_loop_;
};

// Test that our external delegate called the virtual methods at the right time.
TEST_F(AutofillExternalDelegateUnitTest, TestExternalDelegateVirtualCalls) {
  IssueOnQuery(kQueryId);

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  auto element_ids = testing::ElementsAre(
      kAutofillProfileId,
#if !defined(OS_ANDROID)
      static_cast<int>(POPUP_ITEM_ID_SEPARATOR),
#endif
      static_cast<int>(POPUP_ITEM_ID_AUTOFILL_OPTIONS));
  EXPECT_CALL(
      autofill_client_,
      ShowAutofillPopup(_, _, SuggestionVectorIdsAre(element_ids), _));

  // This should call ShowAutofillPopup.
  std::vector<Suggestion> autofill_item;
  autofill_item.push_back(Suggestion());
  autofill_item[0].frontend_id = kAutofillProfileId;
  external_delegate_->OnSuggestionsReturned(kQueryId, autofill_item);

  EXPECT_CALL(*autofill_manager_,
              FillOrPreviewForm(
                  AutofillDriver::FORM_DATA_ACTION_FILL, _, _, _, _));
  EXPECT_CALL(autofill_client_, HideAutofillPopup());

  // This should trigger a call to hide the popup since we've selected an
  // option.
  external_delegate_->DidAcceptSuggestion(autofill_item[0].value,
                                          autofill_item[0].frontend_id,
                                          0);
}

// Test that data list elements for a node will appear in the Autofill popup.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateDataList) {
  IssueOnQuery(kQueryId);

  std::vector<base::string16> data_list_items;
  data_list_items.push_back(base::string16());

  EXPECT_CALL(
      autofill_client_,
      UpdateAutofillPopupDataListValues(data_list_items, data_list_items));

  external_delegate_->SetCurrentDataListValues(data_list_items,
                                               data_list_items);

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  auto element_ids = testing::ElementsAre(
      static_cast<int>(POPUP_ITEM_ID_DATALIST_ENTRY),
#if !defined(OS_ANDROID)
      static_cast<int>(POPUP_ITEM_ID_SEPARATOR),
#endif
      kAutofillProfileId,
#if !defined(OS_ANDROID)
      static_cast<int>(POPUP_ITEM_ID_SEPARATOR),
#endif
      static_cast<int>(POPUP_ITEM_ID_AUTOFILL_OPTIONS));
  EXPECT_CALL(
      autofill_client_,
      ShowAutofillPopup(_, _, SuggestionVectorIdsAre(element_ids), _));

  // This should call ShowAutofillPopup.
  std::vector<Suggestion> autofill_item;
  autofill_item.push_back(Suggestion());
  autofill_item[0].frontend_id = kAutofillProfileId;
  external_delegate_->OnSuggestionsReturned(kQueryId, autofill_item);

  // Try calling OnSuggestionsReturned with no Autofill values and ensure
  // the datalist items are still shown.
  // The enum must be cast to an int to prevent compile errors on linux_rel.
  EXPECT_CALL(
      autofill_client_,
      ShowAutofillPopup(
          _,
          _,
          SuggestionVectorIdsAre(testing::ElementsAre(
              static_cast<int>(POPUP_ITEM_ID_DATALIST_ENTRY))),
          _));

  autofill_item.clear();
  external_delegate_->OnSuggestionsReturned(kQueryId, autofill_item);
}

// Test that datalist values can get updated while a popup is showing.
TEST_F(AutofillExternalDelegateUnitTest, UpdateDataListWhileShowingPopup) {
  IssueOnQuery(kQueryId);

  EXPECT_CALL(autofill_client_, ShowAutofillPopup(_, _, _, _))
      .Times(0);

  // Make sure just setting the data list values doesn't cause the popup to
  // appear.
  std::vector<base::string16> data_list_items;
  data_list_items.push_back(base::string16());

  EXPECT_CALL(
      autofill_client_,
      UpdateAutofillPopupDataListValues(data_list_items, data_list_items));

  external_delegate_->SetCurrentDataListValues(data_list_items,
                                               data_list_items);

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  auto element_ids = testing::ElementsAre(
      static_cast<int>(POPUP_ITEM_ID_DATALIST_ENTRY),
#if !defined(OS_ANDROID)
      static_cast<int>(POPUP_ITEM_ID_SEPARATOR),
#endif
      kAutofillProfileId,
#if !defined(OS_ANDROID)
      static_cast<int>(POPUP_ITEM_ID_SEPARATOR),
#endif
      static_cast<int>(POPUP_ITEM_ID_AUTOFILL_OPTIONS));
  EXPECT_CALL(
      autofill_client_,
      ShowAutofillPopup(_, _, SuggestionVectorIdsAre(element_ids), _));

  // Ensure the popup is displayed.
  std::vector<Suggestion> autofill_item;
  autofill_item.push_back(Suggestion());
  autofill_item[0].frontend_id = kAutofillProfileId;
  external_delegate_->OnSuggestionsReturned(kQueryId, autofill_item);

  // This would normally get called from ShowAutofillPopup, but it is mocked so
  // we need to call OnPopupShown ourselves.
  external_delegate_->OnPopupShown();

  // Update the current data list and ensure the popup is updated.
  data_list_items.push_back(base::string16());

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  EXPECT_CALL(
      autofill_client_,
      UpdateAutofillPopupDataListValues(data_list_items, data_list_items));

  external_delegate_->SetCurrentDataListValues(data_list_items,
                                               data_list_items);
}

// Test that we _don't_ de-dupe autofill values against datalist values. We
// keep both with a separator.
TEST_F(AutofillExternalDelegateUnitTest, DuplicateAutofillDatalistValues) {
  IssueOnQuery(kQueryId);

  std::vector<base::string16> data_list_items;
  data_list_items.push_back(base::ASCIIToUTF16("Rick"));
  data_list_items.push_back(base::ASCIIToUTF16("Deckard"));

  EXPECT_CALL(autofill_client_, UpdateAutofillPopupDataListValues(
                                    data_list_items, data_list_items));

  external_delegate_->SetCurrentDataListValues(data_list_items,
                                               data_list_items);

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  auto element_ids =
      testing::ElementsAre(static_cast<int>(POPUP_ITEM_ID_DATALIST_ENTRY),
                           static_cast<int>(POPUP_ITEM_ID_DATALIST_ENTRY),
#if !defined(OS_ANDROID)
                           static_cast<int>(POPUP_ITEM_ID_SEPARATOR),
#endif
                           kAutofillProfileId,
#if !defined(OS_ANDROID)
                           static_cast<int>(POPUP_ITEM_ID_SEPARATOR),
#endif
                           static_cast<int>(POPUP_ITEM_ID_AUTOFILL_OPTIONS));
  EXPECT_CALL(autofill_client_,
              ShowAutofillPopup(_, _, SuggestionVectorIdsAre(element_ids), _));

  // Have an Autofill item that is identical to one of the datalist entries.
  std::vector<Suggestion> autofill_item;
  autofill_item.push_back(Suggestion());
  autofill_item[0].value = ASCIIToUTF16("Rick");
  autofill_item[0].frontend_id = kAutofillProfileId;
  external_delegate_->OnSuggestionsReturned(kQueryId, autofill_item);
}

// Test that we de-dupe autocomplete values against datalist values, keeping the
// latter in case of a match.
TEST_F(AutofillExternalDelegateUnitTest, DuplicateAutocompleteDatalistValues) {
  IssueOnQuery(kQueryId);

  std::vector<base::string16> data_list_items;
  data_list_items.push_back(base::ASCIIToUTF16("Rick"));
  data_list_items.push_back(base::ASCIIToUTF16("Deckard"));

  EXPECT_CALL(autofill_client_, UpdateAutofillPopupDataListValues(
                                    data_list_items, data_list_items));

  external_delegate_->SetCurrentDataListValues(data_list_items,
                                               data_list_items);

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  auto element_ids = testing::ElementsAre(
      // We are expecting only two data list entries.
      static_cast<int>(POPUP_ITEM_ID_DATALIST_ENTRY),
      static_cast<int>(POPUP_ITEM_ID_DATALIST_ENTRY),
#if !defined(OS_ANDROID)
      static_cast<int>(POPUP_ITEM_ID_SEPARATOR),
#endif
      static_cast<int>(POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY));
  EXPECT_CALL(autofill_client_,
              ShowAutofillPopup(_, _, SuggestionVectorIdsAre(element_ids), _));

  // Have an Autocomplete item that is identical to one of the datalist entries
  // and one that is distinct.
  std::vector<Suggestion> autocomplete_items;
  autocomplete_items.push_back(Suggestion());
  autocomplete_items[0].value = ASCIIToUTF16("Rick");
  autocomplete_items[0].frontend_id = POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY;
  autocomplete_items.push_back(Suggestion());
  autocomplete_items[1].value = ASCIIToUTF16("Cain");
  autocomplete_items[1].frontend_id = POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY;
  external_delegate_->OnSuggestionsReturned(kQueryId, autocomplete_items);
}

// Test that the Autofill popup is able to display warnings explaining why
// Autofill is disabled for a website.
// Regression test for http://crbug.com/247880
TEST_F(AutofillExternalDelegateUnitTest, AutofillWarnings) {
  IssueOnQuery(kQueryId);

  // The enums must be cast to ints to prevent compile errors on linux_rel.
  EXPECT_CALL(
      autofill_client_,
      ShowAutofillPopup(
          _,
          _,
          SuggestionVectorIdsAre(testing::ElementsAre(
              static_cast<int>(POPUP_ITEM_ID_WARNING_MESSAGE))),
          _));

  // This should call ShowAutofillPopup.
  std::vector<Suggestion> autofill_item;
  autofill_item.push_back(Suggestion());
  autofill_item[0].frontend_id = POPUP_ITEM_ID_WARNING_MESSAGE;
  external_delegate_->OnSuggestionsReturned(kQueryId, autofill_item);
}

// Test that the Autofill delegate doesn't try and fill a form with a
// negative unique id.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateInvalidUniqueId) {
  // Ensure it doesn't try to preview the negative id.
  EXPECT_CALL(*autofill_manager_, FillOrPreviewForm(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm()).Times(1);
  external_delegate_->DidSelectSuggestion(base::string16(), -1);

  // Ensure it doesn't try to fill the form in with the negative id.
  EXPECT_CALL(autofill_client_, HideAutofillPopup());
  EXPECT_CALL(*autofill_manager_, FillOrPreviewForm(_, _, _, _, _)).Times(0);
  external_delegate_->DidAcceptSuggestion(base::string16(), -1, 0);
}

// Test that the ClearPreview call is only sent if the form was being previewed
// (i.e. it isn't autofilling a password).
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateClearPreviewedForm) {
  // Ensure selecting a new password entries or Autofill entries will
  // cause any previews to get cleared.
  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm()).Times(1);
  external_delegate_->DidSelectSuggestion(ASCIIToUTF16("baz foo"),
                                          POPUP_ITEM_ID_PASSWORD_ENTRY);
  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm()).Times(1);
  EXPECT_CALL(*autofill_manager_,
              FillOrPreviewForm(
                  AutofillDriver::FORM_DATA_ACTION_PREVIEW, _, _, _, _));
  external_delegate_->DidSelectSuggestion(ASCIIToUTF16("baz foo"), 1);

  // Ensure selecting an autocomplete entry will cause any previews to
  // get cleared.
  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm()).Times(1);
  EXPECT_CALL(*autofill_driver_, RendererShouldPreviewFieldWithValue(
                                     ASCIIToUTF16("baz foo")));
  external_delegate_->DidSelectSuggestion(ASCIIToUTF16("baz foo"),
                                          POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY);
}

// Test that the popup is hidden once we are done editing the autofill field.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateHidePopupAfterEditing) {
  EXPECT_CALL(autofill_client_, ShowAutofillPopup(_, _, _, _));
  GenerateTestAutofillPopup(external_delegate_.get());

  EXPECT_CALL(autofill_client_, HideAutofillPopup());
  external_delegate_->DidEndTextFieldEditing();
}

// Test that the driver is directed to accept the data list after being notified
// that the user accepted the data list suggestion.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateAcceptDatalistSuggestion) {
  EXPECT_CALL(autofill_client_, HideAutofillPopup());
  base::string16 dummy_string(ASCIIToUTF16("baz qux"));
  EXPECT_CALL(*autofill_driver_,
              RendererShouldAcceptDataListSuggestion(dummy_string));
  external_delegate_->DidAcceptSuggestion(dummy_string,
                                          POPUP_ITEM_ID_DATALIST_ENTRY,
                                          0);
}

// Test that an accepted autofill suggestion will fill the form and log the
// proper metric.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateAcceptAutofillSuggestion) {
  EXPECT_CALL(autofill_client_, HideAutofillPopup());
  base::string16 dummy_string(ASCIIToUTF16("John Legend"));
  EXPECT_CALL(*autofill_manager_,
              FillOrPreviewForm(
                  AutofillDriver::FORM_DATA_ACTION_FILL, _, _, _,
                  kAutofillProfileId));
  base::HistogramTester histogram;
  external_delegate_->DidAcceptSuggestion(dummy_string,
                                          kAutofillProfileId,
                                          2);  // Row 2
  histogram.ExpectUniqueSample("Autofill.SuggestionAcceptedIndex", 2, 1);
}

// Test that the driver is directed to clear the form after being notified that
// the user accepted the suggestion to clear the form.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateClearForm) {
  EXPECT_CALL(autofill_client_, HideAutofillPopup());
  EXPECT_CALL(*autofill_driver_, RendererShouldClearFilledForm());

  external_delegate_->DidAcceptSuggestion(base::string16(),
                                          POPUP_ITEM_ID_CLEAR_FORM,
                                          0);
}

// Test that autofill client will scan a credit card after use accepted the
// suggestion to scan a credit card.
TEST_F(AutofillExternalDelegateUnitTest, ScanCreditCardMenuItem) {
  EXPECT_CALL(autofill_client_, ScanCreditCard(_));
  EXPECT_CALL(autofill_client_, HideAutofillPopup());
  external_delegate_->DidAcceptSuggestion(base::string16(),
                                          POPUP_ITEM_ID_SCAN_CREDIT_CARD,
                                          0);
}

TEST_F(AutofillExternalDelegateUnitTest, ScanCreditCardPromptMetricsTest) {
  // Log that the scan card item was shown, although nothing was selected.
  {
    EXPECT_CALL(*autofill_manager_, ShouldShowScanCreditCard(_, _))
        .WillOnce(testing::Return(true));
    base::HistogramTester histogram;
    IssueOnQuery(kQueryId);
    IssueOnSuggestionsReturned();
    external_delegate_->OnPopupHidden();
    histogram.ExpectUniqueSample("Autofill.ScanCreditCardPrompt",
                                 AutofillMetrics::SCAN_CARD_ITEM_SHOWN, 1);
  }
  // Log that the scan card item was selected.
  {
    EXPECT_CALL(*autofill_manager_, ShouldShowScanCreditCard(_, _))
        .WillOnce(testing::Return(true));
    base::HistogramTester histogram;
    IssueOnQuery(kQueryId);
    IssueOnSuggestionsReturned();
    external_delegate_->DidAcceptSuggestion(base::string16(),
                                            POPUP_ITEM_ID_SCAN_CREDIT_CARD,
                                            0);
    histogram.ExpectBucketCount("Autofill.ScanCreditCardPrompt",
                                AutofillMetrics::SCAN_CARD_ITEM_SHOWN, 1);
    histogram.ExpectBucketCount("Autofill.ScanCreditCardPrompt",
                                AutofillMetrics::SCAN_CARD_ITEM_SELECTED, 1);
    histogram.ExpectBucketCount("Autofill.ScanCreditCardPrompt",
                                AutofillMetrics::SCAN_CARD_OTHER_ITEM_SELECTED,
                                0);
  }
  // Log that something else was selected.
  {
    EXPECT_CALL(*autofill_manager_, ShouldShowScanCreditCard(_, _))
        .WillOnce(testing::Return(true));
    base::HistogramTester histogram;
    IssueOnQuery(kQueryId);
    IssueOnSuggestionsReturned();
    external_delegate_->DidAcceptSuggestion(base::string16(),
                                            POPUP_ITEM_ID_CLEAR_FORM,
                                            0);
    histogram.ExpectBucketCount("Autofill.ScanCreditCardPrompt",
                                AutofillMetrics::SCAN_CARD_ITEM_SHOWN, 1);
    histogram.ExpectBucketCount("Autofill.ScanCreditCardPrompt",
                                AutofillMetrics::SCAN_CARD_ITEM_SELECTED, 0);
    histogram.ExpectBucketCount("Autofill.ScanCreditCardPrompt",
                                AutofillMetrics::SCAN_CARD_OTHER_ITEM_SELECTED,
                                1);
  }
  // Nothing is logged when the item isn't shown.
  {
    EXPECT_CALL(*autofill_manager_, ShouldShowScanCreditCard(_, _))
        .WillOnce(testing::Return(false));
    base::HistogramTester histogram;
    IssueOnQuery(kQueryId);
    IssueOnSuggestionsReturned();
    external_delegate_->DidAcceptSuggestion(base::string16(),
                                            POPUP_ITEM_ID_CLEAR_FORM,
                                            0);
    histogram.ExpectTotalCount("Autofill.ScanCreditCardPrompt", 0);
  }
}

MATCHER_P3(CreditCardMatches,
           card_number,
           expiration_month,
           expiration_year,
           "") {
  return !arg.Compare(
      CreditCard(card_number, expiration_month, expiration_year));
}

// Test that autofill manager will fill the credit card form after user scans a
// credit card.
TEST_F(AutofillExternalDelegateUnitTest, FillCreditCardForm) {
  base::string16 card_number = base::ASCIIToUTF16("test");
  int expiration_month = 1;
  int expiration_year = 3000;
  EXPECT_CALL(*autofill_manager_,
              FillCreditCardForm(
                  _, _, _, CreditCardMatches(card_number, expiration_month,
                                             expiration_year),
                  base::string16()));
  external_delegate_->OnCreditCardScanned(card_number, expiration_month,
                                          expiration_year);
}

TEST_F(AutofillExternalDelegateUnitTest, IgnoreAutocompleteOffForAutofill) {
  const FormData form;
  FormFieldData field;
  field.is_focusable = true;
  field.should_autocomplete = false;

  external_delegate_->OnQuery(kQueryId, form, field, gfx::RectF());

  std::vector<Suggestion> autofill_items;
  autofill_items.push_back(Suggestion());
  autofill_items[0].frontend_id = POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY;

  // Ensure the popup tries to show itself, despite autocomplete="off".
  EXPECT_CALL(autofill_client_, ShowAutofillPopup(_, _, _, _));
  EXPECT_CALL(autofill_client_, HideAutofillPopup()).Times(0);

  external_delegate_->OnSuggestionsReturned(kQueryId, autofill_items);
}

TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateFillFieldWithValue) {
  EXPECT_CALL(autofill_client_, HideAutofillPopup());
  base::string16 dummy_string(ASCIIToUTF16("baz foo"));
  EXPECT_CALL(*autofill_driver_,
              RendererShouldFillFieldWithValue(dummy_string));
  base::HistogramTester histogram_tester;
  external_delegate_->DidAcceptSuggestion(dummy_string,
                                          POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY,
                                          0);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SuggestionAcceptedIndex.Autocomplete", 0, 1);
}

}  // namespace autofill
