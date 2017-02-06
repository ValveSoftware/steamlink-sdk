// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/personal_data_manager.h"

#include <stddef.h>
#include <algorithm>
#include <list>
#include <map>
#include <string>
#include <utility>

#include "base/i18n/case_conversion.h"
#include "base/i18n/timezone.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/address_i18n.h"
#include "components/autofill/core/browser/autofill-inl.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_profile_comparator.h"
#include "components/autofill/core/browser/country_data.h"
#include "components/autofill/core/browser/country_names.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/phone_number.h"
#include "components/autofill/core/browser/phone_number_i18n.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/sync_driver/sync_service.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_formatter.h"

namespace autofill {
namespace {

using ::i18n::addressinput::AddressField;
using ::i18n::addressinput::GetStreetAddressLinesAsSingleLine;
using ::i18n::addressinput::STREET_ADDRESS;

template<typename T>
class FormGroupMatchesByGUIDFunctor {
 public:
  explicit FormGroupMatchesByGUIDFunctor(const std::string& guid)
      : guid_(guid) {
  }

  bool operator()(const T& form_group) {
    return form_group.guid() == guid_;
  }

  bool operator()(const T* form_group) {
    return form_group->guid() == guid_;
  }

 private:
  const std::string guid_;
};

template<typename T, typename C>
typename C::const_iterator FindElementByGUID(const C& container,
                                             const std::string& guid) {
  return std::find_if(container.begin(),
                      container.end(),
                      FormGroupMatchesByGUIDFunctor<T>(guid));
}

template<typename T, typename C>
bool FindByGUID(const C& container, const std::string& guid) {
  return FindElementByGUID<T>(container, guid) != container.end();
}

template<typename T>
class IsEmptyFunctor {
 public:
  explicit IsEmptyFunctor(const std::string& app_locale)
      : app_locale_(app_locale) {
  }

  bool operator()(const T& form_group) {
    return form_group.IsEmpty(app_locale_);
  }

 private:
  const std::string app_locale_;
};

// Returns true if minimum requirements for import of a given |profile| have
// been met.  An address submitted via a form must have at least the fields
// required as determined by its country code.
// No verification of validity of the contents is preformed. This is an
// existence check only.
bool IsMinimumAddress(const AutofillProfile& profile,
                      const std::string& app_locale) {
  // All countries require at least one address line.
  if (profile.GetRawInfo(ADDRESS_HOME_LINE1).empty())
    return false;

  std::string country_code =
      base::UTF16ToASCII(profile.GetRawInfo(ADDRESS_HOME_COUNTRY));
  if (country_code.empty())
    country_code = AutofillCountry::CountryCodeForLocale(app_locale);

  AutofillCountry country(country_code, app_locale);

  if (country.requires_city() && profile.GetRawInfo(ADDRESS_HOME_CITY).empty())
    return false;

  if (country.requires_state() &&
      profile.GetRawInfo(ADDRESS_HOME_STATE).empty())
    return false;

  if (country.requires_zip() && profile.GetRawInfo(ADDRESS_HOME_ZIP).empty())
    return false;

  return true;
}

// Return true if the |field_type| and |value| are valid within the context
// of importing a form.
bool IsValidFieldTypeAndValue(const std::set<ServerFieldType>& types_seen,
                              ServerFieldType field_type,
                              const base::string16& value) {
  // Abandon the import if two fields of the same type are encountered.
  // This indicates ambiguous data or miscategorization of types.
  // Make an exception for PHONE_HOME_NUMBER however as both prefix and
  // suffix are stored against this type, and for EMAIL_ADDRESS because it is
  // common to see second 'confirm email address' fields on forms.
  if (types_seen.count(field_type) &&
      field_type != PHONE_HOME_NUMBER &&
      field_type != EMAIL_ADDRESS)
    return false;

  // Abandon the import if an email address value shows up in a field that is
  // not an email address.
  if (field_type != EMAIL_ADDRESS && IsValidEmailAddress(value))
    return false;

  return true;
}

// In addition to just getting the values out of the autocomplete profile, this
// function handles formatting of the street address into a single string.
base::string16 GetInfoInOneLine(const AutofillProfile* profile,
                                const AutofillType& type,
                                const std::string app_locale) {
  std::vector<base::string16> results;

  AddressField address_field;
  if (i18n::FieldForType(type.GetStorableType(), &address_field) &&
      address_field == STREET_ADDRESS) {
    std::string street_address_line;
    GetStreetAddressLinesAsSingleLine(
        *i18n::CreateAddressDataFromAutofillProfile(*profile, app_locale),
        &street_address_line);
    return base::UTF8ToUTF16(street_address_line);
  }

  return profile->GetInfo(type, app_locale);
}

// Receives the loaded profiles from the web data service and stores them in
// |*dest|. The pending handle is the address of the pending handle
// corresponding to this request type. This function is used to save both
// server and local profiles and credit cards.
template <typename ValueType>
void ReceiveLoadedDbValues(WebDataServiceBase::Handle h,
                           const WDTypedResult* result,
                           WebDataServiceBase::Handle* pending_handle,
                           ScopedVector<ValueType>* dest) {
  DCHECK_EQ(*pending_handle, h);
  *pending_handle = 0;

  const WDResult<std::vector<ValueType*>>* r =
      static_cast<const WDResult<std::vector<ValueType*>>*>(result);

  dest->clear();
  for (ValueType* value : r->GetValue())
    dest->push_back(value);
}

// A helper function for finding the maximum value in a string->int map.
static bool CompareVotes(const std::pair<std::string, int>& a,
                         const std::pair<std::string, int>& b) {
  return a.second < b.second;
}

// Returns whether the |suggestion| is valid considering the
// |field_contents_canon|, the |type| and |is_masked_server_card|. Assigns true
// to |is_prefix_matched| if the |field_contents_canon| is a prefix to
// |suggestion|, assigns false otherwise.
bool IsValidSuggestionForFieldContents(base::string16 suggestion_canon,
                                       base::string16 field_contents_canon,
                                       const AutofillType& type,
                                       bool is_masked_server_card,
                                       bool* is_prefix_matched) {
  *is_prefix_matched = true;

  // Phones should do a substring match because they can be trimmed to remove
  // the first parts (e.g. country code or prefix). It is still considered a
  // prefix match in order to put it at the top of the suggestions.
  if ((type.group() == PHONE_HOME || type.group() == PHONE_BILLING) &&
      suggestion_canon.find(field_contents_canon) != base::string16::npos) {
    return true;
  }

  // For card number fields, suggest the card if:
  // - the number matches any part of the card, or
  // - it's a masked card and there are 6 or fewers typed so far.
  if (type.GetStorableType() == CREDIT_CARD_NUMBER) {
    if (suggestion_canon.find(field_contents_canon) == base::string16::npos &&
        (!is_masked_server_card || field_contents_canon.size() >= 6)) {
      return false;
    }
    return true;
  }

  if (base::StartsWith(suggestion_canon, field_contents_canon,
                       base::CompareCase::SENSITIVE)) {
    return true;
  }

  if (IsFeatureSubstringMatchEnabled() &&
      suggestion_canon.length() >= field_contents_canon.length() &&
      GetTextSelectionStart(suggestion_canon, field_contents_canon, false) !=
          base::string16::npos) {
    *is_prefix_matched = false;
    return true;
  }

  return false;
}

}  // namespace

const char kFrecencyFieldTrialName[] = "AutofillProfileOrderByFrecency";
const char kFrecencyFieldTrialLimitParam[] = "limit";

PersonalDataManager::PersonalDataManager(const std::string& app_locale)
    : database_(NULL),
      is_data_loaded_(false),
      pending_profiles_query_(0),
      pending_server_profiles_query_(0),
      pending_creditcards_query_(0),
      pending_server_creditcards_query_(0),
      app_locale_(app_locale),
      pref_service_(NULL),
      account_tracker_(NULL),
      is_off_the_record_(false),
      has_logged_profile_count_(false),
      has_logged_credit_card_count_(false) {}

void PersonalDataManager::Init(scoped_refptr<AutofillWebDataService> database,
                               PrefService* pref_service,
                               AccountTrackerService* account_tracker,
                               SigninManagerBase* signin_manager,
                               bool is_off_the_record) {
  CountryNames::SetLocaleString(app_locale_);

  database_ = database;
  SetPrefService(pref_service);
  account_tracker_ = account_tracker;
  signin_manager_ = signin_manager;
  is_off_the_record_ = is_off_the_record;

  if (!is_off_the_record_)
    AutofillMetrics::LogIsAutofillEnabledAtStartup(IsAutofillEnabled());

  // WebDataService may not be available in tests.
  if (!database_.get())
    return;

  LoadProfiles();
  LoadCreditCards();

  database_->AddObserver(this);
  is_autofill_profile_dedupe_pending_ = IsAutofillProfileCleanupEnabled();
}

PersonalDataManager::~PersonalDataManager() {
  CancelPendingQuery(&pending_profiles_query_);
  CancelPendingQuery(&pending_server_profiles_query_);
  CancelPendingQuery(&pending_creditcards_query_);
  CancelPendingQuery(&pending_server_creditcards_query_);

  if (database_.get())
    database_->RemoveObserver(this);
}

void PersonalDataManager::OnSyncServiceInitialized(
    sync_driver::SyncService* sync_service) {
  // We want to know when, if at all, we need to run autofill profile de-
  // duplication: now or after waiting until sync has started.
  if (!is_autofill_profile_dedupe_pending_) {
    // De-duplication isn't enabled.
    return;
  }

  // If the sync service is configured to start and to sync autofill profiles,
  // then we can just let the notification that sync has started trigger the
  // de-duplication.
  if (sync_service && sync_service->CanSyncStart() &&
      sync_service->GetPreferredDataTypes().Has(syncer::AUTOFILL_PROFILE)) {
    return;
  }

  // This runs as a one-time fix, tracked in syncable prefs. If it has already
  // run, it is a NOP (other than checking the pref).
  ApplyProfileUseDatesFix();

  // This runs at most once per major version, tracked in syncable prefs. If it
  // has already run for this version, it's a NOP, other than checking the pref.
  ApplyDedupingRoutine();
}

void PersonalDataManager::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    const WDTypedResult* result) {
  DCHECK(pending_profiles_query_ || pending_server_profiles_query_ ||
         pending_creditcards_query_ || pending_server_creditcards_query_);

  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/422460 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "422460 PersonalDataManager::OnWebDataServiceRequestDone"));

  if (!result) {
    // Error from the web database.
    if (h == pending_creditcards_query_)
      pending_creditcards_query_ = 0;
    else if (h == pending_profiles_query_)
      pending_profiles_query_ = 0;
    return;
  }

  switch (result->GetType()) {
    case AUTOFILL_PROFILES_RESULT:
      if (h == pending_profiles_query_) {
        ReceiveLoadedDbValues(h, result, &pending_profiles_query_,
                              &web_profiles_);
        LogProfileCount();  // This only logs local profiles.
      } else {
        ReceiveLoadedDbValues(h, result, &pending_server_profiles_query_,
                              &server_profiles_);

        if (!server_profiles_.empty()) {
          std::string account_id = signin_manager_->GetAuthenticatedAccountId();
          base::string16 email =
              base::UTF8ToUTF16(
                  account_tracker_->GetAccountInfo(account_id).email);

          // User may have signed out during the fulfillment of the web data
          // request, in which case there is no point updating
          // |server_profiles_| as it will be cleared.
          if (!email.empty()) {
            for (AutofillProfile* profile : server_profiles_)
              profile->SetRawInfo(EMAIL_ADDRESS, email);
          }
        }
      }
      break;
    case AUTOFILL_CREDITCARDS_RESULT:
      if (h == pending_creditcards_query_) {
        ReceiveLoadedDbValues(h, result, &pending_creditcards_query_,
                              &local_credit_cards_);
        LogLocalCreditCardCount();
      } else {
        ReceiveLoadedDbValues(h, result, &pending_server_creditcards_query_,
                              &server_credit_cards_);

        // If the user has a saved unmasked server card and the experiment is
        // disabled, force mask all cards back to the unsaved state.
        if (!OfferStoreUnmaskedCards())
          ResetFullServerCards();
      }
      break;
    default:
      NOTREACHED();
  }

  // If all requests have responded, then all personal data is loaded.
  if (pending_profiles_query_ == 0 &&
      pending_creditcards_query_ == 0 &&
      pending_server_profiles_query_ == 0 &&
      pending_server_creditcards_query_ == 0) {
    is_data_loaded_ = true;
    NotifyPersonalDataChanged();
  }
}

void PersonalDataManager::AutofillMultipleChanged() {
  Refresh();
}

void PersonalDataManager::SyncStarted(syncer::ModelType model_type) {
  if (model_type == syncer::AUTOFILL_PROFILE) {
    // This runs as a one-time fix, tracked in syncable prefs. If it has already
    // run, it is a NOP (other than checking the pref).
    ApplyProfileUseDatesFix();

    // This runs at most once per major version, tracked in syncable prefs. If
    // it has already run for this version, it's a NOP, other than checking the
    // pref.
    ApplyDedupingRoutine();
  }
}

void PersonalDataManager::AddObserver(PersonalDataManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void PersonalDataManager::RemoveObserver(
    PersonalDataManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool PersonalDataManager::ImportFormData(
    const FormStructure& form,
    bool should_return_local_card,
    std::unique_ptr<CreditCard>* imported_credit_card) {
  // We try the same |form| for both credit card and address import/update.
  // - ImportCreditCard may update an existing card, or fill
  //   |imported_credit_card| with an extracted card. See .h for details of
  //   |should_return_local_card|.
  bool cc_import =
      ImportCreditCard(form, should_return_local_card, imported_credit_card);
  // - ImportAddressProfiles may eventually save or update one or more address
  //   profiles.
  bool address_import = ImportAddressProfiles(form);
  if (cc_import || address_import)
    return true;

  FOR_EACH_OBSERVER(PersonalDataManagerObserver, observers_,
                    OnInsufficientFormData());
  return false;
}

void PersonalDataManager::RecordUseOf(const AutofillDataModel& data_model) {
  if (is_off_the_record_ || !database_.get())
    return;

  CreditCard* credit_card = GetCreditCardByGUID(data_model.guid());
  if (credit_card) {
    credit_card->RecordAndLogUse();

    if (credit_card->record_type() == CreditCard::LOCAL_CARD)
      database_->UpdateCreditCard(*credit_card);
    else
      database_->UpdateServerCardUsageStats(*credit_card);

    Refresh();
    return;
  }

  AutofillProfile* profile = GetProfileByGUID(data_model.guid());
  if (profile) {
    profile->RecordAndLogUse();

    if (profile->record_type() == AutofillProfile::LOCAL_PROFILE)
      database_->UpdateAutofillProfile(*profile);
    else if (profile->record_type() == AutofillProfile::SERVER_PROFILE)
      database_->UpdateServerAddressUsageStats(*profile);

    Refresh();
  }
}

void PersonalDataManager::AddProfile(const AutofillProfile& profile) {
  if (is_off_the_record_)
    return;

  if (profile.IsEmpty(app_locale_))
    return;

  // Don't add an existing profile.
  if (FindByGUID<AutofillProfile>(web_profiles_, profile.guid()))
    return;

  if (!database_.get())
    return;

  // Don't add a duplicate.
  if (FindByContents(web_profiles_, profile))
    return;

  // Add the new profile to the web database.
  database_->AddAutofillProfile(profile);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::UpdateProfile(const AutofillProfile& profile) {
  if (is_off_the_record_)
    return;

  AutofillProfile* existing_profile = GetProfileByGUID(profile.guid());
  if (!existing_profile)
    return;

  // Don't overwrite the origin for a profile that is already stored.
  if (existing_profile->EqualsSansOrigin(profile))
    return;

  if (profile.IsEmpty(app_locale_)) {
    RemoveByGUID(profile.guid());
    return;
  }

  if (!database_.get())
    return;

  // Make the update.
  database_->UpdateAutofillProfile(profile);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

AutofillProfile* PersonalDataManager::GetProfileByGUID(
    const std::string& guid) {
  const std::vector<AutofillProfile*>& profiles = GetProfiles();
  std::vector<AutofillProfile*>::const_iterator iter =
      FindElementByGUID<AutofillProfile>(profiles, guid);
  return (iter != profiles.end()) ? *iter : NULL;
}

void PersonalDataManager::AddCreditCard(const CreditCard& credit_card) {
  if (is_off_the_record_)
    return;

  if (credit_card.IsEmpty(app_locale_))
    return;

  if (FindByGUID<CreditCard>(local_credit_cards_, credit_card.guid()))
    return;

  if (!database_.get())
    return;

  // Don't add a duplicate.
  if (FindByContents(local_credit_cards_, credit_card))
    return;

  // Add the new credit card to the web database.
  database_->AddCreditCard(credit_card);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::UpdateCreditCard(const CreditCard& credit_card) {
  DCHECK_EQ(CreditCard::LOCAL_CARD, credit_card.record_type());
  if (is_off_the_record_)
    return;

  CreditCard* existing_credit_card = GetCreditCardByGUID(credit_card.guid());
  if (!existing_credit_card)
    return;

  // Don't overwrite the origin for a credit card that is already stored.
  if (existing_credit_card->Compare(credit_card) == 0)
    return;

  if (credit_card.IsEmpty(app_locale_)) {
    RemoveByGUID(credit_card.guid());
    return;
  }

  if (!database_.get())
    return;

  // Make the update.
  database_->UpdateCreditCard(credit_card);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::UpdateServerCreditCard(
    const CreditCard& credit_card) {
  DCHECK_NE(CreditCard::LOCAL_CARD, credit_card.record_type());

  if (is_off_the_record_ || !database_.get())
    return;

  // Look up by server id, not GUID.
  CreditCard* existing_credit_card = nullptr;
  for (auto it : server_credit_cards_) {
    if (credit_card.server_id() == it->server_id()) {
      existing_credit_card = it;
      break;
    }
  }
  if (!existing_credit_card)
    return;

  DCHECK_NE(existing_credit_card->record_type(), credit_card.record_type());
  DCHECK_EQ(existing_credit_card->Label(), credit_card.Label());
  if (existing_credit_card->record_type() == CreditCard::MASKED_SERVER_CARD) {
    database_->UnmaskServerCreditCard(credit_card,
                                      credit_card.number());
  } else {
    database_->MaskServerCreditCard(credit_card.server_id());
  }

  Refresh();
}

void PersonalDataManager::UpdateServerCardBillingAddress(
    const CreditCard& credit_card) {
  DCHECK_NE(CreditCard::LOCAL_CARD, credit_card.record_type());

  if (!database_.get())
    return;

  CreditCard* existing_credit_card = nullptr;
  for (auto it : server_credit_cards_) {
    if (credit_card.server_id() == it->server_id()) {
      existing_credit_card = it;
      break;
    }
  }
  if (!existing_credit_card
      || existing_credit_card->billing_address_id() ==
          credit_card.billing_address_id()) {
    return;
  }

  existing_credit_card->set_billing_address_id(
      credit_card.billing_address_id());
  database_->UpdateServerCardBillingAddress(*existing_credit_card);

  Refresh();
}

void PersonalDataManager::ResetFullServerCard(const std::string& guid) {
  for (const CreditCard* card : server_credit_cards_) {
    if (card->guid() == guid) {
      DCHECK_EQ(card->record_type(), CreditCard::FULL_SERVER_CARD);
      CreditCard card_copy = *card;
      card_copy.set_record_type(CreditCard::MASKED_SERVER_CARD);
      card_copy.SetNumber(card->LastFourDigits());
      UpdateServerCreditCard(card_copy);
      break;
    }
  }
}

void PersonalDataManager::ResetFullServerCards() {
  for (const CreditCard* card : server_credit_cards_) {
    if (card->record_type() == CreditCard::FULL_SERVER_CARD) {
      CreditCard card_copy = *card;
      card_copy.set_record_type(CreditCard::MASKED_SERVER_CARD);
      card_copy.SetNumber(card->LastFourDigits());
      UpdateServerCreditCard(card_copy);
    }
  }
}

void PersonalDataManager::ClearAllServerData() {
  // This could theoretically be called before we get the data back from the
  // database on startup, and it could get called when the wallet pref is
  // off (meaning this class won't even query for the server data) so don't
  // check the server_credit_cards_/profiles_ before posting to the DB.
  database_->ClearAllServerData();

  // The above call will eventually clear our server data by notifying us
  // that the data changed and then this class will re-fetch. Preemptively
  // clear so that tests can synchronously verify that this data was cleared.
  server_credit_cards_.clear();
  server_profiles_.clear();
}

void PersonalDataManager::AddServerCreditCardForTest(
    std::unique_ptr<CreditCard> credit_card) {
  server_credit_cards_.push_back(credit_card.release());
}

void PersonalDataManager::RemoveByGUID(const std::string& guid) {
  if (is_off_the_record_)
    return;

  bool is_credit_card = FindByGUID<CreditCard>(local_credit_cards_, guid);
  bool is_profile = !is_credit_card &&
      FindByGUID<AutofillProfile>(web_profiles_, guid);
  if (!is_credit_card && !is_profile)
    return;

  if (!database_.get())
    return;

  if (is_credit_card)
    database_->RemoveCreditCard(guid);
  else
    database_->RemoveAutofillProfile(guid);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

CreditCard* PersonalDataManager::GetCreditCardByGUID(const std::string& guid) {
  const std::vector<CreditCard*>& credit_cards = GetCreditCards();
  std::vector<CreditCard*>::const_iterator iter =
      FindElementByGUID<CreditCard>(credit_cards, guid);
  return (iter != credit_cards.end()) ? *iter : NULL;
}

void PersonalDataManager::GetNonEmptyTypes(
    ServerFieldTypeSet* non_empty_types) {
  for (AutofillProfile* profile : GetProfiles())
    profile->GetNonEmptyTypes(app_locale_, non_empty_types);
  for (CreditCard* card : GetCreditCards())
    card->GetNonEmptyTypes(app_locale_, non_empty_types);
}

bool PersonalDataManager::IsDataLoaded() const {
  return is_data_loaded_;
}

const std::vector<AutofillProfile*>& PersonalDataManager::GetProfiles() const {
  return GetProfiles(false);
}

const std::vector<AutofillProfile*>& PersonalDataManager::web_profiles() const {
  return web_profiles_.get();
}

const std::vector<CreditCard*>& PersonalDataManager::GetLocalCreditCards()
    const {
  return local_credit_cards_.get();
}

const std::vector<CreditCard*>& PersonalDataManager::GetCreditCards() const {
  credit_cards_.clear();
  credit_cards_.insert(credit_cards_.end(), local_credit_cards_.begin(),
                       local_credit_cards_.end());
  if (pref_service_->GetBoolean(prefs::kAutofillWalletImportEnabled)) {
    credit_cards_.insert(credit_cards_.end(), server_credit_cards_.begin(),
                         server_credit_cards_.end());
  }
  return credit_cards_;
}

bool PersonalDataManager::HasServerData() const {
  return !server_credit_cards_.empty() || !server_profiles_.empty();
}

void PersonalDataManager::Refresh() {
  LoadProfiles();
  LoadCreditCards();
}

const std::vector<AutofillProfile*> PersonalDataManager::GetProfilesToSuggest()
    const {
  std::vector<AutofillProfile*> profiles = GetProfiles(true);

  // Rank the suggestions by frecency (see AutofillDataModel for details).
  base::Time comparison_time = base::Time::Now();
  std::sort(profiles.begin(), profiles.end(),
            [comparison_time](const AutofillDataModel* a,
                              const AutofillDataModel* b) {
              return a->CompareFrecency(b, comparison_time);
            });

  return profiles;
}

std::vector<Suggestion> PersonalDataManager::GetProfileSuggestions(
    const AutofillType& type,
    const base::string16& field_contents,
    bool field_is_autofilled,
    const std::vector<ServerFieldType>& other_field_types) {
  if (IsInAutofillSuggestionsDisabledExperiment())
    return std::vector<Suggestion>();

  AutofillProfileComparator comparator(app_locale_);
  base::string16 field_contents_canon =
      comparator.NormalizeForComparison(field_contents);

  // Get the profiles to suggest, which are already sorted.
  std::vector<AutofillProfile*> profiles = GetProfilesToSuggest();

  std::vector<Suggestion> suggestions;
  // Match based on a prefix search.
  std::vector<AutofillProfile*> matched_profiles;
  for (AutofillProfile* profile : profiles) {
    base::string16 value = GetInfoInOneLine(profile, type, app_locale_);
    if (value.empty())
      continue;

    bool prefix_matched_suggestion;
    base::string16 suggestion_canon = comparator.NormalizeForComparison(value);
    if (IsValidSuggestionForFieldContents(
            suggestion_canon, field_contents_canon, type,
            /* is_masked_server_card= */ false, &prefix_matched_suggestion)) {
      matched_profiles.push_back(profile);
      suggestions.push_back(Suggestion(value));
      suggestions.back().backend_id = profile->guid();
      suggestions.back().match = prefix_matched_suggestion
                                     ? Suggestion::PREFIX_MATCH
                                     : Suggestion::SUBSTRING_MATCH;
    }
  }

  // Prefix matches should precede other token matches.
  if (IsFeatureSubstringMatchEnabled()) {
    std::stable_sort(suggestions.begin(), suggestions.end(),
                     [](const Suggestion& a, const Suggestion& b) {
                       return a.match < b.match;
                     });
  }

  // Don't show two suggestions if one is a subset of the other.
  std::vector<AutofillProfile*> unique_matched_profiles;
  std::vector<Suggestion> unique_suggestions;
  ServerFieldTypeSet types(other_field_types.begin(), other_field_types.end());
  for (size_t i = 0; i < matched_profiles.size(); ++i) {
    bool include = true;
    AutofillProfile* profile_a = matched_profiles[i];
    for (size_t j = 0; j < matched_profiles.size(); ++j) {
      AutofillProfile* profile_b = matched_profiles[j];
      // Check if profile A is a subset of profile B. If not, continue.
      if (i == j || suggestions[i].value != suggestions[j].value ||
          !profile_a->IsSubsetOfForFieldSet(*profile_b, app_locale_, types)) {
        continue;
      }

      // Check if profile B is also a subset of profile A. If so, the
      // profiles are identical. Include the first one but not the second.
      if (i < j &&
          profile_b->IsSubsetOfForFieldSet(*profile_a, app_locale_, types)) {
        continue;
      }

      // One-way subset. Don't include profile A.
      include = false;
      break;
    }
    if (include) {
      unique_matched_profiles.push_back(matched_profiles[i]);
      unique_suggestions.push_back(suggestions[i]);
    }
  }

  // Generate disambiguating labels based on the list of matches.
  std::vector<base::string16> labels;
  AutofillProfile::CreateInferredLabels(
      unique_matched_profiles, &other_field_types, type.GetStorableType(), 1,
      app_locale_, &labels);
  DCHECK_EQ(unique_suggestions.size(), labels.size());
  for (size_t i = 0; i < labels.size(); i++)
    unique_suggestions[i].label = labels[i];

  // Get the profile suggestions limit value set for the current frecency field
  // trial group or SIZE_MAX if no limit is defined.
  std::string limit_str = variations::GetVariationParamValue(
      kFrecencyFieldTrialName, kFrecencyFieldTrialLimitParam);
  size_t limit = base::StringToSizeT(limit_str, &limit) ? limit : SIZE_MAX;

  unique_suggestions.resize(std::min(unique_suggestions.size(), limit));

  return unique_suggestions;
}

// TODO(crbug.com/613187): Investigate if it would be more efficient to dedupe
// with a vector instead of a list.
const std::vector<CreditCard*> PersonalDataManager::GetCreditCardsToSuggest()
    const {
  std::vector<CreditCard*> credit_cards = GetCreditCards();

  std::list<CreditCard*> cards_to_dedupe(credit_cards.begin(),
                                         credit_cards.end());

  DedupeCreditCardToSuggest(&cards_to_dedupe);

  std::vector<CreditCard*> cards_to_suggest(
      std::make_move_iterator(std::begin(cards_to_dedupe)),
      std::make_move_iterator(std::end(cards_to_dedupe)));

  // Rank the cards by frecency (see AutofillDataModel for details). All expired
  // cards should be suggested last, also by frecency.
  base::Time comparison_time = base::Time::Now();
  std::stable_sort(cards_to_suggest.begin(), cards_to_suggest.end(),
                   [comparison_time](const CreditCard* a, const CreditCard* b) {
                     bool a_is_expired = a->IsExpired(comparison_time);
                     if (a_is_expired != b->IsExpired(comparison_time))
                       return !a_is_expired;

                     return a->CompareFrecency(b, comparison_time);
                   });

  return cards_to_suggest;
}

std::vector<Suggestion> PersonalDataManager::GetCreditCardSuggestions(
    const AutofillType& type,
    const base::string16& field_contents) {
  if (IsInAutofillSuggestionsDisabledExperiment())
    return std::vector<Suggestion>();

  return GetSuggestionsForCards(type, field_contents,
                                GetCreditCardsToSuggest());
}

bool PersonalDataManager::IsAutofillEnabled() const {
  return ::autofill::IsAutofillEnabled(pref_service_);
}

std::string PersonalDataManager::CountryCodeForCurrentTimezone() const {
  return base::CountryCodeForCurrentTimezone();
}

void PersonalDataManager::SetPrefService(PrefService* pref_service) {
  enabled_pref_.reset(new BooleanPrefMember);
  wallet_enabled_pref_.reset(new BooleanPrefMember);
  pref_service_ = pref_service;
  // |pref_service_| can be NULL in tests.
  if (pref_service_) {
    enabled_pref_->Init(prefs::kAutofillEnabled, pref_service_,
        base::Bind(&PersonalDataManager::EnabledPrefChanged,
                   base::Unretained(this)));
    wallet_enabled_pref_->Init(prefs::kAutofillWalletImportEnabled,
        pref_service_,
        base::Bind(&PersonalDataManager::EnabledPrefChanged,
                   base::Unretained(this)));
  }
}

// static
bool PersonalDataManager::IsValidLearnableProfile(
    const AutofillProfile& profile,
    const std::string& app_locale) {
  if (!IsMinimumAddress(profile, app_locale))
    return false;

  base::string16 email = profile.GetRawInfo(EMAIL_ADDRESS);
  if (!email.empty() && !IsValidEmailAddress(email))
    return false;

  // Reject profiles with invalid US state information.
  if (profile.IsPresentButInvalid(ADDRESS_HOME_STATE))
    return false;

  // Reject profiles with invalid US zip information.
  if (profile.IsPresentButInvalid(ADDRESS_HOME_ZIP))
    return false;

  return true;
}

// TODO(crbug.com/618448): Refactor MergeProfile to not depend on class
// variables.
std::string PersonalDataManager::MergeProfile(
    const AutofillProfile& new_profile,
    std::vector<AutofillProfile*> existing_profiles,
    const std::string& app_locale,
    std::vector<AutofillProfile>* merged_profiles) {
  merged_profiles->clear();

  // Sort the existing profiles in decreasing order of frecency, so the "best"
  // profiles are checked first. Put the verified profiles last so the non
  // verified profiles get deduped among themselves before reaching the verified
  // profiles.
  // TODO(crbug.com/620521): Remove the check for verified from the sort.
  base::Time comparison_time = base::Time::Now();
  std::sort(existing_profiles.begin(), existing_profiles.end(),
            [comparison_time](const AutofillDataModel* a,
                              const AutofillDataModel* b) {
              if (a->IsVerified() != b->IsVerified())
                return !a->IsVerified();
              return a->CompareFrecency(b, comparison_time);
            });

  // Set to true if |existing_profiles| already contains an equivalent profile.
  bool matching_profile_found = false;
  std::string guid = new_profile.guid();

  // If we have already saved this address, merge in any missing values.
  // Only merge with the first match.
  AutofillProfileComparator comparator(app_locale);
  for (AutofillProfile* existing_profile : existing_profiles) {
    if (!matching_profile_found &&
        comparator.AreMergeable(new_profile, *existing_profile) &&
        existing_profile->SaveAdditionalInfo(new_profile, app_locale)) {
      // Unverified profiles should always be updated with the newer data,
      // whereas verified profiles should only ever be overwritten by verified
      // data.  If an automatically aggregated profile would overwrite a
      // verified profile, just drop it.
      matching_profile_found = true;
      guid = existing_profile->guid();

      // We set the modification date so that immediate requests for profiles
      // will properly reflect the fact that this profile has been modified
      // recently. After writing to the database and refreshing the local copies
      // the profile will have a very slightly newer time reflecting what's
      // actually stored in the database.
      existing_profile->set_modification_date(base::Time::Now());
    }
    merged_profiles->push_back(*existing_profile);
  }

  // If the new profile was not merged with an existing one, add it to the list.
  if (!matching_profile_found) {
    merged_profiles->push_back(new_profile);
    // Similar to updating merged profiles above, set the modification date on
    // new profiles.
    merged_profiles->back().set_modification_date(base::Time::Now());
    AutofillMetrics::LogProfileActionOnFormSubmitted(
        AutofillMetrics::NEW_PROFILE_CREATED);
  }

  return guid;
}

bool PersonalDataManager::IsCountryOfInterest(const std::string& country_code)
    const {
  DCHECK_EQ(2U, country_code.size());

  const std::vector<AutofillProfile*>& profiles = web_profiles();
  std::list<std::string> country_codes;
  for (size_t i = 0; i < profiles.size(); ++i) {
    country_codes.push_back(base::ToLowerASCII(base::UTF16ToASCII(
        profiles[i]->GetRawInfo(ADDRESS_HOME_COUNTRY))));
  }

  std::string timezone_country = CountryCodeForCurrentTimezone();
  if (!timezone_country.empty())
    country_codes.push_back(base::ToLowerASCII(timezone_country));

  // Only take the locale into consideration if all else fails.
  if (country_codes.empty()) {
    country_codes.push_back(base::ToLowerASCII(
        AutofillCountry::CountryCodeForLocale(app_locale())));
  }

  return std::find(country_codes.begin(), country_codes.end(),
                   base::ToLowerASCII(country_code)) != country_codes.end();
}

const std::string& PersonalDataManager::GetDefaultCountryCodeForNewAddress()
    const {
  if (default_country_code_.empty())
    default_country_code_ = MostCommonCountryCodeFromProfiles();

  // Failing that, guess based on system timezone.
  if (default_country_code_.empty())
    default_country_code_ = CountryCodeForCurrentTimezone();

  // Failing that, guess based on locale.
  if (default_country_code_.empty())
    default_country_code_ = AutofillCountry::CountryCodeForLocale(app_locale());

  return default_country_code_;
}

// static
void PersonalDataManager::DedupeCreditCardToSuggest(
    std::list<CreditCard*>* cards_to_suggest) {
  for (auto outer_it = cards_to_suggest->begin();
       outer_it != cards_to_suggest->end(); ++outer_it) {
    // If considering a full server card, look for local cards that are
    // duplicates of it and remove them.
    if ((*outer_it)->record_type() == CreditCard::FULL_SERVER_CARD) {
      for (auto inner_it = cards_to_suggest->begin();
           inner_it != cards_to_suggest->end();) {
        auto inner_it_copy = inner_it++;
        if ((*inner_it_copy)->IsLocalDuplicateOfServerCard(**outer_it))
          cards_to_suggest->erase(inner_it_copy);
      }
      // If considering a local card, look for masked server cards that are
      // duplicates of it and remove them.
    } else if ((*outer_it)->record_type() == CreditCard::LOCAL_CARD) {
      for (auto inner_it = cards_to_suggest->begin();
           inner_it != cards_to_suggest->end();) {
        auto inner_it_copy = inner_it++;
        if ((*inner_it_copy)->record_type() == CreditCard::MASKED_SERVER_CARD &&
            (*outer_it)->IsLocalDuplicateOfServerCard(**inner_it_copy)) {
          cards_to_suggest->erase(inner_it_copy);
        }
      }
    }
  }
}

void PersonalDataManager::SetProfiles(std::vector<AutofillProfile>* profiles) {
  if (is_off_the_record_)
    return;

  // Remove empty profiles from input.
  profiles->erase(std::remove_if(profiles->begin(), profiles->end(),
                                 IsEmptyFunctor<AutofillProfile>(app_locale_)),
                  profiles->end());

  if (!database_.get())
    return;

  // Any profiles that are not in the new profile list should be removed from
  // the web database.
  for (const AutofillProfile* it : web_profiles_) {
    if (!FindByGUID<AutofillProfile>(*profiles, it->guid()))
      database_->RemoveAutofillProfile(it->guid());
  }

  // Update the web database with the existing profiles.
  for (const AutofillProfile& it : *profiles) {
    if (FindByGUID<AutofillProfile>(web_profiles_, it.guid()))
      database_->UpdateAutofillProfile(it);
  }

  // Add the new profiles to the web database.  Don't add a duplicate.
  for (const AutofillProfile& it : *profiles) {
    if (!FindByGUID<AutofillProfile>(web_profiles_, it.guid()) &&
        !FindByContents(web_profiles_, it))
      database_->AddAutofillProfile(it);
  }

  // Copy in the new profiles.
  web_profiles_.clear();
  for (const AutofillProfile& it : *profiles) {
    web_profiles_.push_back(new AutofillProfile(it));
  }

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::SetCreditCards(
    std::vector<CreditCard>* credit_cards) {
  if (is_off_the_record_)
    return;

  // Remove empty credit cards from input.
  credit_cards->erase(std::remove_if(credit_cards->begin(), credit_cards->end(),
                                     IsEmptyFunctor<CreditCard>(app_locale_)),
                      credit_cards->end());

  if (!database_.get())
    return;

  // Any credit cards that are not in the new credit card list should be
  // removed.
  for (const CreditCard* card : local_credit_cards_) {
    if (!FindByGUID<CreditCard>(*credit_cards, card->guid()))
      database_->RemoveCreditCard(card->guid());
  }

  // Update the web database with the existing credit cards.
  for (const CreditCard& card : *credit_cards) {
    if (FindByGUID<CreditCard>(local_credit_cards_, card.guid()))
      database_->UpdateCreditCard(card);
  }

  // Add the new credit cards to the web database.  Don't add a duplicate.
  for (const CreditCard& card : *credit_cards) {
    if (!FindByGUID<CreditCard>(local_credit_cards_, card.guid()) &&
        !FindByContents(local_credit_cards_, card))
      database_->AddCreditCard(card);
  }

  // Copy in the new credit cards.
  local_credit_cards_.clear();
  for (const CreditCard& card : *credit_cards)
    local_credit_cards_.push_back(new CreditCard(card));

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::LoadProfiles() {
  if (!database_.get()) {
    NOTREACHED();
    return;
  }

  CancelPendingQuery(&pending_profiles_query_);
  CancelPendingQuery(&pending_server_profiles_query_);

  pending_profiles_query_ = database_->GetAutofillProfiles(this);
  pending_server_profiles_query_ = database_->GetServerProfiles(this);
}

void PersonalDataManager::LoadCreditCards() {
  if (!database_.get()) {
    NOTREACHED();
    return;
  }

  CancelPendingQuery(&pending_creditcards_query_);
  CancelPendingQuery(&pending_server_creditcards_query_);

  pending_creditcards_query_ = database_->GetCreditCards(this);
  pending_server_creditcards_query_ = database_->GetServerCreditCards(this);
}

void PersonalDataManager::CancelPendingQuery(
    WebDataServiceBase::Handle* handle) {
  if (*handle) {
    if (!database_.get()) {
      NOTREACHED();
      return;
    }
    database_->CancelRequest(*handle);
  }
  *handle = 0;
}

std::string PersonalDataManager::SaveImportedProfile(
    const AutofillProfile& imported_profile) {
  if (is_off_the_record_)
    return std::string();

  // Don't save a web profile if the data in the profile is a subset of a
  // server profile, but do record the fact that it was used.
  for (const AutofillProfile* profile : server_profiles_) {
    if (imported_profile.IsSubsetOf(*profile, app_locale_)) {
      RecordUseOf(*profile);
      return profile->guid();
    }
  }

  std::vector<AutofillProfile> profiles;
  std::string guid = MergeProfile(imported_profile, web_profiles_.get(),
                                  app_locale_, &profiles);
  SetProfiles(&profiles);
  return guid;
}

void PersonalDataManager::NotifyPersonalDataChanged() {
  FOR_EACH_OBSERVER(PersonalDataManagerObserver, observers_,
                    OnPersonalDataChanged());
}

std::string PersonalDataManager::SaveImportedCreditCard(
    const CreditCard& imported_card) {
  DCHECK(!imported_card.number().empty());
  if (is_off_the_record_)
    return std::string();

  // Set to true if |imported_card| is merged into the credit card list.
  bool merged = false;

  std::string guid = imported_card.guid();
  std::vector<CreditCard> credit_cards;
  for (CreditCard* card : local_credit_cards_) {
    // If |imported_card| has not yet been merged, check whether it should be
    // with the current |card|.
    if (!merged && card->UpdateFromImportedCard(imported_card, app_locale_)) {
      guid = card->guid();
      merged = true;
    }

    credit_cards.push_back(*card);
  }

  if (!merged)
    credit_cards.push_back(imported_card);

  SetCreditCards(&credit_cards);
  return guid;
}

void PersonalDataManager::LogProfileCount() const {
  if (!has_logged_profile_count_) {
    AutofillMetrics::LogStoredProfileCount(web_profiles_.size());
    has_logged_profile_count_ = true;
  }
}

void PersonalDataManager::LogLocalCreditCardCount() const {
  if (!has_logged_credit_card_count_) {
    AutofillMetrics::LogStoredLocalCreditCardCount(local_credit_cards_.size());
    has_logged_credit_card_count_ = true;
  }
}

std::string PersonalDataManager::MostCommonCountryCodeFromProfiles() const {
  if (!IsAutofillEnabled())
    return std::string();

  // Count up country codes from existing profiles.
  std::map<std::string, int> votes;
  // TODO(estade): can we make this GetProfiles() instead? It seems to cause
  // errors in tests on mac trybots. See http://crbug.com/57221
  const std::vector<AutofillProfile*>& profiles = web_profiles();
  const std::vector<std::string>& country_codes =
      CountryDataMap::GetInstance()->country_codes();
  for (size_t i = 0; i < profiles.size(); ++i) {
    std::string country_code = base::ToUpperASCII(base::UTF16ToASCII(
        profiles[i]->GetRawInfo(ADDRESS_HOME_COUNTRY)));

    if (std::find(country_codes.begin(), country_codes.end(), country_code) !=
            country_codes.end()) {
      // Verified profiles count 100x more than unverified ones.
      votes[country_code] += profiles[i]->IsVerified() ? 100 : 1;
    }
  }

  // Take the most common country code.
  if (!votes.empty()) {
    std::map<std::string, int>::iterator iter =
        std::max_element(votes.begin(), votes.end(), CompareVotes);
    return iter->first;
  }

  return std::string();
}

void PersonalDataManager::EnabledPrefChanged() {
  default_country_code_.clear();
  if (!pref_service_->GetBoolean(prefs::kAutofillWalletImportEnabled)) {
    // Re-mask all server cards when the user turns off wallet card
    // integration.
    ResetFullServerCards();
  }
  NotifyPersonalDataChanged();
}

bool PersonalDataManager::ImportAddressProfiles(const FormStructure& form) {
  if (!form.field_count())
    return false;

  // Relevant sections for address fields.
  std::set<std::string> sections;
  for (const AutofillField* field : form) {
    if (field->Type().group() != CREDIT_CARD)
      sections.insert(field->section());
  }

  // We save a maximum of 2 profiles per submitted form (e.g. for shipping and
  // billing).
  static const size_t kMaxNumAddressProfilesSaved = 2;
  size_t num_saved_profiles = 0;
  for (const std::string& section : sections) {
    if (num_saved_profiles == kMaxNumAddressProfilesSaved)
      break;

    if (ImportAddressProfileForSection(form, section))
      num_saved_profiles++;
  }

  return num_saved_profiles > 0;
}

bool PersonalDataManager::ImportAddressProfileForSection(
    const FormStructure& form,
    const std::string& section) {
  // The candidate for profile import. There are many ways for the candidate to
  // be rejected (see everywhere this function returns false).
  AutofillProfile candidate_profile;
  candidate_profile.set_origin(form.source_url().spec());

  // We only set complete phone, so aggregate phone parts in these vars and set
  // complete at the end.
  PhoneNumber::PhoneCombineHelper combined_phone;

  // Used to detect and discard address forms with multiple fields of the same
  // type.
  std::set<ServerFieldType> types_seen;

  // Go through each |form| field and attempt to constitute a valid profile.
  for (const AutofillField* field : form) {
    // Reject fields that are not within the specified |section|.
    if (field->section() != section)
      continue;

    base::string16 value;
    base::TrimWhitespace(field->value, base::TRIM_ALL, &value);

    // If we don't know the type of the field, or the user hasn't entered any
    // information into the field, or the field is non-focusable (hidden), then
    // skip it.
    if (!field->IsFieldFillable() || !field->is_focusable || value.empty())
      continue;

    AutofillType field_type = field->Type();

    // Credit card fields are handled by ImportCreditCard().
    if (field_type.group() == CREDIT_CARD)
      continue;

    // There can be multiple email fields (e.g. in the case of 'confirm email'
    // fields) but they must all contain the same value, else the profile is
    // invalid.
    ServerFieldType server_field_type = field_type.GetStorableType();
    if (server_field_type == EMAIL_ADDRESS &&
        types_seen.count(server_field_type) &&
        candidate_profile.GetRawInfo(EMAIL_ADDRESS) != value)
      return false;

    // If the field type and |value| don't pass basic validity checks then
    // abandon the import.
    if (!IsValidFieldTypeAndValue(types_seen, server_field_type, value))
      return false;
    types_seen.insert(server_field_type);

    // We need to store phone data in the variables, before building the whole
    // number at the end. If |value| is not from a phone field, home.SetInfo()
    // returns false and data is stored directly in |candidate_profile|.
    if (!combined_phone.SetInfo(field_type, value))
      candidate_profile.SetInfo(field_type, value, app_locale_);

    // Reject profiles with invalid country information.
    if (server_field_type == ADDRESS_HOME_COUNTRY &&
        candidate_profile.GetRawInfo(ADDRESS_HOME_COUNTRY).empty())
      return false;
  }

  // Construct the phone number. Reject the whole profile if the number is
  // invalid.
  if (!combined_phone.IsEmpty()) {
    base::string16 constructed_number;
    if (!combined_phone.ParseNumber(candidate_profile, app_locale_,
                                    &constructed_number) ||
        !candidate_profile.SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER),
                                   constructed_number, app_locale_)) {
      return false;
    }
  }

  // Reject the profile if minimum address and validation requirements are not
  // met.
  if (!IsValidLearnableProfile(candidate_profile, app_locale_))
    return false;

  SaveImportedProfile(candidate_profile);
  return true;
}

bool PersonalDataManager::ImportCreditCard(
    const FormStructure& form,
    bool should_return_local_card,
    std::unique_ptr<CreditCard>* imported_credit_card) {
  DCHECK(!imported_credit_card->get());

  // The candidate for credit card import. There are many ways for the candidate
  // to be rejected (see everywhere this function returns false, below).
  CreditCard candidate_credit_card;
  candidate_credit_card.set_origin(form.source_url().spec());

  std::set<ServerFieldType> types_seen;
  for (const AutofillField* field : form) {
    base::string16 value;
    base::TrimWhitespace(field->value, base::TRIM_ALL, &value);

    // If we don't know the type of the field, or the user hasn't entered any
    // information into the field, or the field is non-focusable (hidden), then
    // skip it.
    if (!field->IsFieldFillable() || !field->is_focusable || value.empty())
      continue;

    AutofillType field_type = field->Type();
    // Field was not identified as a credit card field.
    if (field_type.group() != CREDIT_CARD)
      continue;

    // If we've seen the same credit card field type twice in the same form,
    // abort credit card import/update.
    ServerFieldType server_field_type = field_type.GetStorableType();
    if (types_seen.count(server_field_type))
      return false;
    types_seen.insert(server_field_type);

    // If |field| is an HTML5 month input, handle it as a special case.
    if (base::LowerCaseEqualsASCII(field->form_control_type, "month")) {
      DCHECK_EQ(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, server_field_type);
      candidate_credit_card.SetInfoForMonthInputType(value);
      continue;
    }

    // CreditCard handles storing the |value| according to |field_type|.
    bool saved = candidate_credit_card.SetInfo(field_type, value, app_locale_);

    // Saving with the option text (here |value|) may fail for the expiration
    // month. Attempt to save with the option value. First find the index of the
    // option text in the select options and try the corresponding value.
    if (!saved && server_field_type == CREDIT_CARD_EXP_MONTH) {
      for (size_t i = 0; i < field->option_contents.size(); ++i) {
        if (value == field->option_contents[i]) {
          candidate_credit_card.SetInfo(field_type, field->option_values[i],
                                        app_locale_);
          break;
        }
      }
    }
  }

  // Reject the credit card if we did not detect enough filled credit card
  // fields (such as valid number, month, year).
  if (!candidate_credit_card.IsValid())
    return false;

  // Attempt to merge with an existing credit card. Don't present a prompt if we
  // have already saved this card number, unless |should_return_local_card| is
  // true which indicates that upload is enabled. In this case, it's useful to
  // present the upload prompt to the user to promote the card from a local card
  // to a synced server card.
  for (const CreditCard* card : local_credit_cards_) {
    // Make a local copy so that the data in |local_credit_cards_| isn't
    // modified directly by the UpdateFromImportedCard() call.
    CreditCard card_copy(*card);
    if (card_copy.UpdateFromImportedCard(candidate_credit_card,
                                         app_locale_)) {
      UpdateCreditCard(card_copy);
      // If we should not return the local card, return that we merged it,
      // without setting |imported_credit_card|.
      if (!should_return_local_card)
        return true;

      break;
    }
  }

  // Also don't offer to save if we already have this stored as a server card.
  // We only check the number because if the new card has the same number as the
  // server card, upload is guaranteed to fail. There's no mechanism for entries
  // with the same number but different names or expiration dates as there is
  // for local cards.
  for (const CreditCard* card : server_credit_cards_) {
    if (candidate_credit_card.HasSameNumberAs(*card))
      return false;
  }

  imported_credit_card->reset(new CreditCard(candidate_credit_card));
  return true;
}

const std::vector<AutofillProfile*>& PersonalDataManager::GetProfiles(
    bool record_metrics) const {
  profiles_.clear();
  profiles_.insert(profiles_.end(), web_profiles().begin(),
                   web_profiles().end());
  if (pref_service_->GetBoolean(prefs::kAutofillWalletImportEnabled)) {
    profiles_.insert(
        profiles_.end(), server_profiles_.begin(), server_profiles_.end());
  }
  return profiles_;
}

std::vector<Suggestion> PersonalDataManager::GetSuggestionsForCards(
    const AutofillType& type,
    const base::string16& field_contents,
    const std::vector<CreditCard*>& cards_to_suggest) const {
  std::vector<Suggestion> suggestions;
  base::string16 field_contents_lower = base::i18n::ToLower(field_contents);
  for (const CreditCard* credit_card : cards_to_suggest) {
    // The value of the stored data for this field type in the |credit_card|.
    base::string16 creditcard_field_value =
        credit_card->GetInfo(type, app_locale_);
    if (creditcard_field_value.empty())
      continue;
    base::string16 creditcard_field_lower =
        base::i18n::ToLower(creditcard_field_value);

    bool prefix_matched_suggestion;
    if (IsValidSuggestionForFieldContents(
            creditcard_field_lower, field_contents_lower, type,
            credit_card->record_type() == CreditCard::MASKED_SERVER_CARD,
            &prefix_matched_suggestion)) {
      // Make a new suggestion.
      suggestions.push_back(Suggestion());
      Suggestion* suggestion = &suggestions.back();

      suggestion->value = credit_card->GetInfo(type, app_locale_);
      suggestion->icon = base::UTF8ToUTF16(credit_card->type());
      suggestion->backend_id = credit_card->guid();
      suggestion->match = prefix_matched_suggestion
                              ? Suggestion::PREFIX_MATCH
                              : Suggestion::SUBSTRING_MATCH;

      // If the value is the card number, the label is the expiration date.
      // Otherwise the label is the card number, or if that is empty the
      // cardholder name. The label should never repeat the value.
      if (type.GetStorableType() == CREDIT_CARD_NUMBER) {
        suggestion->value = credit_card->TypeAndLastFourDigits();
        suggestion->label = credit_card->GetInfo(
            AutofillType(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR), app_locale_);
      } else if (credit_card->number().empty()) {
        if (type.GetStorableType() != CREDIT_CARD_NAME_FULL) {
          suggestion->label = credit_card->GetInfo(
              AutofillType(CREDIT_CARD_NAME_FULL), app_locale_);
        }
      } else {
#if defined(OS_ANDROID)
        // Since Android places the label on its own row, there's more
        // horizontal
        // space to work with. Show "Amex - 1234" rather than desktop's "*1234".
        suggestion->label = credit_card->TypeAndLastFourDigits();
#else
        suggestion->label = base::ASCIIToUTF16("*");
        suggestion->label.append(credit_card->LastFourDigits());
#endif
      }
    }
  }

  // Prefix matches should precede other token matches.
  if (IsFeatureSubstringMatchEnabled()) {
    std::stable_sort(suggestions.begin(), suggestions.end(),
                     [](const Suggestion& a, const Suggestion& b) {
                       return a.match < b.match;
                     });
  }

  return suggestions;
}

void PersonalDataManager::ApplyProfileUseDatesFix() {
  // Don't run if the fix has already been applied.
  if (pref_service_->GetBoolean(prefs::kAutofillProfileUseDatesFixed))
    return;

  std::vector<AutofillProfile> profiles;
  bool has_changed_data = false;
  for (AutofillProfile* profile : web_profiles()) {
    if (profile->use_date() == base::Time()) {
      profile->set_use_date(base::Time::Now() - base::TimeDelta::FromDays(14));
      has_changed_data = true;
    }
    profiles.push_back(*profile);
  }

  // Set the pref so that this fix is never run again.
  pref_service_->SetBoolean(prefs::kAutofillProfileUseDatesFixed, true);

  if (has_changed_data)
    SetProfiles(&profiles);
}

bool PersonalDataManager::ApplyDedupingRoutine() {
  if (!is_autofill_profile_dedupe_pending_)
    return false;

  DCHECK(IsAutofillProfileCleanupEnabled());
  is_autofill_profile_dedupe_pending_ = false;

  // Check if the deduping routine has already been run on this major version.
  int current_major_version = atoi(version_info::GetVersionNumber().c_str());
  if (pref_service_->GetInteger(prefs::kAutofillLastVersionDeduped) >=
      current_major_version) {
    DVLOG(1)
        << "Autofill profile de-duplication already performed for this version";
    return false;
  }

  DVLOG(1) << "Starting autofill profile de-duplication.";
  std::vector<AutofillProfile*> existing_profiles = web_profiles_.get();
  std::unordered_set<AutofillProfile*> profiles_to_delete;
  profiles_to_delete.reserve(existing_profiles.size());

  DedupeProfiles(&existing_profiles, &profiles_to_delete);

  // Apply the changes to the database.
  for (AutofillProfile* profile : existing_profiles) {
    // If the profile was set to be deleted, remove it from the database.
    if (profiles_to_delete.count(profile)) {
      database_->RemoveAutofillProfile(profile->guid());
    } else {
      // Otherwise, update the profile in the database.
      database_->UpdateAutofillProfile(*profile);
    }
  }

  // Set the pref to the current major version.
  pref_service_->SetInteger(prefs::kAutofillLastVersionDeduped,
                            current_major_version);

  // Refresh the local cache and send notifications to observers.
  Refresh();

  return true;
}

void PersonalDataManager::DedupeProfiles(
    std::vector<AutofillProfile*>* existing_profiles,
    std::unordered_set<AutofillProfile*>* profiles_to_delete) {
  AutofillMetrics::LogNumberOfProfilesConsideredForDedupe(
      existing_profiles->size());

  // Sort the profiles by frecency with all the verified profiles at the end.
  // That way the most relevant profiles will get merged into the less relevant
  // profiles, which keeps the syntax of the most relevant profiles data.
  // Verified profiles are put at the end because they do not merge into other
  // profiles, so the loop can be stopped when we reach those. However they need
  // to be in the vector because an unverified profile trying to merge into a
  // similar verified profile will be discarded.
  base::Time comparison_time = base::Time::Now();
  std::sort(existing_profiles->begin(), existing_profiles->end(),
            [comparison_time](const AutofillDataModel* a,
                              const AutofillDataModel* b) {
              if (a->IsVerified() != b->IsVerified())
                return !a->IsVerified();
              return a->CompareFrecency(b, comparison_time);
            });

  AutofillProfileComparator comparator(app_locale_);

  for (size_t i = 0; i < existing_profiles->size(); ++i) {
    AutofillProfile* profile_to_merge = (*existing_profiles)[i];

    // If the profile was set to be deleted, skip it. It has already been
    // merged into another profile.
    if (profiles_to_delete->count(profile_to_merge))
      continue;

    // If we have reached the verified profiles, stop trying to merge. Verified
    // profiles do not get merged.
    if (profile_to_merge->IsVerified())
      break;

    // If we have not reached the last profile, try to merge |profile_to_merge|
    // with all the less relevant |existing_profiles|.
    for (size_t j = i + 1; j < existing_profiles->size(); ++j) {
      AutofillProfile* existing_profile = (*existing_profiles)[j];

      // Don't try to merge a profile that was already set for deletion.
      if (profiles_to_delete->count(existing_profile))
        continue;

      // Move on if the profiles are not mergeable.
      if (!comparator.AreMergeable(*existing_profile, *profile_to_merge))
        continue;

      // The profiles are found to be mergeable. Attempt to update the existing
      // profile. This returns true if the merge was successful, or if the
      // merge would have been successful but the existing profile IsVerified()
      // and will not accept updates from profile_to_merge.
      if (existing_profile->SaveAdditionalInfo(*profile_to_merge,
                                               app_locale_)) {
        // Since |profile_to_merge| was a duplicate of |existing_profile|
        // and was merged sucessfully, it can now be deleted.
        profiles_to_delete->insert(profile_to_merge);

        // Now try to merge the new resulting profile with the rest of the
        // existing profiles.
        profile_to_merge = existing_profile;

        // Verified profiles do not get merged. Save some time by not
        // trying.
        if (profile_to_merge->IsVerified())
          break;
      }
    }
  }
  AutofillMetrics::LogNumberOfProfilesRemovedDuringDedupe(
      profiles_to_delete->size());
}

}  // namespace autofill
