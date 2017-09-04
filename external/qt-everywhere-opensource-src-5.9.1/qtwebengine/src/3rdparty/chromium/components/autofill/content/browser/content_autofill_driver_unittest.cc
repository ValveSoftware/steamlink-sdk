// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_driver.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

const char kAppLocale[] = "en-US";
const AutofillManager::AutofillDownloadManagerState kDownloadState =
    AutofillManager::DISABLE_AUTOFILL_DOWNLOAD_MANAGER;

class FakeAutofillAgent : public mojom::AutofillAgent {
 public:
  FakeAutofillAgent()
      : fill_form_id_(-1),
        preview_form_id_(-1),
        called_clear_form_(false),
        called_clear_previewed_form_(false) {}

  ~FakeAutofillAgent() override {}

  void BindRequest(mojo::ScopedMessagePipeHandle handle) {
    bindings_.AddBinding(
        this, mojo::MakeRequest<mojom::AutofillAgent>(std::move(handle)));
  }

  void SetQuitLoopClosure(base::Closure closure) { quit_closure_ = closure; }

  // Returns the id and formdata received via
  // mojo interface method mojom::AutofillAgent::FillForm().
  bool GetAutofillFillFormMessage(int* page_id, FormData* results) {
    if (fill_form_id_ == -1)
      return false;
    if (!fill_form_form_)
      return false;

    if (page_id)
      *page_id = fill_form_id_;
    if (results)
      *results = *fill_form_form_;
    return true;
  }

  // Returns the id and formdata received via
  // mojo interface method mojom::AutofillAgent::PreviewForm().
  bool GetAutofillPreviewFormMessage(int* page_id, FormData* results) {
    if (preview_form_id_ == -1)
      return false;
    if (!preview_form_form_)
      return false;

    if (page_id)
      *page_id = preview_form_id_;
    if (results)
      *results = *preview_form_form_;
    return true;
  }

  // Returns data received via mojo interface method
  // mojom::AutofillAent::FieldTypePredictionsAvailable().
  bool GetFieldTypePredictionsAvailable(
      std::vector<FormDataPredictions>* predictions) {
    if (!predictions_)
      return false;
    if (predictions)
      *predictions = *predictions_;
    return true;
  }

  // Returns whether mojo interface method mojom::AutofillAgent::ClearForm() got
  // called.
  bool GetCalledClearForm() { return called_clear_form_; }

  // Returns whether mojo interface method
  // mojom::AutofillAgent::ClearPreviewedForm() got called.
  bool GetCalledClearPreviewedForm() { return called_clear_previewed_form_; }

  // Returns data received via mojo interface method
  // mojom::AutofillAent::FillFieldWithValue().
  bool GetString16FillFieldWithValue(base::string16* value) {
    if (!value_fill_field_)
      return false;
    if (value)
      *value = *value_fill_field_;
    return true;
  }

  // Returns data received via mojo interface method
  // mojom::AutofillAent::PreviewFieldWithValue().
  bool GetString16PreviewFieldWithValue(base::string16* value) {
    if (!value_preview_field_)
      return false;
    if (value)
      *value = *value_preview_field_;
    return true;
  }

  // Returns data received via mojo interface method
  // mojom::AutofillAent::AcceptDataListSuggestion().
  bool GetString16AcceptDataListSuggestion(base::string16* value) {
    if (!value_accept_data_)
      return false;
    if (value)
      *value = *value_accept_data_;
    return true;
  }

 private:
  void CallDone() {
    if (!quit_closure_.is_null()) {
      quit_closure_.Run();
      quit_closure_.Reset();
    }
  }

  // mojom::AutofillAgent methods:
  void FirstUserGestureObservedInTab() override {}

  void FillForm(int32_t id, const FormData& form) override {
    fill_form_id_ = id;
    fill_form_form_ = form;
    CallDone();
  }

  void PreviewForm(int32_t id, const FormData& form) override {
    preview_form_id_ = id;
    preview_form_form_ = form;
    CallDone();
  }

  void FieldTypePredictionsAvailable(
      const std::vector<FormDataPredictions>& forms) override {
    predictions_ = forms;
    CallDone();
  }

  void ClearForm() override {
    called_clear_form_ = true;
    CallDone();
  }

  void ClearPreviewedForm() override {
    called_clear_previewed_form_ = true;
    CallDone();
  }

  void FillFieldWithValue(const base::string16& value) override {
    value_fill_field_ = value;
    CallDone();
  }

  void PreviewFieldWithValue(const base::string16& value) override {
    value_preview_field_ = value;
    CallDone();
  }

  void AcceptDataListSuggestion(const base::string16& value) override {
    value_accept_data_ = value;
    CallDone();
  }

  void FillPasswordSuggestion(const base::string16& username,
                              const base::string16& password) override {}

  void PreviewPasswordSuggestion(const base::string16& username,
                                 const base::string16& password) override {}

  void ShowInitialPasswordAccountSuggestions(
      int32_t key,
      const PasswordFormFillData& form_data) override {}

  mojo::BindingSet<mojom::AutofillAgent> bindings_;

  base::Closure quit_closure_;

  // Records data received from FillForm() call.
  int32_t fill_form_id_;
  base::Optional<FormData> fill_form_form_;
  // Records data received from PreviewForm() call.
  int32_t preview_form_id_;
  base::Optional<FormData> preview_form_form_;
  // Records data received from FieldTypePredictionsAvailable() call.
  base::Optional<std::vector<FormDataPredictions>> predictions_;
  // Records whether ClearForm() got called.
  bool called_clear_form_;
  // Records whether ClearPreviewedForm() got called.
  bool called_clear_previewed_form_;
  // Records string received from FillFieldWithValue() call.
  base::Optional<base::string16> value_fill_field_;
  // Records string received from PreviewFieldWithValue() call.
  base::Optional<base::string16> value_preview_field_;
  // Records string received from AcceptDataListSuggestion() call.
  base::Optional<base::string16> value_accept_data_;
};

}  // namespace

class MockAutofillManager : public AutofillManager {
 public:
  MockAutofillManager(AutofillDriver* driver, AutofillClient* client)
      : AutofillManager(driver, client, kAppLocale, kDownloadState) {}
  virtual ~MockAutofillManager() {}

  MOCK_METHOD0(Reset, void());
};

class TestContentAutofillDriver : public ContentAutofillDriver {
 public:
  TestContentAutofillDriver(content::RenderFrameHost* rfh,
                            AutofillClient* client)
      : ContentAutofillDriver(rfh, client, kAppLocale, kDownloadState) {
    std::unique_ptr<AutofillManager> autofill_manager(
        new MockAutofillManager(this, client));
    SetAutofillManager(std::move(autofill_manager));
  }
  ~TestContentAutofillDriver() override {}

  virtual MockAutofillManager* mock_autofill_manager() {
    return static_cast<MockAutofillManager*>(autofill_manager());
  }

  using ContentAutofillDriver::DidNavigateFrame;
};

class ContentAutofillDriverTest : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    test_autofill_client_.reset(new TestAutofillClient());
    driver_.reset(new TestContentAutofillDriver(web_contents()->GetMainFrame(),
                                                test_autofill_client_.get()));

    service_manager::InterfaceProvider* remote_interfaces =
        web_contents()->GetMainFrame()->GetRemoteInterfaces();
    service_manager::InterfaceProvider::TestApi test_api(remote_interfaces);
    test_api.SetBinderForName(mojom::AutofillAgent::Name_,
                              base::Bind(&FakeAutofillAgent::BindRequest,
                                         base::Unretained(&fake_agent_)));
  }

  void TearDown() override {
    // Reset the driver now to cause all pref observers to be removed and avoid
    // crashes that otherwise occur in the destructor.
    driver_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<TestAutofillClient> test_autofill_client_;
  std::unique_ptr<TestContentAutofillDriver> driver_;

  FakeAutofillAgent fake_agent_;
};

TEST_F(ContentAutofillDriverTest, GetURLRequestContext) {
  net::URLRequestContextGetter* request_context =
      driver_->GetURLRequestContext();
  net::URLRequestContextGetter* expected_request_context =
      content::BrowserContext::GetDefaultStoragePartition(
          web_contents()->GetBrowserContext())->GetURLRequestContext();
  EXPECT_EQ(request_context, expected_request_context);
}

TEST_F(ContentAutofillDriverTest, NavigatedToDifferentPage) {
  EXPECT_CALL(*driver_->mock_autofill_manager(), Reset());
  content::LoadCommittedDetails details = content::LoadCommittedDetails();
  details.is_main_frame = true;
  details.is_in_page = false;
  ASSERT_TRUE(details.is_navigation_to_different_page());
  content::FrameNavigateParams params = content::FrameNavigateParams();
  driver_->DidNavigateFrame(details, params);
}

TEST_F(ContentAutofillDriverTest, NavigatedWithinSamePage) {
  EXPECT_CALL(*driver_->mock_autofill_manager(), Reset()).Times(0);
  content::LoadCommittedDetails details = content::LoadCommittedDetails();
  details.is_main_frame = false;
  ASSERT_TRUE(!details.is_navigation_to_different_page());
  content::FrameNavigateParams params = content::FrameNavigateParams();
  driver_->DidNavigateFrame(details, params);
}

TEST_F(ContentAutofillDriverTest, FormDataSentToRenderer_FillForm) {
  int input_page_id = 42;
  FormData input_form_data;
  test::CreateTestAddressFormData(&input_form_data);
  base::RunLoop run_loop;
  fake_agent_.SetQuitLoopClosure(run_loop.QuitClosure());
  driver_->SendFormDataToRenderer(
      input_page_id, AutofillDriver::FORM_DATA_ACTION_FILL, input_form_data);

  run_loop.RunUntilIdle();

  int output_page_id = 0;
  FormData output_form_data;
  EXPECT_FALSE(fake_agent_.GetAutofillPreviewFormMessage(&output_page_id,
                                                         &output_form_data));
  EXPECT_TRUE(fake_agent_.GetAutofillFillFormMessage(&output_page_id,
                                                     &output_form_data));
  EXPECT_EQ(input_page_id, output_page_id);
  EXPECT_TRUE(input_form_data.SameFormAs(output_form_data));
}

TEST_F(ContentAutofillDriverTest, FormDataSentToRenderer_PreviewForm) {
  int input_page_id = 42;
  FormData input_form_data;
  test::CreateTestAddressFormData(&input_form_data);
  base::RunLoop run_loop;
  fake_agent_.SetQuitLoopClosure(run_loop.QuitClosure());
  driver_->SendFormDataToRenderer(
      input_page_id, AutofillDriver::FORM_DATA_ACTION_PREVIEW, input_form_data);

  run_loop.RunUntilIdle();

  int output_page_id = 0;
  FormData output_form_data;
  EXPECT_FALSE(fake_agent_.GetAutofillFillFormMessage(&output_page_id,
                                                      &output_form_data));
  EXPECT_TRUE(fake_agent_.GetAutofillPreviewFormMessage(&output_page_id,
                                                        &output_form_data));
  EXPECT_EQ(input_page_id, output_page_id);
  EXPECT_TRUE(input_form_data.SameFormAs(output_form_data));
}

TEST_F(ContentAutofillDriverTest,
       TypePredictionsNotSentToRendererWhenDisabled) {
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormStructure form_structure(form);
  std::vector<FormStructure*> forms(1, &form_structure);

  base::RunLoop run_loop;
  fake_agent_.SetQuitLoopClosure(run_loop.QuitClosure());
  driver_->SendAutofillTypePredictionsToRenderer(forms);
  run_loop.RunUntilIdle();

  EXPECT_FALSE(fake_agent_.GetFieldTypePredictionsAvailable(NULL));
}

TEST_F(ContentAutofillDriverTest, TypePredictionsSentToRendererWhenEnabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kShowAutofillTypePredictions);

  FormData form;
  test::CreateTestAddressFormData(&form);
  FormStructure form_structure(form);
  std::vector<FormStructure*> forms(1, &form_structure);
  std::vector<FormDataPredictions> expected_type_predictions =
      FormStructure::GetFieldTypePredictions(forms);

  base::RunLoop run_loop;
  fake_agent_.SetQuitLoopClosure(run_loop.QuitClosure());
  driver_->SendAutofillTypePredictionsToRenderer(forms);
  run_loop.RunUntilIdle();

  std::vector<FormDataPredictions> output_type_predictions;
  EXPECT_TRUE(
      fake_agent_.GetFieldTypePredictionsAvailable(&output_type_predictions));
  EXPECT_EQ(expected_type_predictions, output_type_predictions);
}

TEST_F(ContentAutofillDriverTest, AcceptDataListSuggestion) {
  base::string16 input_value(base::ASCIIToUTF16("barfoo"));
  base::string16 output_value;

  base::RunLoop run_loop;
  fake_agent_.SetQuitLoopClosure(run_loop.QuitClosure());
  driver_->RendererShouldAcceptDataListSuggestion(input_value);
  run_loop.RunUntilIdle();

  EXPECT_TRUE(fake_agent_.GetString16AcceptDataListSuggestion(&output_value));
  EXPECT_EQ(input_value, output_value);
}

TEST_F(ContentAutofillDriverTest, ClearFilledFormSentToRenderer) {
  base::RunLoop run_loop;
  fake_agent_.SetQuitLoopClosure(run_loop.QuitClosure());
  driver_->RendererShouldClearFilledForm();
  run_loop.RunUntilIdle();

  EXPECT_TRUE(fake_agent_.GetCalledClearForm());
}

TEST_F(ContentAutofillDriverTest, ClearPreviewedFormSentToRenderer) {
  base::RunLoop run_loop;
  fake_agent_.SetQuitLoopClosure(run_loop.QuitClosure());
  driver_->RendererShouldClearPreviewedForm();
  run_loop.RunUntilIdle();

  EXPECT_TRUE(fake_agent_.GetCalledClearPreviewedForm());
}

TEST_F(ContentAutofillDriverTest, FillFieldWithValue) {
  base::string16 input_value(base::ASCIIToUTF16("barqux"));
  base::string16 output_value;

  base::RunLoop run_loop;
  fake_agent_.SetQuitLoopClosure(run_loop.QuitClosure());
  driver_->RendererShouldFillFieldWithValue(input_value);
  run_loop.RunUntilIdle();

  EXPECT_TRUE(fake_agent_.GetString16FillFieldWithValue(&output_value));
  EXPECT_EQ(input_value, output_value);
}

TEST_F(ContentAutofillDriverTest, PreviewFieldWithValue) {
  base::string16 input_value(base::ASCIIToUTF16("barqux"));
  base::string16 output_value;

  base::RunLoop run_loop;
  fake_agent_.SetQuitLoopClosure(run_loop.QuitClosure());
  driver_->RendererShouldPreviewFieldWithValue(input_value);
  run_loop.RunUntilIdle();

  EXPECT_TRUE(fake_agent_.GetString16PreviewFieldWithValue(&output_value));
  EXPECT_EQ(input_value, output_value);
}

// Tests that credit card form interactions are recorded on the current
// NavigationEntry's SSLStatus if the page is HTTP.
TEST_F(ContentAutofillDriverTest, CreditCardFormInteraction) {
  GURL url("http://example.test");
  NavigateAndCommit(url);
  driver_->DidInteractWithCreditCardForm();

  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(url, entry->GetURL());
  EXPECT_TRUE(!!(entry->GetSSL().content_status &
                 content::SSLStatus::DISPLAYED_CREDIT_CARD_FIELD_ON_HTTP));
}

// Tests that credit card form interactions are *not* recorded on the current
// NavigationEntry's SSLStatus if the page is *not* HTTP.
TEST_F(ContentAutofillDriverTest, CreditCardFormInteractionOnHTTPS) {
  GURL url("https://example.test");
  NavigateAndCommit(url);
  driver_->DidInteractWithCreditCardForm();

  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(url, entry->GetURL());
  EXPECT_FALSE(!!(entry->GetSSL().content_status &
                  content::SSLStatus::DISPLAYED_CREDIT_CARD_FIELD_ON_HTTP));
}

}  // namespace autofill
