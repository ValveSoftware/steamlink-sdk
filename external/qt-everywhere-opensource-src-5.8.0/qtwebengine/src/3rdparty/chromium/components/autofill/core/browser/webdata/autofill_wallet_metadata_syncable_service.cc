// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_metadata_syncable_service.h"

#include <stddef.h>

#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_vector.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_data_model.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_data.h"
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

// Returns syncable metadata for the |local| profile or credit card.
syncer::SyncData BuildSyncData(sync_pb::WalletMetadataSpecifics::Type type,
                               const std::string& server_id,
                               const AutofillDataModel& local) {
  sync_pb::EntitySpecifics entity;
  sync_pb::WalletMetadataSpecifics* metadata = entity.mutable_wallet_metadata();
  metadata->set_type(type);
  metadata->set_id(server_id);
  metadata->set_use_count(local.use_count());
  metadata->set_use_date(local.use_date().ToInternalValue());

  std::string sync_tag;
  switch (type) {
    case sync_pb::WalletMetadataSpecifics::ADDRESS:
      sync_tag = "address-" + server_id;
      break;
    case sync_pb::WalletMetadataSpecifics::CARD:
      sync_tag = "card-" + server_id;
      break;
    case sync_pb::WalletMetadataSpecifics::UNKNOWN:
      NOTREACHED();
      break;
  }

  return syncer::SyncData::CreateLocalData(sync_tag, sync_tag, entity);
}

// If the metadata exists locally, undelete it on the sync server.
template <class DataType>
void UndeleteMetadataIfExisting(
    const std::string& server_id,
    const sync_pb::WalletMetadataSpecifics::Type& metadata_type,
    base::ScopedPtrHashMap<std::string, std::unique_ptr<DataType>>* locals,
    syncer::SyncChangeList* changes_to_sync) {
  const auto& it = locals->find(server_id);
  if (it != locals->end()) {
    std::unique_ptr<DataType> local_metadata = locals->take_and_erase(it);
    changes_to_sync->push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_ADD,
        BuildSyncData(metadata_type, server_id, *local_metadata)));
  }
}

syncer::SyncDataList::iterator FindServerIdAndTypeInCache(
    const std::string& server_id,
    const sync_pb::WalletMetadataSpecifics::Type& type,
    syncer::SyncDataList* cache) {
  for (auto it = cache->begin(); it != cache->end(); ++it) {
    if (server_id == it->GetSpecifics().wallet_metadata().id() &&
        type == it->GetSpecifics().wallet_metadata().type()) {
      return it;
    }
  }

  return cache->end();
}

syncer::SyncDataList::iterator FindItemInCache(const syncer::SyncData& item,
                                               syncer::SyncDataList* cache) {
  return FindServerIdAndTypeInCache(
      item.GetSpecifics().wallet_metadata().id(),
      item.GetSpecifics().wallet_metadata().type(), cache);
}

void RemoveItemFromCache(const syncer::SyncData& item,
                         syncer::SyncDataList* cache) {
  auto it = FindItemInCache(item, cache);
  if (it != cache->end())
    cache->erase(it);
}

void AddOrUpdateItemInCache(const syncer::SyncData& item,
                            syncer::SyncDataList* cache) {
  auto it = FindItemInCache(item, cache);
  if (it != cache->end())
    *it = item;
  else
    cache->push_back(item);
}

void ApplyChangesToCache(const syncer::SyncChangeList& changes,
                         syncer::SyncDataList* cache) {
  for (const syncer::SyncChange& change : changes) {
    switch (change.change_type()) {
      case syncer::SyncChange::ACTION_ADD:
      // Intentional fall through.
      case syncer::SyncChange::ACTION_UPDATE:
        AddOrUpdateItemInCache(change.sync_data(), cache);
        break;

      case syncer::SyncChange::ACTION_DELETE:
        RemoveItemFromCache(change.sync_data(), cache);
        break;

      case syncer::SyncChange::ACTION_INVALID:
        NOTREACHED();
        break;
    }
  }
}

// Merges |remote| metadata into a collection of metadata |locals|. Returns true
// if the corresponding local metadata was found.
//
// Stores an "update" in |changes_to_sync| if |remote| corresponds to an item in
// |locals| that has higher use count and later use date.
template <class DataType>
bool MergeRemote(
    const syncer::SyncData& remote,
    const base::Callback<bool(const DataType&)>& updater,
    base::ScopedPtrHashMap<std::string, std::unique_ptr<DataType>>* locals,
    syncer::SyncChangeList* changes_to_sync) {
  DCHECK(locals);
  DCHECK(changes_to_sync);

  const sync_pb::WalletMetadataSpecifics& remote_metadata =
      remote.GetSpecifics().wallet_metadata();
  auto it = locals->find(remote_metadata.id());
  if (it == locals->end())
    return false;

  std::unique_ptr<DataType> local_metadata = locals->take_and_erase(it);

  size_t remote_use_count =
      base::checked_cast<size_t>(remote_metadata.use_count());
  bool is_local_modified = false;
  bool is_remote_outdated = false;
  if (local_metadata->use_count() < remote_use_count) {
    local_metadata->set_use_count(remote_use_count);
    is_local_modified = true;
  } else if (local_metadata->use_count() > remote_use_count) {
    is_remote_outdated = true;
  }

  base::Time remote_use_date =
      base::Time::FromInternalValue(remote_metadata.use_date());
  if (local_metadata->use_date() < remote_use_date) {
    local_metadata->set_use_date(remote_use_date);
    is_local_modified = true;
  } else if (local_metadata->use_date() > remote_use_date) {
    is_remote_outdated = true;
  }

  if (is_remote_outdated) {
    changes_to_sync->push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
        BuildSyncData(remote_metadata.type(), remote_metadata.id(),
                      *local_metadata)));
  }

  if (is_local_modified)
    updater.Run(*local_metadata);

  return true;
}

template <typename DataType>
std::string GetServerId(const DataType& data) {
  std::string server_id;
  base::Base64Encode(data.server_id(), &server_id);
  return server_id;
}

}  // namespace

AutofillWalletMetadataSyncableService::
    ~AutofillWalletMetadataSyncableService() {}

syncer::SyncMergeResult
AutofillWalletMetadataSyncableService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
    std::unique_ptr<syncer::SyncErrorFactory> sync_error_factory) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!sync_processor_);
  DCHECK(!sync_error_factory_);
  DCHECK_EQ(syncer::AUTOFILL_WALLET_METADATA, type);

  sync_processor_ = std::move(sync_processor);
  sync_error_factory_ = std::move(sync_error_factory);

  cache_ = initial_sync_data;

  syncer::SyncMergeResult result = MergeData(initial_sync_data);
  if (web_data_backend_)
    web_data_backend_->NotifyThatSyncHasStarted(type);
  return result;
}

void AutofillWalletMetadataSyncableService::StopSyncing(
    syncer::ModelType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(syncer::AUTOFILL_WALLET_METADATA, type);

  sync_processor_.reset();
  sync_error_factory_.reset();
  cache_.clear();
}

syncer::SyncDataList AutofillWalletMetadataSyncableService::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(syncer::AUTOFILL_WALLET_METADATA, type);

  syncer::SyncDataList data_list;
  base::ScopedPtrHashMap<std::string, std::unique_ptr<AutofillProfile>>
      profiles;
  base::ScopedPtrHashMap<std::string, std::unique_ptr<CreditCard>> cards;
  if (GetLocalData(&profiles, &cards)) {
    for (const auto& it : profiles) {
      data_list.push_back(BuildSyncData(
          sync_pb::WalletMetadataSpecifics::ADDRESS, it.first, *it.second));
    }

    for (const auto& it : cards) {
      data_list.push_back(BuildSyncData(sync_pb::WalletMetadataSpecifics::CARD,
                                        it.first, *it.second));
    }
  }

  return data_list;
}

syncer::SyncError AutofillWalletMetadataSyncableService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& changes_from_sync) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ApplyChangesToCache(changes_from_sync, &cache_);

  base::ScopedPtrHashMap<std::string, std::unique_ptr<AutofillProfile>>
      profiles;
  base::ScopedPtrHashMap<std::string, std::unique_ptr<CreditCard>> cards;
  GetLocalData(&profiles, &cards);

  // base::Unretained is used because the callbacks are invoked synchronously.
  base::Callback<bool(const AutofillProfile&)> address_updater =
      base::Bind(&AutofillWalletMetadataSyncableService::UpdateAddressStats,
                 base::Unretained(this));
  base::Callback<bool(const CreditCard&)> card_updater =
      base::Bind(&AutofillWalletMetadataSyncableService::UpdateCardStats,
                 base::Unretained(this));

  syncer::SyncChangeList changes_to_sync;
  for (const syncer::SyncChange& change : changes_from_sync) {
    const sync_pb::WalletMetadataSpecifics& remote_metadata =
        change.sync_data().GetSpecifics().wallet_metadata();
    switch (change.change_type()) {
      case syncer::SyncChange::ACTION_ADD:
      // Intentional fall through.
      case syncer::SyncChange::ACTION_UPDATE:
        switch (remote_metadata.type()) {
          case sync_pb::WalletMetadataSpecifics::ADDRESS:
            MergeRemote(change.sync_data(), address_updater, &profiles,
                        &changes_to_sync);
            break;

          case sync_pb::WalletMetadataSpecifics::CARD:
            MergeRemote(change.sync_data(), card_updater, &cards,
                        &changes_to_sync);
            break;

          case sync_pb::WalletMetadataSpecifics::UNKNOWN:
            NOTREACHED();
            break;
        }
        break;

      // Metadata should only be deleted when the underlying data is deleted.
      case syncer::SyncChange::ACTION_DELETE:
        switch (remote_metadata.type()) {
          case sync_pb::WalletMetadataSpecifics::ADDRESS:
            UndeleteMetadataIfExisting(
                remote_metadata.id(), sync_pb::WalletMetadataSpecifics::ADDRESS,
                &profiles, &changes_to_sync);
            break;

          case sync_pb::WalletMetadataSpecifics::CARD:
            UndeleteMetadataIfExisting(remote_metadata.id(),
                                       sync_pb::WalletMetadataSpecifics::CARD,
                                       &cards, &changes_to_sync);
            break;

          case sync_pb::WalletMetadataSpecifics::UNKNOWN:
            NOTREACHED();
            break;
        }
        break;

      case syncer::SyncChange::ACTION_INVALID:
        NOTREACHED();
        break;
    }
  }

  syncer::SyncError status;
  if (!changes_to_sync.empty())
    status = SendChangesToSyncServer(changes_to_sync);

  return status;
}

void AutofillWalletMetadataSyncableService::AutofillProfileChanged(
    const AutofillProfileChange& change) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (sync_processor_ && change.data_model() &&
      change.data_model()->record_type() != AutofillProfile::LOCAL_PROFILE) {
    AutofillDataModelChanged(GetServerId(*change.data_model()),
                             sync_pb::WalletMetadataSpecifics::ADDRESS,
                             *change.data_model());
  }
}

void AutofillWalletMetadataSyncableService::CreditCardChanged(
    const CreditCardChange& change) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (sync_processor_ && change.data_model() &&
      change.data_model()->record_type() != CreditCard::LOCAL_CARD) {
    AutofillDataModelChanged(GetServerId(*change.data_model()),
                             sync_pb::WalletMetadataSpecifics::CARD,
                             *change.data_model());
  }
}

void AutofillWalletMetadataSyncableService::AutofillMultipleChanged() {
  if (sync_processor_)
    MergeData(cache_);
}

// static
void AutofillWalletMetadataSyncableService::CreateForWebDataServiceAndBackend(
    AutofillWebDataService* web_data_service,
    AutofillWebDataBackend* web_data_backend,
    const std::string& app_locale) {
  web_data_service->GetDBUserData()->SetUserData(
      UserDataKey(),
      new AutofillWalletMetadataSyncableService(web_data_backend, app_locale));
}

// static
AutofillWalletMetadataSyncableService*
AutofillWalletMetadataSyncableService::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<AutofillWalletMetadataSyncableService*>(
      web_data_service->GetDBUserData()->GetUserData(UserDataKey()));
}

AutofillWalletMetadataSyncableService::AutofillWalletMetadataSyncableService(
    AutofillWebDataBackend* web_data_backend,
    const std::string& app_locale)
    : web_data_backend_(web_data_backend), scoped_observer_(this) {
  scoped_observer_.Add(web_data_backend_);
}

bool AutofillWalletMetadataSyncableService::GetLocalData(
    base::ScopedPtrHashMap<std::string, std::unique_ptr<AutofillProfile>>*
        profiles,
    base::ScopedPtrHashMap<std::string, std::unique_ptr<CreditCard>>* cards)
    const {
  ScopedVector<AutofillProfile> profile_list;
  bool success =
      AutofillTable::FromWebDatabase(web_data_backend_->GetDatabase())
          ->GetServerProfiles(&profile_list.get());
  while (!profile_list.empty()) {
    profiles->add(GetServerId(*profile_list.front()),
                  base::WrapUnique(profile_list.front()));
    profile_list.weak_erase(profile_list.begin());
  }

  ScopedVector<CreditCard> card_list;
  success &= AutofillTable::FromWebDatabase(web_data_backend_->GetDatabase())
                 ->GetServerCreditCards(&card_list.get());
  while (!card_list.empty()) {
    cards->add(GetServerId(*card_list.front()),
               base::WrapUnique(card_list.front()));
    card_list.weak_erase(card_list.begin());
  }

  return success;
}

bool AutofillWalletMetadataSyncableService::UpdateAddressStats(
    const AutofillProfile& profile) {
  return AutofillTable::FromWebDatabase(web_data_backend_->GetDatabase())
      ->UpdateServerAddressUsageStats(profile);
}

bool AutofillWalletMetadataSyncableService::UpdateCardStats(
    const CreditCard& credit_card) {
  return AutofillTable::FromWebDatabase(web_data_backend_->GetDatabase())
      ->UpdateServerCardUsageStats(credit_card);
}

syncer::SyncError
AutofillWalletMetadataSyncableService::SendChangesToSyncServer(
    const syncer::SyncChangeList& changes_to_sync) {
  DCHECK(sync_processor_);
  ApplyChangesToCache(changes_to_sync, &cache_);
  return sync_processor_->ProcessSyncChanges(FROM_HERE, changes_to_sync);
}

syncer::SyncMergeResult AutofillWalletMetadataSyncableService::MergeData(
    const syncer::SyncDataList& sync_data) {
  base::ScopedPtrHashMap<std::string, std::unique_ptr<AutofillProfile>>
      profiles;
  base::ScopedPtrHashMap<std::string, std::unique_ptr<CreditCard>> cards;
  GetLocalData(&profiles, &cards);

  syncer::SyncMergeResult result(syncer::AUTOFILL_WALLET_METADATA);
  result.set_num_items_before_association(profiles.size() + cards.size());

  // base::Unretained is used because the callbacks are invoked synchronously.
  base::Callback<bool(const AutofillProfile&)> address_updater =
      base::Bind(&AutofillWalletMetadataSyncableService::UpdateAddressStats,
                 base::Unretained(this));
  base::Callback<bool(const CreditCard&)> card_updater =
      base::Bind(&AutofillWalletMetadataSyncableService::UpdateCardStats,
                 base::Unretained(this));

  syncer::SyncChangeList changes_to_sync;
  for (const syncer::SyncData& remote : sync_data) {
    DCHECK(remote.IsValid());
    DCHECK_EQ(syncer::AUTOFILL_WALLET_METADATA, remote.GetDataType());
    switch (remote.GetSpecifics().wallet_metadata().type()) {
      case sync_pb::WalletMetadataSpecifics::ADDRESS:
        if (!MergeRemote(remote, address_updater, &profiles,
                         &changes_to_sync)) {
          changes_to_sync.push_back(syncer::SyncChange(
              FROM_HERE, syncer::SyncChange::ACTION_DELETE, remote));
        }
        break;

      case sync_pb::WalletMetadataSpecifics::CARD:
        if (!MergeRemote(remote, card_updater, &cards, &changes_to_sync)) {
          changes_to_sync.push_back(syncer::SyncChange(
              FROM_HERE, syncer::SyncChange::ACTION_DELETE, remote));
        }
        break;

      case sync_pb::WalletMetadataSpecifics::UNKNOWN:
        NOTREACHED();
        break;
    }
  }

  // The remainder of |profiles| were not listed in |sync_data|.
  for (const auto& it : profiles) {
    changes_to_sync.push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_ADD,
        BuildSyncData(sync_pb::WalletMetadataSpecifics::ADDRESS, it.first,
                      *it.second)));
  }

  // The remainder of |cards| were not listed in |sync_data|.
  for (const auto& it : cards) {
    changes_to_sync.push_back(
        syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_ADD,
                           BuildSyncData(sync_pb::WalletMetadataSpecifics::CARD,
                                         it.first, *it.second)));
  }

  // The only operation that is performed locally in response to a sync is an
  // update. Adds and deletes are performed in response to changes to the Wallet
  // data.
  result.set_num_items_after_association(result.num_items_before_association());
  result.set_num_items_added(0);
  result.set_num_items_deleted(0);

  if (!changes_to_sync.empty())
    result.set_error(SendChangesToSyncServer(changes_to_sync));

  return result;
}

void AutofillWalletMetadataSyncableService::AutofillDataModelChanged(
    const std::string& server_id,
    const sync_pb::WalletMetadataSpecifics::Type& type,
    const AutofillDataModel& local) {
  auto it = FindServerIdAndTypeInCache(server_id, type, &cache_);
  if (it == cache_.end())
    return;

  const sync_pb::WalletMetadataSpecifics& remote =
      it->GetSpecifics().wallet_metadata();
  if (base::checked_cast<size_t>(remote.use_count()) < local.use_count() &&
      base::Time::FromInternalValue(remote.use_date()) < local.use_date()) {
    SendChangesToSyncServer(syncer::SyncChangeList(
        1, syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
                              BuildSyncData(remote.type(), server_id, local))));
  }
}

}  // namespace autofill
