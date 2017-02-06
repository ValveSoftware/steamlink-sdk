// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_PERSONAL_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_PERSONAL_DATA_MANAGER_H_

#include <vector>

#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace autofill {

// A simplistic PersonalDataManager used for testing.
class TestPersonalDataManager : public PersonalDataManager {
 public:
  TestPersonalDataManager();
  ~TestPersonalDataManager() override;

  // Sets which PrefService to use and observe. |pref_service| is not owned by
  // this class and must outlive |this|.
  void SetTestingPrefService(PrefService* pref_service);

  // Adds |profile| to |profiles_|. This does not take ownership of |profile|.
  void AddTestingProfile(AutofillProfile* profile);

  // Adds |credit_card| to |credit_cards_|. This does not take ownership of
  // |credit_card|.
  void AddTestingCreditCard(CreditCard* credit_card);

  // Adds |credit_card| to |server_credit_cards_| by copying.
  void AddTestingServerCreditCard(const CreditCard& credit_card);

  const std::vector<AutofillProfile*>& GetProfiles() const override;
  const std::vector<AutofillProfile*>& web_profiles() const override;
  const std::vector<CreditCard*>& GetCreditCards() const override;

  std::string SaveImportedProfile(
      const AutofillProfile& imported_profile) override;
  std::string SaveImportedCreditCard(
      const CreditCard& imported_credit_card) override;

  std::string CountryCodeForCurrentTimezone() const override;
  const std::string& GetDefaultCountryCodeForNewAddress() const override;

  void set_timezone_country_code(const std::string& timezone_country_code) {
    timezone_country_code_ = timezone_country_code;
  }
  void set_default_country_code(const std::string& default_country_code) {
    default_country_code_ = default_country_code;
  }

  const AutofillProfile& imported_profile() { return imported_profile_; }
  const CreditCard& imported_credit_card() { return imported_credit_card_; }

 private:
  std::vector<AutofillProfile*> profiles_;
  std::vector<CreditCard*> credit_cards_;
  AutofillProfile imported_profile_;
  CreditCard imported_credit_card_;
  std::string timezone_country_code_;
  std::string default_country_code_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_PERSONAL_DATA_MANAGER_H_
