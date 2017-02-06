// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_syncable_service.h"

#include <stddef.h>
#include <set>
#include <utility>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "sync/api/sync_error_factory.h"
#include "sync/protocol/sync.pb.h"

namespace autofill {

namespace {

void* UserDataKey() {
  // Use the address of a static so that COMDAT folding won't ever fold
  // with something else.
  static int user_data_key = 0;
  return reinterpret_cast<void*>(&user_data_key);
}

const char* CardTypeFromWalletCardType(
    sync_pb::WalletMaskedCreditCard::WalletCardType type) {
  switch (type) {
    case sync_pb::WalletMaskedCreditCard::AMEX:
      return kAmericanExpressCard;
    case sync_pb::WalletMaskedCreditCard::DISCOVER:
      return kDiscoverCard;
    case sync_pb::WalletMaskedCreditCard::JCB:
      return kJCBCard;
    case sync_pb::WalletMaskedCreditCard::MASTER_CARD:
      return kMasterCard;
    case sync_pb::WalletMaskedCreditCard::VISA:
      return kVisaCard;

    // These aren't supported by the client, so just declare a generic card.
    case sync_pb::WalletMaskedCreditCard::MAESTRO:
    case sync_pb::WalletMaskedCreditCard::SOLO:
    case sync_pb::WalletMaskedCreditCard::SWITCH:
    default:
      return kGenericCard;
  }
}

CreditCard::ServerStatus ServerToLocalStatus(
    sync_pb::WalletMaskedCreditCard::WalletCardStatus status) {
  switch (status) {
    case sync_pb::WalletMaskedCreditCard::VALID:
      return CreditCard::OK;
    case sync_pb::WalletMaskedCreditCard::EXPIRED:
    default:
      DCHECK_EQ(sync_pb::WalletMaskedCreditCard::EXPIRED, status);
      return CreditCard::EXPIRED;
  }
}

CreditCard CardFromSpecifics(const sync_pb::WalletMaskedCreditCard& card) {
  CreditCard result(CreditCard::MASKED_SERVER_CARD, card.id());
  result.SetNumber(base::UTF8ToUTF16(card.last_four()));
  result.SetServerStatus(ServerToLocalStatus(card.status()));
  result.SetTypeForMaskedCard(CardTypeFromWalletCardType(card.type()));
  result.SetRawInfo(CREDIT_CARD_NAME_FULL,
                    base::UTF8ToUTF16(card.name_on_card()));
  result.SetExpirationMonth(card.exp_month());
  result.SetExpirationYear(card.exp_year());
  return result;
}

AutofillProfile ProfileFromSpecifics(
    const sync_pb::WalletPostalAddress& address) {
  AutofillProfile profile(AutofillProfile::SERVER_PROFILE, std::string());

  // AutofillProfile stores multi-line addresses with newline separators.
  std::vector<std::string> street_address(address.street_address().begin(),
                                          address.street_address().end());
  profile.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS,
                     base::UTF8ToUTF16(base::JoinString(street_address, "\n")));

  profile.SetRawInfo(COMPANY_NAME, base::UTF8ToUTF16(address.company_name()));
  profile.SetRawInfo(ADDRESS_HOME_STATE,
                     base::UTF8ToUTF16(address.address_1()));
  profile.SetRawInfo(ADDRESS_HOME_CITY,
                     base::UTF8ToUTF16(address.address_2()));
  profile.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY,
                     base::UTF8ToUTF16(address.address_3()));
  // AutofillProfile doesn't support address_4 ("sub dependent locality").
  profile.SetRawInfo(ADDRESS_HOME_ZIP,
                     base::UTF8ToUTF16(address.postal_code()));
  profile.SetRawInfo(ADDRESS_HOME_SORTING_CODE,
                     base::UTF8ToUTF16(address.sorting_code()));
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY,
                     base::UTF8ToUTF16(address.country_code()));
  profile.set_language_code(address.language_code());

  // SetInfo instead of SetRawInfo so the constituent pieces will be parsed
  // for these data types.
  profile.SetInfo(AutofillType(NAME_FULL),
                  base::UTF8ToUTF16(address.recipient_name()),
                  profile.language_code());
  profile.SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER),
                  base::UTF8ToUTF16(address.phone_number()),
                  profile.language_code());

  profile.GenerateServerProfileIdentifier();

  return profile;
}

// Searches for CreditCards with identical server IDs and copies the billing
// address ID from the existing cards on disk into the new cards from server.
// The credit card's IDs do not change over time.
void CopyBillingAddressesFromDisk(AutofillTable* table,
                                  std::vector<CreditCard>* cards_from_server) {
  ScopedVector<CreditCard> cards_on_disk;
  table->GetServerCreditCards(&cards_on_disk.get());

  // The reasons behind brute-force search are explained in SetDataIfChanged.
  for (const CreditCard* saved_card : cards_on_disk) {
    for (CreditCard& server_card : *cards_from_server) {
      if (saved_card->server_id() == server_card.server_id()) {
        server_card.set_billing_address_id(saved_card->billing_address_id());
        break;
      }
    }
  }
}

// This function handles conditionally updating the AutofillTable with either
// a set of CreditCards or AutocompleteProfiles only when the existing data
// doesn't match.
//
// It's passed the getter and setter function on the AutofillTable for the
// corresponding data type, and expects the types to implement a Compare
// function. Note that the guid can't be used here as a key since these are
// generated locally and will be different each time, and the server ID can't
// be used because it's empty for addresses (though it could be used for credit
// cards if we wanted separate implementations).
//
// Returns true if anything changed. The previous number of items in the table
// (for sync tracking) will be placed into *prev_item_count.
template<class Data>
bool SetDataIfChanged(
    AutofillTable* table,
    const std::vector<Data>& data,
    bool (AutofillTable::*getter)(std::vector<Data*>*),
    void (AutofillTable::*setter)(const std::vector<Data>&),
    size_t* prev_item_count) {
  ScopedVector<Data> existing_data;
  (table->*getter)(&existing_data.get());
  *prev_item_count = existing_data.size();

  // If the user has a large number of addresses, don't bother verifying
  // anything changed and just rewrite the data.
  const size_t kTooBigToCheckThreshold = 8;

  bool difference_found;
  if (existing_data.size() != data.size() ||
      data.size() > kTooBigToCheckThreshold) {
    difference_found = true;
  } else {
    difference_found = false;

    // Implement brute-force searching. Address and card counts are typically
    // small, and comparing them is relatively expensive (many string
    // compares). A std::set only uses operator< requiring multiple calls to
    // check equality, giving 8 compares for 2 elements and 16 for 3. For these
    // set sizes, brute force O(n^2) is faster.
    for (const Data* cur_existing : existing_data) {
      bool found_match_for_cur_existing = false;
      for (const Data& cur_new : data) {
        if (cur_existing->Compare(cur_new) == 0) {
          found_match_for_cur_existing = true;
          break;
        }
      }
      if (!found_match_for_cur_existing) {
        difference_found = true;
        break;
      }
    }
  }

  if (difference_found) {
    (table->*setter)(data);
    return true;
  }
  return false;
}

}  // namespace

AutofillWalletSyncableService::AutofillWalletSyncableService(
    AutofillWebDataBackend* webdata_backend,
    const std::string& app_locale)
    : webdata_backend_(webdata_backend) {
}

AutofillWalletSyncableService::~AutofillWalletSyncableService() {
}

syncer::SyncMergeResult AutofillWalletSyncableService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
    std::unique_ptr<syncer::SyncErrorFactory> sync_error_factory) {
  DCHECK(thread_checker_.CalledOnValidThread());
  sync_processor_ = std::move(sync_processor);
  syncer::SyncMergeResult result = SetSyncData(initial_sync_data);
  if (webdata_backend_)
    webdata_backend_->NotifyThatSyncHasStarted(type);
  return result;
}

void AutofillWalletSyncableService::StopSyncing(syncer::ModelType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(type, syncer::AUTOFILL_WALLET_DATA);
  sync_processor_.reset();
}

syncer::SyncDataList AutofillWalletSyncableService::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  // This data type is never synced "up" so we don't need to implement this.
  syncer::SyncDataList current_data;
  return current_data;
}

syncer::SyncError AutofillWalletSyncableService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Don't bother handling incremental updates. Wallet data changes very rarely
  // and has few items. Instead, just get all the current data and save it.
  SetSyncData(sync_processor_->GetAllSyncData(syncer::AUTOFILL_WALLET_DATA));
  return syncer::SyncError();
}

// static
void AutofillWalletSyncableService::CreateForWebDataServiceAndBackend(
    AutofillWebDataService* web_data_service,
    AutofillWebDataBackend* webdata_backend,
    const std::string& app_locale) {
  web_data_service->GetDBUserData()->SetUserData(
      UserDataKey(),
      new AutofillWalletSyncableService(webdata_backend, app_locale));
}

// static
AutofillWalletSyncableService*
AutofillWalletSyncableService::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<AutofillWalletSyncableService*>(
      web_data_service->GetDBUserData()->GetUserData(UserDataKey()));
}

void AutofillWalletSyncableService::InjectStartSyncFlare(
    const syncer::SyncableService::StartSyncFlare& flare) {
  flare_ = flare;
}

syncer::SyncMergeResult AutofillWalletSyncableService::SetSyncData(
    const syncer::SyncDataList& data_list) {
  std::vector<CreditCard> wallet_cards;
  std::vector<AutofillProfile> wallet_addresses;

  for (const syncer::SyncData& data : data_list) {
    DCHECK_EQ(syncer::AUTOFILL_WALLET_DATA, data.GetDataType());
    const sync_pb::AutofillWalletSpecifics& autofill_specifics =
        data.GetSpecifics().autofill_wallet();
    if (autofill_specifics.type() ==
        sync_pb::AutofillWalletSpecifics::MASKED_CREDIT_CARD) {
      wallet_cards.push_back(
          CardFromSpecifics(autofill_specifics.masked_card()));
    } else {
      DCHECK_EQ(sync_pb::AutofillWalletSpecifics::POSTAL_ADDRESS,
                autofill_specifics.type());
      wallet_addresses.push_back(
          ProfileFromSpecifics(autofill_specifics.address()));
    }
  }

  // Users can set billing address of the server credit card locally, but that
  // information does not propagate to either Chrome Sync or Google Payments
  // server. To preserve user's preferred billing address, copy the billing
  // addresses from disk into |wallet_cards|.
  AutofillTable* table =
      AutofillTable::FromWebDatabase(webdata_backend_->GetDatabase());
  CopyBillingAddressesFromDisk(table, &wallet_cards);

  // In the common case, the database won't have changed. Committing an update
  // to the database will require at least one DB page write and will schedule
  // a fsync. To avoid this I/O, it should be more efficient to do a read and
  // only do the writes if something changed.
  size_t prev_card_count = 0;
  size_t prev_address_count = 0;
  bool changed_cards = SetDataIfChanged(
      table, wallet_cards,
      &AutofillTable::GetServerCreditCards,
      &AutofillTable::SetServerCreditCards,
      &prev_card_count);
  bool changed_addresses = SetDataIfChanged(
      table, wallet_addresses, &AutofillTable::GetServerProfiles,
      &AutofillTable::SetServerProfiles, &prev_address_count);

  syncer::SyncMergeResult merge_result(syncer::AUTOFILL_WALLET_DATA);
  merge_result.set_num_items_before_association(
      static_cast<int>(prev_card_count + prev_address_count));
  merge_result.set_num_items_after_association(
      static_cast<int>(wallet_cards.size() + wallet_addresses.size()));

  if (webdata_backend_ && (changed_cards || changed_addresses))
    webdata_backend_->NotifyOfMultipleAutofillChanges();

  return merge_result;
}

}  // namespace autofil
