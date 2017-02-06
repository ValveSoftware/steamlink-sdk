// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLIENT_H_

#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/prefs/pref_service.h"
#include "components/rappor/test_rappor_service.h"
#include "google_apis/gaia/fake_identity_provider.h"
#include "google_apis/gaia/fake_oauth2_token_service.h"

namespace autofill {

// This class is for easier writing of tests.
class TestAutofillClient : public AutofillClient {
 public:
  TestAutofillClient();
  ~TestAutofillClient() override;

  // AutofillClient implementation.
  PersonalDataManager* GetPersonalDataManager() override;
  scoped_refptr<AutofillWebDataService> GetDatabase() override;
  PrefService* GetPrefs() override;
  sync_driver::SyncService* GetSyncService() override;
  IdentityProvider* GetIdentityProvider() override;
  rappor::RapporService* GetRapporService() override;
  void ShowAutofillSettings() override;
  void ShowUnmaskPrompt(const CreditCard& card,
                        UnmaskCardReason reason,
                        base::WeakPtr<CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(PaymentsRpcResult result) override;
  void ConfirmSaveCreditCardLocally(const CreditCard& card,
                                    const base::Closure& callback) override;
  void ConfirmSaveCreditCardToCloud(
      const CreditCard& card,
      std::unique_ptr<base::DictionaryValue> legal_message,
      const base::Closure& callback) override;
  void LoadRiskData(
      const base::Callback<void(const std::string&)>& callback) override;
  bool HasCreditCardScanFeature() override;
  void ScanCreditCard(const CreditCardScanCallback& callback) override;
  void ShowAutofillPopup(
      const gfx::RectF& element_bounds,
      base::i18n::TextDirection text_direction,
      const std::vector<Suggestion>& suggestions,
      base::WeakPtr<AutofillPopupDelegate> delegate) override;
  void UpdateAutofillPopupDataListValues(
      const std::vector<base::string16>& values,
      const std::vector<base::string16>& labels) override;
  void HideAutofillPopup() override;
  bool IsAutocompleteEnabled() override;
  void PropagateAutofillPredictions(
      content::RenderFrameHost* rfh,
      const std::vector<autofill::FormStructure*>& forms) override;
  void DidFillOrPreviewField(const base::string16& autofilled_value,
                             const base::string16& profile_full_name) override;
  void OnFirstUserGestureObserved() override;
  bool IsContextSecure(const GURL& form_origin) override;

  void set_is_context_secure(bool is_context_secure) {
    is_context_secure_ = is_context_secure;
  };

  void SetPrefs(std::unique_ptr<PrefService> prefs) {
    prefs_ = std::move(prefs);
  }

  rappor::TestRapporService* test_rappor_service() {
    return rappor_service_.get();
  }

 private:
  // NULL by default.
  std::unique_ptr<PrefService> prefs_;
  std::unique_ptr<FakeOAuth2TokenService> token_service_;
  std::unique_ptr<FakeIdentityProvider> identity_provider_;
  std::unique_ptr<rappor::TestRapporService> rappor_service_;

  bool is_context_secure_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillClient);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLIENT_H_
