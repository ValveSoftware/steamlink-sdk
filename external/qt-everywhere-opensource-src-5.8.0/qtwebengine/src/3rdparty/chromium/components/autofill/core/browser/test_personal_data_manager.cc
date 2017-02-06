// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_personal_data_manager.h"

#include "components/autofill/core/browser/personal_data_manager_observer.h"

namespace autofill {

TestPersonalDataManager::TestPersonalDataManager()
    : PersonalDataManager("en-US") {}

TestPersonalDataManager::~TestPersonalDataManager() {}

void TestPersonalDataManager::SetTestingPrefService(PrefService* pref_service) {
  SetPrefService(pref_service);
}

void TestPersonalDataManager::AddTestingProfile(AutofillProfile* profile) {
  profiles_.push_back(profile);
  NotifyPersonalDataChanged();
}

void TestPersonalDataManager::AddTestingCreditCard(CreditCard* credit_card) {
  credit_cards_.push_back(credit_card);
  NotifyPersonalDataChanged();
}

void TestPersonalDataManager::AddTestingServerCreditCard(
    const CreditCard& credit_card) {
  server_credit_cards_.push_back(new CreditCard(credit_card));
}

const std::vector<AutofillProfile*>& TestPersonalDataManager::GetProfiles()
    const {
  return profiles_;
}

const std::vector<AutofillProfile*>& TestPersonalDataManager::web_profiles()
    const {
  return profiles_;
}

const std::vector<CreditCard*>& TestPersonalDataManager::
    GetCreditCards() const {
  return credit_cards_;
}

std::string TestPersonalDataManager::SaveImportedProfile(
    const AutofillProfile& imported_profile) {
  imported_profile_ = imported_profile;
  return imported_profile.guid();
}

std::string TestPersonalDataManager::SaveImportedCreditCard(
    const CreditCard& imported_credit_card) {
  imported_credit_card_ = imported_credit_card;
  return imported_credit_card.guid();
}

std::string TestPersonalDataManager::CountryCodeForCurrentTimezone()
    const {
  return timezone_country_code_;
}

const std::string& TestPersonalDataManager::GetDefaultCountryCodeForNewAddress()
    const {
  if (default_country_code_.empty())
    return PersonalDataManager::GetDefaultCountryCodeForNewAddress();

  return default_country_code_;
}

}  // namespace autofill
