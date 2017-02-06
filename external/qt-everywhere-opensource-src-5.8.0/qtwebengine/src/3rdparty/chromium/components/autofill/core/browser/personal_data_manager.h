// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_H_

#include <memory>
#include <set>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/suggestion.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_member.h"
#include "components/webdata/common/web_data_service_consumer.h"

class AccountTrackerService;
class Browser;
class PrefService;
class RemoveAutofillTester;
class SigninManagerBase;

#if defined(OS_IOS)
// TODO(crbug.com/513344): Remove this once Chrome on iOS is unforked.
class PersonalDataManagerFactory;
#endif

namespace sync_driver {
class SyncService;
}

namespace autofill {
class AutofillInteractiveTest;
class AutofillTest;
class FormStructure;
class PersonalDataManagerObserver;
class PersonalDataManagerFactory;
}  // namespace autofill

namespace autofill_helper {
void SetProfiles(int, std::vector<autofill::AutofillProfile>*);
void SetCreditCards(int, std::vector<autofill::CreditCard>*);
}  // namespace autofill_helper

namespace autofill {

extern const char kFrecencyFieldTrialName[];
extern const char kFrecencyFieldTrialStateEnabled[];
extern const char kFrecencyFieldTrialLimitParam[];

// Handles loading and saving Autofill profile information to the web database.
// This class also stores the profiles loaded from the database for use during
// Autofill.
class PersonalDataManager : public KeyedService,
                            public WebDataServiceConsumer,
                            public AutofillWebDataServiceObserverOnUIThread {
 public:
  explicit PersonalDataManager(const std::string& app_locale);
  ~PersonalDataManager() override;

  // Kicks off asynchronous loading of profiles and credit cards.
  // |pref_service| must outlive this instance.  |is_off_the_record| informs
  // this instance whether the user is currently operating in an off-the-record
  // context.
  void Init(scoped_refptr<AutofillWebDataService> database,
            PrefService* pref_service,
            AccountTrackerService* account_tracker,
            SigninManagerBase* signin_manager,
            bool is_off_the_record);

  // Called once the sync service is known to be instantiated. Note that it may
  // not be started, but it's preferences can be queried.
  void OnSyncServiceInitialized(sync_driver::SyncService* sync_service);

  // WebDataServiceConsumer:
  void OnWebDataServiceRequestDone(WebDataServiceBase::Handle h,
                                   const WDTypedResult* result) override;

  // AutofillWebDataServiceObserverOnUIThread:
  void AutofillMultipleChanged() override;
  void SyncStarted(syncer::ModelType model_type) override;

  // Adds a listener to be notified of PersonalDataManager events.
  virtual void AddObserver(PersonalDataManagerObserver* observer);

  // Removes |observer| as an observer of this PersonalDataManager.
  virtual void RemoveObserver(PersonalDataManagerObserver* observer);

  // Scans the given |form| for importable Autofill data. If the form includes
  // sufficient address data for a new profile, it is immediately imported. If
  // the form includes sufficient credit card data for a new credit card, it is
  // stored into |imported_credit_card| so that we can prompt the user whether
  // to save this data. If the form contains credit card data already present in
  // a local credit card entry *and* |should_return_local_card| is true, the
  // data is stored into |imported_credit_card| so that we can prompt the user
  // whether to upload it.
  // Returns |true| if sufficient address or credit card data was found.
  bool ImportFormData(const FormStructure& form,
                      bool should_return_local_card,
                      std::unique_ptr<CreditCard>* imported_credit_card);

  // Called to indicate |data_model| was used (to fill in a form). Updates
  // the database accordingly. Can invalidate |data_model|, particularly if
  // it's a Mac address book entry.
  virtual void RecordUseOf(const AutofillDataModel& data_model);

  // Saves |imported_profile| to the WebDB if it exists. Returns the guid of
  // the new or updated profile, or the empty string if no profile was saved.
  virtual std::string SaveImportedProfile(
      const AutofillProfile& imported_profile);

  // Saves |imported_credit_card| to the WebDB if it exists. Returns the guid of
  // of the new or updated card, or the empty string if no card was saved.
  virtual std::string SaveImportedCreditCard(
      const CreditCard& imported_credit_card);

  // Adds |profile| to the web database.
  void AddProfile(const AutofillProfile& profile);

  // Updates |profile| which already exists in the web database.
  void UpdateProfile(const AutofillProfile& profile);

  // Removes the profile or credit card represented by |guid|.
  virtual void RemoveByGUID(const std::string& guid);

  // Returns the profile with the specified |guid|, or NULL if there is no
  // profile with the specified |guid|. Both web and auxiliary profiles may
  // be returned.
  AutofillProfile* GetProfileByGUID(const std::string& guid);

  // Adds |credit_card| to the web database.
  void AddCreditCard(const CreditCard& credit_card);

  // Updates |credit_card| which already exists in the web database. This
  // can only be used on local credit cards.
  virtual void UpdateCreditCard(const CreditCard& credit_card);

  // Update a server card. Only the full number and masked/unmasked
  // status can be changed. Looks up the card by server ID.
  virtual void UpdateServerCreditCard(const CreditCard& credit_card);

  // Updates the billing address for the server |credit_card|. Looks up the card
  // by GUID.
  void UpdateServerCardBillingAddress(const CreditCard& credit_card);

  // Resets the card for |guid| to the masked state.
  void ResetFullServerCard(const std::string& guid);

  // Resets all unmasked cards to the masked state.
  void ResetFullServerCards();

  // Deletes all server profiles and cards (both masked and unmasked).
  void ClearAllServerData();

  // Sets a server credit card for test.
  void AddServerCreditCardForTest(std::unique_ptr<CreditCard> credit_card);

  // Returns the credit card with the specified |guid|, or NULL if there is
  // no credit card with the specified |guid|.
  CreditCard* GetCreditCardByGUID(const std::string& guid);

  // Gets the field types availabe in the stored address and credit card data.
  void GetNonEmptyTypes(ServerFieldTypeSet* non_empty_types);

  // Returns true if the credit card information is stored with a password.
  bool HasPassword();

  // Returns whether the personal data has been loaded from the web database.
  virtual bool IsDataLoaded() const;

  // This PersonalDataManager owns these profiles and credit cards.  Their
  // lifetime is until the web database is updated with new profile and credit
  // card information, respectively.  |GetProfiles()| returns both web and
  // auxiliary profiles.  |web_profiles()| returns only web profiles.
  virtual const std::vector<AutofillProfile*>& GetProfiles() const;
  virtual const std::vector<AutofillProfile*>& web_profiles() const;
  // Returns just LOCAL_CARD cards.
  virtual const std::vector<CreditCard*>& GetLocalCreditCards() const;
  // Returns all credit cards, server and local.
  virtual const std::vector<CreditCard*>& GetCreditCards() const;

  // Returns true if there is some data synced from Wallet.
  bool HasServerData() const;

  // Returns the profiles to suggest to the user, ordered by frecency.
  const std::vector<AutofillProfile*> GetProfilesToSuggest() const;

  // Loads profiles that can suggest data for |type|. |field_contents| is the
  // part the user has already typed. |field_is_autofilled| is true if the field
  // has already been autofilled. |other_field_types| represents the rest of
  // form.
  std::vector<Suggestion> GetProfileSuggestions(
      const AutofillType& type,
      const base::string16& field_contents,
      bool field_is_autofilled,
      const std::vector<ServerFieldType>& other_field_types);

  // Returns the credit cards to suggest to the user. Those have been deduped
  // and ordered by frecency with the expired cards put at the end of the
  // vector.
  const std::vector<CreditCard*> GetCreditCardsToSuggest() const;

  // Gets credit cards that can suggest data for |type|. See
  // GetProfileSuggestions for argument descriptions. The variant in each
  // GUID pair should be ignored.
  std::vector<Suggestion> GetCreditCardSuggestions(
      const AutofillType& type,
      const base::string16& field_contents);

  // Re-loads profiles and credit cards from the WebDatabase asynchronously.
  // In the general case, this is a no-op and will re-create the same
  // in-memory model as existed prior to the call.  If any change occurred to
  // profiles in the WebDatabase directly, as is the case if the browser sync
  // engine processed a change from the cloud, we will learn of these as a
  // result of this call.
  //
  // Also see SetProfile for more details.
  virtual void Refresh();

  const std::string& app_locale() const { return app_locale_; }

  // Checks suitability of |profile| for adding to the user's set of profiles.
  static bool IsValidLearnableProfile(const AutofillProfile& profile,
                                      const std::string& app_locale);

  // Merges |new_profile| into one of the |existing_profiles| if possible;
  // otherwise appends |new_profile| to the end of that list. Fills
  // |merged_profiles| with the result. Returns the |guid| of the new or updated
  // profile.
  std::string MergeProfile(const AutofillProfile& new_profile,
                           std::vector<AutofillProfile*> existing_profiles,
                           const std::string& app_locale,
                           std::vector<AutofillProfile>* merged_profiles);

  // Returns true if |country_code| is a country that the user is likely to
  // be associated with the user. More concretely, it checks if there are any
  // addresses with this country or if the user's system timezone is in the
  // given country.
  virtual bool IsCountryOfInterest(const std::string& country_code) const;

  // Returns our best guess for the country a user is likely to use when
  // inputting a new address. The value is calculated once and cached, so it
  // will only update when Chrome is restarted.
  virtual const std::string& GetDefaultCountryCodeForNewAddress() const;

  // De-dupe credit card to suggest. Full server cards are prefered over their
  // local duplicates, and local cards are preferred over their masked server
  // card duplicate.
  static void DedupeCreditCardToSuggest(
      std::list<CreditCard*>* cards_to_suggest);

  // Notifies test observers that personal data has changed.
  void NotifyPersonalDataChangedForTest() {
    NotifyPersonalDataChanged();
  }

 protected:
  // Only PersonalDataManagerFactory and certain tests can create instances of
  // PersonalDataManager.
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, FirstMiddleLast);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, AutofillIsEnabledAtStartup);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           DedupeProfiles_ProfilesToDelete);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest, ApplyProfileUseDatesFix);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyProfileUseDatesFix_NotAppliedTwice);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_MergedProfileValues);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_VerifiedProfileFirst);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_VerifiedProfileLast);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_MultipleVerifiedProfiles);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_FeatureDisabled);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_OncePerVersion);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_MultipleDedupes);
  friend class autofill::AutofillInteractiveTest;
  friend class autofill::AutofillTest;
  friend class autofill::PersonalDataManagerFactory;
  friend class PersonalDataManagerTest;
#if defined(OS_IOS)
  // TODO(crbug.com/513344): Remove this once Chrome on iOS is unforked.
  friend class ::PersonalDataManagerFactory;
#endif
  friend class ProfileSyncServiceAutofillTest;
  friend class ::RemoveAutofillTester;
  friend std::default_delete<PersonalDataManager>;
  friend void autofill_helper::SetProfiles(
      int, std::vector<autofill::AutofillProfile>*);
  friend void autofill_helper::SetCreditCards(
      int, std::vector<autofill::CreditCard>*);
  friend void SetTestProfiles(
      Browser* browser, std::vector<AutofillProfile>* profiles);

  // Sets |web_profiles_| to the contents of |profiles| and updates the web
  // database by adding, updating and removing profiles.
  // The relationship between this and Refresh is subtle.
  // A call to |SetProfiles| could include out-of-date data that may conflict
  // if we didn't refresh-to-latest before an Autofill window was opened for
  // editing. |SetProfiles| is implemented to make a "best effort" to apply the
  // changes, but in extremely rare edge cases it is possible not all of the
  // updates in |profiles| make it to the DB.  This is why SetProfiles will
  // invoke Refresh after finishing, to ensure we get into a
  // consistent state.  See Refresh for details.
  virtual void SetProfiles(std::vector<AutofillProfile>* profiles);

  // Sets |credit_cards_| to the contents of |credit_cards| and updates the web
  // database by adding, updating and removing credit cards.
  void SetCreditCards(std::vector<CreditCard>* credit_cards);

  // Loads the saved profiles from the web database.
  virtual void LoadProfiles();

  // Loads the saved credit cards from the web database.
  virtual void LoadCreditCards();

  // Cancels a pending query to the web database.  |handle| is a pointer to the
  // query handle.
  void CancelPendingQuery(WebDataServiceBase::Handle* handle);

  // Notifies observers that personal data has changed.
  void NotifyPersonalDataChanged();

  // The first time this is called, logs an UMA metric for the number of
  // profiles the user has. On subsequent calls, does nothing.
  void LogProfileCount() const;

  // The first time this is called, logs an UMA metric for the number of local
  // credit cards the user has. On subsequent calls, does nothing.
  void LogLocalCreditCardCount() const;

  // Returns the value of the AutofillEnabled pref.
  virtual bool IsAutofillEnabled() const;

  // Overrideable for testing.
  virtual std::string CountryCodeForCurrentTimezone() const;

  // Sets which PrefService to use and observe. |pref_service| is not owned by
  // this class and must outlive |this|.
  void SetPrefService(PrefService* pref_service);

  void set_database(scoped_refptr<AutofillWebDataService> database) {
    database_ = database;
  }

  void set_account_tracker(AccountTrackerService* account_tracker) {
    account_tracker_ = account_tracker;
  }

  void set_signin_manager(SigninManagerBase* signin_manager) {
    signin_manager_ = signin_manager;
  }

  // The backing database that this PersonalDataManager uses.
  scoped_refptr<AutofillWebDataService> database_;

  // True if personal data has been loaded from the web database.
  bool is_data_loaded_;

  // The loaded web profiles. These are constructed from entries on web pages
  // and from manually editing in the settings.
  ScopedVector<AutofillProfile> web_profiles_;

  // Profiles read from the user's account stored on the server.
  mutable ScopedVector<AutofillProfile> server_profiles_;

  // Storage for web profiles.  Contents are weak references.  Lifetime managed
  // by |web_profiles_|.
  mutable std::vector<AutofillProfile*> profiles_;

  // Cached versions of the local and server credit cards.
  ScopedVector<CreditCard> local_credit_cards_;
  ScopedVector<CreditCard> server_credit_cards_;

  // A combination of local and server credit cards. The pointers are owned
  // by the local/sverver_credit_cards_ vectors.
  mutable std::vector<CreditCard*> credit_cards_;

  // When the manager makes a request from WebDataServiceBase, the database
  // is queried on another thread, we record the query handle until we
  // get called back.  We store handles for both profile and credit card queries
  // so they can be loaded at the same time.
  WebDataServiceBase::Handle pending_profiles_query_;
  WebDataServiceBase::Handle pending_server_profiles_query_;
  WebDataServiceBase::Handle pending_creditcards_query_;
  WebDataServiceBase::Handle pending_server_creditcards_query_;

  // The observers.
  base::ObserverList<PersonalDataManagerObserver> observers_;

 private:
  // Finds the country code that occurs most frequently among all profiles.
  // Prefers verified profiles over unverified ones.
  std::string MostCommonCountryCodeFromProfiles() const;

  // Called when the value of prefs::kAutofillEnabled changes.
  void EnabledPrefChanged();

  // Go through the |form| fields and attempt to extract and import valid
  // address profiles. Returns true on extraction success of at least one
  // profile. There are many reasons that extraction may fail (see
  // implementation).
  bool ImportAddressProfiles(const FormStructure& form);

  // Helper method for ImportAddressProfiles which only considers the fields for
  // a specified |section|.
  bool ImportAddressProfileForSection(const FormStructure& form,
                                      const std::string& section);

  // Go through the |form| fields and attempt to extract a new credit card in
  // |imported_credit_card|, or update an existing card.
  // |should_return_local_card| will indicate whether |imported_credit_card| is
  // filled even if an existing card was updated. Success is defined as having a
  // new card to import, or having merged with an existing card.
  bool ImportCreditCard(const FormStructure& form,
                        bool should_return_local_card,
                        std::unique_ptr<CreditCard>* imported_credit_card);

  // Functionally equivalent to GetProfiles(), but also records metrics if
  // |record_metrics| is true. Metrics should be recorded when the returned
  // profiles will be used to populate the fields shown in an Autofill popup.
  const std::vector<AutofillProfile*>& GetProfiles(
      bool record_metrics) const;

  // Returns credit card suggestions based on the |cards_to_suggest| and the
  // |type| and |field_contents| of the credit card field.
  std::vector<Suggestion> GetSuggestionsForCards(
      const AutofillType& type,
      const base::string16& field_contents,
      const std::vector<CreditCard*>& cards_to_suggest) const;

  // Runs the Autofill use date fix routine if it's never been done. Returns
  // whether the routine was run.
  void ApplyProfileUseDatesFix();

  // Applies the deduping routine once per major version if the feature is
  // enabled. Calls DedupeProfiles with the content of |web_profiles_| as a
  // parameter. Removes the profiles to delete from the database and updates the
  // others. Returns true if the routine was run.
  bool ApplyDedupingRoutine();

  // Goes through all the |existing_profiles| and merges all similar unverified
  // profiles together. Also discards unverified profiles that are similar to a
  // verified profile. All the profiles except the results of the merges will be
  // added to |profile_guids_to_delete|. This routine should be run once per
  // major version.
  //
  // This method should only be called by ApplyDedupingRoutine. It is split for
  // testing purposes.
  void DedupeProfiles(
      std::vector<AutofillProfile*>* existing_profiles,
      std::unordered_set<AutofillProfile*>* profile_guids_to_delete);

  const std::string app_locale_;

  // The default country code for new addresses.
  mutable std::string default_country_code_;

  // The PrefService that this instance uses. Must outlive this instance.
  PrefService* pref_service_;

  // The AccountTrackerService that this instance uses. Must outlive this
  // instance.
  AccountTrackerService* account_tracker_;

  // The signin manager that this instance uses. Must outlive this instance.
  SigninManagerBase* signin_manager_;

  // Whether the user is currently operating in an off-the-record context.
  // Default value is false.
  bool is_off_the_record_;

  // Whether we have already logged the number of profiles this session.
  mutable bool has_logged_profile_count_;

  // Whether we have already logged the number of local credit cards this
  // session.
  mutable bool has_logged_credit_card_count_;

  // An observer to listen for changes to prefs::kAutofillEnabled.
  std::unique_ptr<BooleanPrefMember> enabled_pref_;

  // An observer to listen for changes to prefs::kAutofillWalletImportEnabled.
  std::unique_ptr<BooleanPrefMember> wallet_enabled_pref_;

  // Set to true if autofill profile deduplication is enabled and needs to be
  // performed on the next data refresh.
  bool is_autofill_profile_dedupe_pending_ = false;

  DISALLOW_COPY_AND_ASSIGN(PersonalDataManager);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_H_
