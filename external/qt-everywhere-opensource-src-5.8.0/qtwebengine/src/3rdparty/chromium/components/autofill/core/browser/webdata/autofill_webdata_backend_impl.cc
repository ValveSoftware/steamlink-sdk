// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_webdata_backend_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/webdata/common/web_database_backend.h"

using base::Bind;
using base::Time;

namespace autofill {

AutofillWebDataBackendImpl::AutofillWebDataBackendImpl(
    scoped_refptr<WebDatabaseBackend> web_database_backend,
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread,
    scoped_refptr<base::SingleThreadTaskRunner> db_thread,
    const base::Closure& on_changed_callback,
    const base::Callback<void(syncer::ModelType)>& on_sync_started_callback)
    : base::RefCountedDeleteOnMessageLoop<AutofillWebDataBackendImpl>(
          db_thread),
      ui_thread_(ui_thread),
      db_thread_(db_thread),
      web_database_backend_(web_database_backend),
      on_changed_callback_(on_changed_callback),
      on_sync_started_callback_(on_sync_started_callback) {
}

void AutofillWebDataBackendImpl::AddObserver(
    AutofillWebDataServiceObserverOnDBThread* observer) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  db_observer_list_.AddObserver(observer);
}

void AutofillWebDataBackendImpl::RemoveObserver(
    AutofillWebDataServiceObserverOnDBThread* observer) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  db_observer_list_.RemoveObserver(observer);
}

AutofillWebDataBackendImpl::~AutofillWebDataBackendImpl() {
  DCHECK(!user_data_.get());  // Forgot to call ResetUserData?
}

WebDatabase* AutofillWebDataBackendImpl::GetDatabase() {
  DCHECK(db_thread_->BelongsToCurrentThread());
  return web_database_backend_->database();
}

void AutofillWebDataBackendImpl::RemoveExpiredFormElements() {
  web_database_backend_->ExecuteWriteTask(
      Bind(&AutofillWebDataBackendImpl::RemoveExpiredFormElementsImpl,
           this));
}

void AutofillWebDataBackendImpl::NotifyOfMultipleAutofillChanges() {
  DCHECK(db_thread_->BelongsToCurrentThread());

  // DB thread notification.
  FOR_EACH_OBSERVER(AutofillWebDataServiceObserverOnDBThread, db_observer_list_,
                    AutofillMultipleChanged());

  // UI thread notification.
  ui_thread_->PostTask(FROM_HERE, on_changed_callback_);
}

void AutofillWebDataBackendImpl::NotifyThatSyncHasStarted(
    syncer::ModelType model_type) {
  DCHECK(db_thread_->BelongsToCurrentThread());

  if (on_sync_started_callback_.is_null())
    return;

  // UI thread notification.
  ui_thread_->PostTask(FROM_HERE,
                       base::Bind(on_sync_started_callback_, model_type));
}

base::SupportsUserData* AutofillWebDataBackendImpl::GetDBUserData() {
  DCHECK(db_thread_->BelongsToCurrentThread());
  if (!user_data_)
    user_data_.reset(new SupportsUserDataAggregatable());
  return user_data_.get();
}

void AutofillWebDataBackendImpl::ResetUserData() {
  user_data_.reset();
}

WebDatabase::State AutofillWebDataBackendImpl::AddFormElements(
    const std::vector<FormFieldData>& fields, WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  AutofillChangeList changes;
  if (!AutofillTable::FromWebDatabase(db)->AddFormFieldValues(
        fields, &changes)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  // Post the notifications including the list of affected keys.
  // This is sent here so that work resulting from this notification will be
  // done on the DB thread, and not the UI thread.
  FOR_EACH_OBSERVER(AutofillWebDataServiceObserverOnDBThread,
                    db_observer_list_,
                    AutofillEntriesChanged(changes));

  return WebDatabase::COMMIT_NEEDED;
}

std::unique_ptr<WDTypedResult>
AutofillWebDataBackendImpl::GetFormValuesForElementName(
    const base::string16& name,
    const base::string16& prefix,
    int limit,
    WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  std::vector<base::string16> values;
  AutofillTable::FromWebDatabase(db)->GetFormValuesForElementName(
      name, prefix, &values, limit);
  return std::unique_ptr<WDTypedResult>(
      new WDResult<std::vector<base::string16>>(AUTOFILL_VALUE_RESULT, values));
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveFormElementsAddedBetween(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  AutofillChangeList changes;

  if (AutofillTable::FromWebDatabase(db)->RemoveFormElementsAddedBetween(
          delete_begin, delete_end, &changes)) {
    if (!changes.empty()) {
      // Post the notifications including the list of affected keys.
      // This is sent here so that work resulting from this notification
      // will be done on the DB thread, and not the UI thread.
      FOR_EACH_OBSERVER(AutofillWebDataServiceObserverOnDBThread,
                        db_observer_list_,
                        AutofillEntriesChanged(changes));
    }
    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveFormValueForElementName(
    const base::string16& name, const base::string16& value, WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());

  if (AutofillTable::FromWebDatabase(db)->RemoveFormElement(name, value)) {
    AutofillChangeList changes;
    changes.push_back(
        AutofillChange(AutofillChange::REMOVE, AutofillKey(name, value)));

    // Post the notifications including the list of affected keys.
    FOR_EACH_OBSERVER(AutofillWebDataServiceObserverOnDBThread,
                      db_observer_list_,
                      AutofillEntriesChanged(changes));

    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::AddAutofillProfile(
    const AutofillProfile& profile, WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  if (!AutofillTable::FromWebDatabase(db)->AddAutofillProfile(profile)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  // Send GUID-based notification.
  AutofillProfileChange change(
      AutofillProfileChange::ADD, profile.guid(), &profile);
  FOR_EACH_OBSERVER(AutofillWebDataServiceObserverOnDBThread,
                    db_observer_list_,
                    AutofillProfileChanged(change));

  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateAutofillProfile(
    const AutofillProfile& profile, WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  // Only perform the update if the profile exists.  It is currently
  // valid to try to update a missing profile.  We simply drop the write and
  // the caller will detect this on the next refresh.
  std::unique_ptr<AutofillProfile> original_profile =
      AutofillTable::FromWebDatabase(db)->GetAutofillProfile(profile.guid());
  if (!original_profile) {
    return WebDatabase::COMMIT_NOT_NEEDED;
  }
  if (!AutofillTable::FromWebDatabase(db)->UpdateAutofillProfile(profile)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NEEDED;
  }

  // Send GUID-based notification.
  AutofillProfileChange change(
      AutofillProfileChange::UPDATE, profile.guid(), &profile);
  FOR_EACH_OBSERVER(AutofillWebDataServiceObserverOnDBThread,
                    db_observer_list_,
                    AutofillProfileChanged(change));

  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveAutofillProfile(
    const std::string& guid, WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  std::unique_ptr<AutofillProfile> profile =
      AutofillTable::FromWebDatabase(db)->GetAutofillProfile(guid);
  if (!profile) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  if (!AutofillTable::FromWebDatabase(db)->RemoveAutofillProfile(guid)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  // Send GUID-based notification.
  AutofillProfileChange change(AutofillProfileChange::REMOVE, guid, NULL);
  FOR_EACH_OBSERVER(AutofillWebDataServiceObserverOnDBThread,
                    db_observer_list_,
                    AutofillProfileChanged(change));

  return WebDatabase::COMMIT_NEEDED;
}

std::unique_ptr<WDTypedResult> AutofillWebDataBackendImpl::GetAutofillProfiles(
    WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  std::vector<AutofillProfile*> profiles;
  AutofillTable::FromWebDatabase(db)->GetAutofillProfiles(&profiles);
  return std::unique_ptr<WDTypedResult>(
      new WDDestroyableResult<std::vector<AutofillProfile*>>(
          AUTOFILL_PROFILES_RESULT, profiles,
          base::Bind(&AutofillWebDataBackendImpl::DestroyAutofillProfileResult,
                     base::Unretained(this))));
}

std::unique_ptr<WDTypedResult> AutofillWebDataBackendImpl::GetServerProfiles(
    WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  std::vector<AutofillProfile*> profiles;
  AutofillTable::FromWebDatabase(db)->GetServerProfiles(&profiles);
  return std::unique_ptr<WDTypedResult>(
      new WDDestroyableResult<std::vector<AutofillProfile*>>(
          AUTOFILL_PROFILES_RESULT, profiles,
          base::Bind(&AutofillWebDataBackendImpl::DestroyAutofillProfileResult,
                     base::Unretained(this))));
}

std::unique_ptr<WDTypedResult>
AutofillWebDataBackendImpl::GetCountOfValuesContainedBetween(
    const base::Time& begin,
    const base::Time& end,
    WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  int value = AutofillTable::FromWebDatabase(db)
      ->GetCountOfValuesContainedBetween(begin, end);
  return std::unique_ptr<WDTypedResult>(
      new WDResult<int>(AUTOFILL_VALUE_RESULT, value));
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateAutofillEntries(
    const std::vector<AutofillEntry>& autofill_entries,
    WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  if (!AutofillTable::FromWebDatabase(db)
           ->UpdateAutofillEntries(autofill_entries))
    return WebDatabase::COMMIT_NOT_NEEDED;

  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::AddCreditCard(
    const CreditCard& credit_card, WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  if (!AutofillTable::FromWebDatabase(db)->AddCreditCard(credit_card)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  FOR_EACH_OBSERVER(
      AutofillWebDataServiceObserverOnDBThread, db_observer_list_,
      CreditCardChanged(CreditCardChange(CreditCardChange::ADD,
                                         credit_card.guid(), &credit_card)));
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateCreditCard(
    const CreditCard& credit_card, WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  // It is currently valid to try to update a missing profile.  We simply drop
  // the write and the caller will detect this on the next refresh.
  std::unique_ptr<CreditCard> original_credit_card =
      AutofillTable::FromWebDatabase(db)->GetCreditCard(credit_card.guid());
  if (!original_credit_card)
    return WebDatabase::COMMIT_NOT_NEEDED;

  if (!AutofillTable::FromWebDatabase(db)->UpdateCreditCard(credit_card)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  FOR_EACH_OBSERVER(
      AutofillWebDataServiceObserverOnDBThread, db_observer_list_,
      CreditCardChanged(CreditCardChange(CreditCardChange::UPDATE,
                                         credit_card.guid(), &credit_card)));
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveCreditCard(
    const std::string& guid, WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  if (!AutofillTable::FromWebDatabase(db)->RemoveCreditCard(guid)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  FOR_EACH_OBSERVER(AutofillWebDataServiceObserverOnDBThread, db_observer_list_,
                    CreditCardChanged(CreditCardChange(CreditCardChange::REMOVE,
                                                       guid, nullptr)));
  return WebDatabase::COMMIT_NEEDED;
}

std::unique_ptr<WDTypedResult> AutofillWebDataBackendImpl::GetCreditCards(
    WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  std::vector<CreditCard*> credit_cards;
  AutofillTable::FromWebDatabase(db)->GetCreditCards(&credit_cards);
  return std::unique_ptr<WDTypedResult>(
      new WDDestroyableResult<std::vector<CreditCard*>>(
          AUTOFILL_CREDITCARDS_RESULT, credit_cards,
          base::Bind(
              &AutofillWebDataBackendImpl::DestroyAutofillCreditCardResult,
              base::Unretained(this))));
}

std::unique_ptr<WDTypedResult> AutofillWebDataBackendImpl::GetServerCreditCards(
    WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  std::vector<CreditCard*> credit_cards;
  AutofillTable::FromWebDatabase(db)->GetServerCreditCards(&credit_cards);
  return std::unique_ptr<WDTypedResult>(
      new WDDestroyableResult<std::vector<CreditCard*>>(
          AUTOFILL_CREDITCARDS_RESULT, credit_cards,
          base::Bind(
              &AutofillWebDataBackendImpl::DestroyAutofillCreditCardResult,
              base::Unretained(this))));
}

WebDatabase::State AutofillWebDataBackendImpl::UnmaskServerCreditCard(
    const CreditCard& card,
    const base::string16& full_number,
    WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  if (AutofillTable::FromWebDatabase(db)->UnmaskServerCreditCard(
          card, full_number))
    return WebDatabase::COMMIT_NEEDED;
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State
    AutofillWebDataBackendImpl::MaskServerCreditCard(
        const std::string& id,
        WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  if (AutofillTable::FromWebDatabase(db)->MaskServerCreditCard(id))
    return WebDatabase::COMMIT_NEEDED;
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateServerCardUsageStats(
    const CreditCard& card,
    WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  if (!AutofillTable::FromWebDatabase(db)->UpdateServerCardUsageStats(card))
    return WebDatabase::COMMIT_NOT_NEEDED;

  FOR_EACH_OBSERVER(AutofillWebDataServiceObserverOnDBThread, db_observer_list_,
                    CreditCardChanged(CreditCardChange(CreditCardChange::UPDATE,
                                                       card.guid(), &card)));

  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateServerAddressUsageStats(
    const AutofillProfile& profile,
    WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  if (!AutofillTable::FromWebDatabase(db)->UpdateServerAddressUsageStats(
          profile)) {
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  FOR_EACH_OBSERVER(
      AutofillWebDataServiceObserverOnDBThread, db_observer_list_,
      AutofillProfileChanged(AutofillProfileChange(
          AutofillProfileChange::UPDATE, profile.guid(), &profile)));

  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::UpdateServerCardBillingAddress(
    const CreditCard& card,
    WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  if (!AutofillTable::FromWebDatabase(db)->UpdateServerCardBillingAddress(
          card)) {
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  FOR_EACH_OBSERVER(AutofillWebDataServiceObserverOnDBThread, db_observer_list_,
                    CreditCardChanged(CreditCardChange(CreditCardChange::UPDATE,
                                                       card.guid(), &card)));

  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::ClearAllServerData(
    WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  if (AutofillTable::FromWebDatabase(db)->ClearAllServerData()) {
    NotifyOfMultipleAutofillChanges();
    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State
    AutofillWebDataBackendImpl::RemoveAutofillDataModifiedBetween(
        const base::Time& delete_begin,
        const base::Time& delete_end,
        WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  std::vector<std::string> profile_guids;
  std::vector<std::string> credit_card_guids;
  if (AutofillTable::FromWebDatabase(db)->RemoveAutofillDataModifiedBetween(
          delete_begin,
          delete_end,
          &profile_guids,
          &credit_card_guids)) {
    for (const std::string& guid : profile_guids) {
      FOR_EACH_OBSERVER(AutofillWebDataServiceObserverOnDBThread,
                        db_observer_list_,
                        AutofillProfileChanged(AutofillProfileChange(
                            AutofillProfileChange::REMOVE, guid, nullptr)));
    }
    for (const std::string& guid : credit_card_guids) {
      FOR_EACH_OBSERVER(AutofillWebDataServiceObserverOnDBThread,
                        db_observer_list_,
                        CreditCardChanged(CreditCardChange(
                            CreditCardChange::REMOVE, guid, nullptr)));
    }
    // Note: It is the caller's responsibility to post notifications for any
    // changes, e.g. by calling the Refresh() method of PersonalDataManager.
    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveOriginURLsModifiedBetween(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  ScopedVector<AutofillProfile> profiles;
  if (!AutofillTable::FromWebDatabase(db)->RemoveOriginURLsModifiedBetween(
          delete_begin, delete_end, &profiles)) {
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  for (const AutofillProfile* it : profiles) {
    AutofillProfileChange change(AutofillProfileChange::UPDATE, it->guid(), it);
    FOR_EACH_OBSERVER(AutofillWebDataServiceObserverOnDBThread,
                      db_observer_list_,
                      AutofillProfileChanged(change));
  }
  // Note: It is the caller's responsibility to post notifications for any
  // changes, e.g. by calling the Refresh() method of PersonalDataManager.
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State AutofillWebDataBackendImpl::RemoveExpiredFormElementsImpl(
    WebDatabase* db) {
  DCHECK(db_thread_->BelongsToCurrentThread());
  AutofillChangeList changes;

  if (AutofillTable::FromWebDatabase(db)->RemoveExpiredFormElements(&changes)) {
    if (!changes.empty()) {
      // Post the notifications including the list of affected keys.
      // This is sent here so that work resulting from this notification
      // will be done on the DB thread, and not the UI thread.
      FOR_EACH_OBSERVER(AutofillWebDataServiceObserverOnDBThread,
                        db_observer_list_,
                        AutofillEntriesChanged(changes));
    }
    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

void AutofillWebDataBackendImpl::DestroyAutofillProfileResult(
    const WDTypedResult* result) {
  DCHECK(result->GetType() == AUTOFILL_PROFILES_RESULT);
  const WDResult<std::vector<AutofillProfile*> >* r =
      static_cast<const WDResult<std::vector<AutofillProfile*> >*>(result);
  std::vector<AutofillProfile*> profiles = r->GetValue();
  STLDeleteElements(&profiles);
}

void AutofillWebDataBackendImpl::DestroyAutofillCreditCardResult(
      const WDTypedResult* result) {
  DCHECK(result->GetType() == AUTOFILL_CREDITCARDS_RESULT);
  const WDResult<std::vector<CreditCard*> >* r =
      static_cast<const WDResult<std::vector<CreditCard*> >*>(result);

  std::vector<CreditCard*> credit_cards = r->GetValue();
  STLDeleteElements(&credit_cards);
}

}  // namespace autofill
