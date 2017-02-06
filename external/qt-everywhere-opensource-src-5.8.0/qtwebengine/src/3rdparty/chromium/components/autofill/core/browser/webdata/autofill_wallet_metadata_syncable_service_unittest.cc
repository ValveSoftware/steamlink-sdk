// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_metadata_syncable_service.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_change_processor_wrapper_for_test.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/protocol/autofill_specifics.pb.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using testing::DoAll;
using testing::ElementsAre;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::Test;
using testing::UnorderedElementsAre;
using testing::Value;
using testing::_;

// Non-UTF8 server IDs.
const char kAddr1[] = "addr1\xEF\xBF\xBE";
const char kAddr2[] = "addr2\xEF\xBF\xBE";
const char kCard1[] = "card1\xEF\xBF\xBE";
const char kCard2[] = "card2\xEF\xBF\xBE";

// Base64 encodings of the server IDs. These are suitable for syncing, because
// they are valid UTF-8.
const char kAddr1Utf8[] = "YWRkcjHvv74=";
const char kAddr2Utf8[] = "YWRkcjLvv74=";
const char kCard1Utf8[] = "Y2FyZDHvv74=";
const char kCard2Utf8[] = "Y2FyZDLvv74=";

// Unique sync tags for the server IDs.
const char kAddr1SyncTag[] = "address-YWRkcjHvv74=";
const char kAddr2SyncTag[] = "address-YWRkcjLvv74=";
const char kCard1SyncTag[] = "card-Y2FyZDHvv74=";
const char kCard2SyncTag[] = "card-Y2FyZDLvv74=";

// Map values are owned by the caller to GetLocalData.
ACTION_P2(GetCopiesOf, profiles, cards) {
  for (const auto& profile : *profiles) {
    std::string utf8_server_id;
    base::Base64Encode(profile.server_id(), &utf8_server_id);
    arg0->add(utf8_server_id, base::WrapUnique(new AutofillProfile(profile)));
  }

  for (const auto& card : *cards) {
    std::string utf8_server_id;
    base::Base64Encode(card.server_id(), &utf8_server_id);
    arg1->add(utf8_server_id, base::WrapUnique(new CreditCard(card)));
  }
}

ACTION_P(SaveDataIn, list) {
  for (auto& item : *list) {
    if (item.server_id() == arg0.server_id()) {
      item = arg0;
      return;
    }
  }

  list->push_back(arg0);
}

// A syncable service for Wallet metadata that mocks out disk IO.
class MockService : public AutofillWalletMetadataSyncableService {
 public:
  MockService(AutofillWebDataBackend* web_data_backend)
      : AutofillWalletMetadataSyncableService(web_data_backend, std::string()) {
    ON_CALL(*this, GetLocalData(_, _))
        .WillByDefault(DoAll(GetCopiesOf(&server_profiles_, &server_cards_),
                             Return(true)));

    ON_CALL(*this, UpdateAddressStats(_))
        .WillByDefault(DoAll(SaveDataIn(&server_profiles_), Return(true)));

    ON_CALL(*this, UpdateCardStats(_))
        .WillByDefault(DoAll(SaveDataIn(&server_cards_), Return(true)));

    ON_CALL(*this, SendChangesToSyncServer(_))
        .WillByDefault(
            Invoke(this, &MockService::SendChangesToSyncServerConcrete));
  }

  ~MockService() override {}

  MOCK_METHOD1(UpdateAddressStats, bool(const AutofillProfile&));
  MOCK_METHOD1(UpdateCardStats, bool(const CreditCard&));
  MOCK_METHOD1(SendChangesToSyncServer,
               syncer::SyncError(const syncer::SyncChangeList&));

  void ClearServerData() {
    server_profiles_.clear();
    server_cards_.clear();
  }

 private:
  MOCK_CONST_METHOD2(
      GetLocalData,
      bool(base::ScopedPtrHashMap<std::string,
                                  std::unique_ptr<AutofillProfile>>*,
           base::ScopedPtrHashMap<std::string, std::unique_ptr<CreditCard>>*));

  syncer::SyncError SendChangesToSyncServerConcrete(
      const syncer::SyncChangeList& changes) {
    return AutofillWalletMetadataSyncableService::SendChangesToSyncServer(
        changes);
  }

  syncer::SyncDataList GetAllSyncDataConcrete(syncer::ModelType type) const {
    return AutofillWalletMetadataSyncableService::GetAllSyncData(type);
  }

  std::vector<AutofillProfile> server_profiles_;
  std::vector<CreditCard> server_cards_;

  DISALLOW_COPY_AND_ASSIGN(MockService);
};

class NoOpWebData : public AutofillWebDataBackend {
 public:
  NoOpWebData() {}
  ~NoOpWebData() override {}

 private:
  // AutofillWebDataBackend implementation.
  WebDatabase* GetDatabase() override { return nullptr; }
  void AddObserver(
      AutofillWebDataServiceObserverOnDBThread* observer) override {}
  void RemoveObserver(
      AutofillWebDataServiceObserverOnDBThread* observer) override {}
  void RemoveExpiredFormElements() override {}
  void NotifyOfMultipleAutofillChanges() override {}
  void NotifyThatSyncHasStarted(syncer::ModelType /* model_type */) override {}

  DISALLOW_COPY_AND_ASSIGN(NoOpWebData);
};

class AutofillWalletMetadataSyncableServiceTest : public Test {
 public:
  AutofillWalletMetadataSyncableServiceTest()
      : local_(&no_op_web_data_), remote_(&no_op_web_data_) {}
  ~AutofillWalletMetadataSyncableServiceTest() override {}

  // Outlives local_ and remote_.
  NoOpWebData no_op_web_data_;

  // Outlived by no_op_web_data_.
  NiceMock<MockService> local_;
  NiceMock<MockService> remote_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillWalletMetadataSyncableServiceTest);
};

// Verify that nothing is sent to the sync server when there's no metadata on
// disk.
TEST_F(AutofillWalletMetadataSyncableServiceTest, NoMetadataToReturn) {
  EXPECT_TRUE(local_.GetAllSyncData(syncer::AUTOFILL_WALLET_METADATA).empty());
}

AutofillProfile BuildAddress(const std::string& server_id,
                             int64_t use_count,
                             int64_t use_date) {
  AutofillProfile profile(AutofillProfile::SERVER_PROFILE, server_id);
  profile.set_use_count(use_count);
  profile.set_use_date(base::Time::FromInternalValue(use_date));
  return profile;
}

CreditCard BuildCard(const std::string& server_id,
                     int64_t use_count,
                     int64_t use_date) {
  CreditCard card(CreditCard::MASKED_SERVER_CARD, server_id);
  card.set_use_count(use_count);
  card.set_use_date(base::Time::FromInternalValue(use_date));
  return card;
}

MATCHER_P5(SyncDataMatches,
           sync_tag,
           metadata_type,
           server_id,
           use_count,
           use_date,
           "") {
  return arg.IsValid() &&
         syncer::AUTOFILL_WALLET_METADATA == arg.GetDataType() &&
         sync_tag == syncer::SyncDataLocal(arg).GetTag() &&
         metadata_type == arg.GetSpecifics().wallet_metadata().type() &&
         server_id == arg.GetSpecifics().wallet_metadata().id() &&
         use_count == arg.GetSpecifics().wallet_metadata().use_count() &&
         use_date == arg.GetSpecifics().wallet_metadata().use_date();
}

// Verify that all metadata from disk is sent to the sync server.
TEST_F(AutofillWalletMetadataSyncableServiceTest, ReturnAllMetadata) {
  local_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  local_.UpdateCardStats(BuildCard(kCard1, 3, 4));

  EXPECT_THAT(
      local_.GetAllSyncData(syncer::AUTOFILL_WALLET_METADATA),
      UnorderedElementsAre(
          SyncDataMatches(kAddr1SyncTag,
                          sync_pb::WalletMetadataSpecifics::ADDRESS, kAddr1Utf8,
                          1, 2),
          SyncDataMatches(kCard1SyncTag, sync_pb::WalletMetadataSpecifics::CARD,
                          kCard1Utf8, 3, 4)));
}

void MergeMetadata(MockService* local, MockService* remote) {
  // The wrapper for |remote| gives it a null change processor, so sending
  // changes is not possible.
  ON_CALL(*remote, SendChangesToSyncServer(_))
      .WillByDefault(Return(syncer::SyncError()));

  std::unique_ptr<syncer::SyncErrorFactoryMock> errors(
      new syncer::SyncErrorFactoryMock);
  EXPECT_CALL(*errors, CreateAndUploadError(_, _)).Times(0);
  EXPECT_FALSE(
      local
          ->MergeDataAndStartSyncing(
              syncer::AUTOFILL_WALLET_METADATA,
              remote->GetAllSyncData(syncer::AUTOFILL_WALLET_METADATA),
              std::unique_ptr<syncer::SyncChangeProcessor>(
                  new syncer::SyncChangeProcessorWrapperForTest(remote)),
              std::move(errors))
          .error()
          .IsSet());
}

// Verify that nothing is written to disk or sent to the sync server when two
// empty clients are syncing.
TEST_F(AutofillWalletMetadataSyncableServiceTest, TwoEmptyClients) {
  EXPECT_CALL(local_, UpdateAddressStats(_)).Times(0);
  EXPECT_CALL(local_, UpdateCardStats(_)).Times(0);
  EXPECT_CALL(local_, SendChangesToSyncServer(_)).Times(0);

  MergeMetadata(&local_, &remote_);
}

MATCHER_P2(SyncChangeMatches, change_type, sync_tag, "") {
  return arg.IsValid() && change_type == arg.change_type() &&
         sync_tag == syncer::SyncDataLocal(arg.sync_data()).GetTag() &&
         syncer::AUTOFILL_WALLET_METADATA == arg.sync_data().GetDataType();
}

// Verify that remote data without local counterpart is deleted during the
// initial merge.
TEST_F(AutofillWalletMetadataSyncableServiceTest, DeleteFromServerOnMerge) {
  remote_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  remote_.UpdateCardStats(BuildCard(kCard1, 3, 4));

  EXPECT_CALL(local_, UpdateAddressStats(_)).Times(0);
  EXPECT_CALL(local_, UpdateCardStats(_)).Times(0);
  EXPECT_CALL(
      local_,
      SendChangesToSyncServer(UnorderedElementsAre(
          SyncChangeMatches(syncer::SyncChange::ACTION_DELETE, kAddr1SyncTag),
          SyncChangeMatches(syncer::SyncChange::ACTION_DELETE,
                            kCard1SyncTag))));

  MergeMetadata(&local_, &remote_);
}

MATCHER_P6(SyncChangeAndDataMatch,
           change_type,
           sync_tag,
           metadata_type,
           server_id,
           use_count,
           use_date,
           "") {
  return Value(arg, SyncChangeMatches(change_type, sync_tag)) &&
         Value(arg.sync_data(),
               SyncDataMatches(sync_tag, metadata_type, server_id, use_count,
                               use_date));
}

// Verify that local data is sent to the sync server during the initial merge,
// if the server does not have the data already.
TEST_F(AutofillWalletMetadataSyncableServiceTest, AddToServerOnMerge) {
  local_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  local_.UpdateCardStats(BuildCard(kCard1, 3, 4));

  EXPECT_CALL(local_, UpdateAddressStats(_)).Times(0);
  EXPECT_CALL(local_, UpdateCardStats(_)).Times(0);
  EXPECT_CALL(
      local_,
      SendChangesToSyncServer(UnorderedElementsAre(
          SyncChangeAndDataMatch(syncer::SyncChange::ACTION_ADD, kAddr1SyncTag,
                                 sync_pb::WalletMetadataSpecifics::ADDRESS,
                                 kAddr1Utf8, 1, 2),
          SyncChangeAndDataMatch(syncer::SyncChange::ACTION_ADD, kCard1SyncTag,
                                 sync_pb::WalletMetadataSpecifics::CARD,
                                 kCard1Utf8, 3, 4))));

  MergeMetadata(&local_, &remote_);
}

// Verify that no data is written to disk or sent to the sync server if the
// local and remote data are identical during the initial merge.
TEST_F(AutofillWalletMetadataSyncableServiceTest,
       IgnoreIdenticalValuesOnMerge) {
  local_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  local_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  remote_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  remote_.UpdateCardStats(BuildCard(kCard1, 3, 4));

  EXPECT_CALL(local_, UpdateAddressStats(_)).Times(0);
  EXPECT_CALL(local_, UpdateCardStats(_)).Times(0);
  EXPECT_CALL(local_, SendChangesToSyncServer(_)).Times(0);

  MergeMetadata(&local_, &remote_);
}

MATCHER_P3(AutofillMetadataMatches, server_id, use_count, use_date, "") {
  return arg.server_id() == server_id &&
         arg.use_count() == base::checked_cast<size_t>(use_count) &&
         arg.use_date() == base::Time::FromInternalValue(use_date);
}

// Verify that remote data with higher values of use count and last use date is
// saved to disk during the initial merge.
TEST_F(AutofillWalletMetadataSyncableServiceTest,
       SaveHigherValuesLocallyOnMerge) {
  local_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  local_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  remote_.UpdateAddressStats(BuildAddress(kAddr1, 10, 20));
  remote_.UpdateCardStats(BuildCard(kCard1, 30, 40));

  EXPECT_CALL(local_,
              UpdateAddressStats(AutofillMetadataMatches(kAddr1, 10, 20)));
  EXPECT_CALL(local_, UpdateCardStats(AutofillMetadataMatches(kCard1, 30, 40)));
  EXPECT_CALL(local_, SendChangesToSyncServer(_)).Times(0);

  MergeMetadata(&local_, &remote_);
}

// Verify that local data with higher values of use count and last use date is
// sent to the sync server during the initial merge.
TEST_F(AutofillWalletMetadataSyncableServiceTest,
       SendHigherValuesToServerOnMerge) {
  local_.UpdateAddressStats(BuildAddress(kAddr1, 10, 20));
  local_.UpdateCardStats(BuildCard(kCard1, 30, 40));
  remote_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  remote_.UpdateCardStats(BuildCard(kCard1, 3, 4));

  EXPECT_CALL(local_, UpdateAddressStats(_)).Times(0);
  EXPECT_CALL(local_, UpdateCardStats(_)).Times(0);
  EXPECT_CALL(
      local_,
      SendChangesToSyncServer(UnorderedElementsAre(
          SyncChangeAndDataMatch(
              syncer::SyncChange::ACTION_UPDATE, kAddr1SyncTag,
              sync_pb::WalletMetadataSpecifics::ADDRESS, kAddr1Utf8, 10, 20),
          SyncChangeAndDataMatch(
              syncer::SyncChange::ACTION_UPDATE, kCard1SyncTag,
              sync_pb::WalletMetadataSpecifics::CARD, kCard1Utf8, 30, 40))));

  MergeMetadata(&local_, &remote_);
}

// Verify that lower values of metadata are not sent to the sync server when
// local metadata is updated.
TEST_F(AutofillWalletMetadataSyncableServiceTest,
       DontSendLowerValueToServerOnSingleChange) {
  local_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  local_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  remote_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  remote_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  MergeMetadata(&local_, &remote_);
  AutofillProfile address = BuildAddress(kAddr1, 0, 0);
  CreditCard card = BuildCard(kCard1, 0, 0);

  EXPECT_CALL(local_, SendChangesToSyncServer(_)).Times(0);

  local_.AutofillProfileChanged(AutofillProfileChange(
      AutofillProfileChange::UPDATE, address.guid(), &address));
  local_.CreditCardChanged(
      CreditCardChange(CreditCardChange::UPDATE, card.guid(), &card));
}

// Verify that higher values of metadata are sent to the sync server when local
// metadata is updated.
TEST_F(AutofillWalletMetadataSyncableServiceTest,
       SendHigherValuesToServerOnLocalSingleChange) {
  local_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  local_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  remote_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  remote_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  MergeMetadata(&local_, &remote_);
  AutofillProfile address = BuildAddress(kAddr1, 10, 20);
  CreditCard card = BuildCard(kCard1, 30, 40);

  EXPECT_CALL(
      local_,
      SendChangesToSyncServer(ElementsAre(SyncChangeAndDataMatch(
          syncer::SyncChange::ACTION_UPDATE, kAddr1SyncTag,
          sync_pb::WalletMetadataSpecifics::ADDRESS, kAddr1Utf8, 10, 20))));
  EXPECT_CALL(
      local_,
      SendChangesToSyncServer(ElementsAre(SyncChangeAndDataMatch(
          syncer::SyncChange::ACTION_UPDATE, kCard1SyncTag,
          sync_pb::WalletMetadataSpecifics::CARD, kCard1Utf8, 30, 40))));

  local_.AutofillProfileChanged(AutofillProfileChange(
      AutofillProfileChange::UPDATE, address.guid(), &address));
  local_.CreditCardChanged(
      CreditCardChange(CreditCardChange::UPDATE, card.guid(), &card));
}

// Verify that one-off addition of metadata is not sent to the sync
// server. Metadata add and delete trigger multiple changes notification
// instead.
TEST_F(AutofillWalletMetadataSyncableServiceTest,
       DontAddToServerOnSingleChange) {
  local_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  local_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  remote_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  remote_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  MergeMetadata(&local_, &remote_);
  AutofillProfile address = BuildAddress(kAddr2, 5, 6);
  CreditCard card = BuildCard(kCard2, 7, 8);

  EXPECT_CALL(local_, SendChangesToSyncServer(_)).Times(0);

  local_.AutofillProfileChanged(AutofillProfileChange(
      AutofillProfileChange::UPDATE, address.guid(), &address));
  local_.CreditCardChanged(
      CreditCardChange(CreditCardChange::UPDATE, card.guid(), &card));
}

// Verify that new metadata is sent to the sync server when multiple metadata
// values change at once.
TEST_F(AutofillWalletMetadataSyncableServiceTest, AddToServerOnMultiChange) {
  local_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  local_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  remote_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  remote_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  MergeMetadata(&local_, &remote_);
  // These methods do not trigger notifications or sync:
  local_.UpdateAddressStats(BuildAddress(kAddr2, 5, 6));
  local_.UpdateCardStats(BuildCard(kCard2, 7, 8));

  EXPECT_CALL(
      local_,
      SendChangesToSyncServer(UnorderedElementsAre(
          SyncChangeAndDataMatch(syncer::SyncChange::ACTION_ADD, kAddr2SyncTag,
                                 sync_pb::WalletMetadataSpecifics::ADDRESS,
                                 kAddr2Utf8, 5, 6),
          SyncChangeAndDataMatch(syncer::SyncChange::ACTION_ADD, kCard2SyncTag,
                                 sync_pb::WalletMetadataSpecifics::CARD,
                                 kCard2Utf8, 7, 8))));

  local_.AutofillMultipleChanged();
}

// Verify that higher values of existing metadata are sent to the sync server
// when multiple metadata values change at once.
TEST_F(AutofillWalletMetadataSyncableServiceTest,
       UpdateToHigherValueOnServerOnMultiChange) {
  local_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  local_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  remote_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  remote_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  MergeMetadata(&local_, &remote_);
  // These methods do not trigger notifications or sync:
  local_.UpdateAddressStats(BuildAddress(kAddr1, 5, 6));
  local_.UpdateCardStats(BuildCard(kCard1, 7, 8));

  EXPECT_CALL(
      local_,
      SendChangesToSyncServer(UnorderedElementsAre(
          SyncChangeAndDataMatch(
              syncer::SyncChange::ACTION_UPDATE, kAddr1SyncTag,
              sync_pb::WalletMetadataSpecifics::ADDRESS, kAddr1Utf8, 5, 6),
          SyncChangeAndDataMatch(
              syncer::SyncChange::ACTION_UPDATE, kCard1SyncTag,
              sync_pb::WalletMetadataSpecifics::CARD, kCard1Utf8, 7, 8))));

  local_.AutofillMultipleChanged();
}

// Verify that lower values of existing metadata are not sent to the sync server
// when multiple metadata values change at once.
TEST_F(AutofillWalletMetadataSyncableServiceTest,
       DontUpdateToLowerValueOnServerOnMultiChange) {
  local_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  local_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  remote_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  remote_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  MergeMetadata(&local_, &remote_);
  // These methods do not trigger notifications or sync:
  local_.UpdateAddressStats(BuildAddress(kAddr1, 0, 0));
  local_.UpdateCardStats(BuildCard(kCard1, 0, 0));

  EXPECT_CALL(local_, SendChangesToSyncServer(_)).Times(0);

  local_.AutofillMultipleChanged();
}

// Verify that erased local metadata is also erased from the sync server when
// multiple metadata values change at once.
TEST_F(AutofillWalletMetadataSyncableServiceTest,
       DeleteFromServerOnMultiChange) {
  local_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  local_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  remote_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  remote_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  MergeMetadata(&local_, &remote_);
  // This method dooes not trigger notifications or sync:
  local_.ClearServerData();

  EXPECT_CALL(
      local_,
      SendChangesToSyncServer(UnorderedElementsAre(
          SyncChangeMatches(syncer::SyncChange::ACTION_DELETE, kAddr1SyncTag),
          SyncChangeMatches(syncer::SyncChange::ACTION_DELETE,
                            kCard1SyncTag))));

  local_.AutofillMultipleChanged();
}

// Verify that empty sync change from the sync server does not trigger writing
// to disk or sending any data to the sync server.
TEST_F(AutofillWalletMetadataSyncableServiceTest, EmptySyncChange) {
  local_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  local_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  remote_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  remote_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  MergeMetadata(&local_, &remote_);

  EXPECT_CALL(local_, UpdateAddressStats(_)).Times(0);
  EXPECT_CALL(local_, UpdateCardStats(_)).Times(0);
  EXPECT_CALL(local_, SendChangesToSyncServer(_)).Times(0);

  local_.ProcessSyncChanges(FROM_HERE, syncer::SyncChangeList());
}

syncer::SyncChange BuildChange(
    syncer::SyncChange::SyncChangeType change_type,
    const std::string& sync_tag,
    sync_pb::WalletMetadataSpecifics::Type metadata_type,
    const std::string& server_id,
    int64_t use_count,
    int64_t use_date) {
  sync_pb::EntitySpecifics entity;
  entity.mutable_wallet_metadata()->set_type(metadata_type);
  entity.mutable_wallet_metadata()->set_id(server_id);
  entity.mutable_wallet_metadata()->set_use_count(use_count);
  entity.mutable_wallet_metadata()->set_use_date(use_date);
  return syncer::SyncChange(
      FROM_HERE, change_type,
      syncer::SyncData::CreateLocalData(sync_tag, sync_tag, entity));
}

// Verify that new metadata from the sync server is ignored when processing
// on-going sync changes. There should be no disk writes or messages to the sync
// server.
TEST_F(AutofillWalletMetadataSyncableServiceTest,
       IgnoreNewMetadataFromServerOnSyncChange) {
  local_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  local_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  remote_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  remote_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  MergeMetadata(&local_, &remote_);
  syncer::SyncChangeList changes;
  changes.push_back(BuildChange(syncer::SyncChange::ACTION_ADD, kAddr2SyncTag,
                                sync_pb::WalletMetadataSpecifics::ADDRESS,
                                kAddr2Utf8, 5, 6));
  changes.push_back(BuildChange(syncer::SyncChange::ACTION_ADD, kCard2SyncTag,
                                sync_pb::WalletMetadataSpecifics::CARD,
                                kCard2Utf8, 7, 8));

  EXPECT_CALL(local_, UpdateAddressStats(_)).Times(0);
  EXPECT_CALL(local_, UpdateCardStats(_)).Times(0);
  EXPECT_CALL(local_, SendChangesToSyncServer(_)).Times(0);

  local_.ProcessSyncChanges(FROM_HERE, changes);
}

// Verify that higher values of metadata from the sync server are saved to
// disk when processing on-going sync changes.
TEST_F(AutofillWalletMetadataSyncableServiceTest,
       SaveHigherValuesFromServerOnSyncChange) {
  local_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  local_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  remote_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  remote_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  MergeMetadata(&local_, &remote_);
  syncer::SyncChangeList changes;
  changes.push_back(BuildChange(
      syncer::SyncChange::ACTION_UPDATE, kAddr1SyncTag,
      sync_pb::WalletMetadataSpecifics::ADDRESS, kAddr1Utf8, 10, 20));
  changes.push_back(
      BuildChange(syncer::SyncChange::ACTION_UPDATE, kCard1SyncTag,
                  sync_pb::WalletMetadataSpecifics::CARD, kCard1Utf8, 30, 40));

  EXPECT_CALL(local_,
              UpdateAddressStats(AutofillMetadataMatches(kAddr1, 10, 20)));
  EXPECT_CALL(local_, UpdateCardStats(AutofillMetadataMatches(kCard1, 30, 40)));
  EXPECT_CALL(local_, SendChangesToSyncServer(_)).Times(0);

  local_.ProcessSyncChanges(FROM_HERE, changes);
}

// Verify that higher local values of metadata are sent to the sync server when
// processing on-going sync changes.
TEST_F(AutofillWalletMetadataSyncableServiceTest,
       SendHigherValuesToServerOnSyncChange) {
  local_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  local_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  remote_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  remote_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  MergeMetadata(&local_, &remote_);
  syncer::SyncChangeList changes;
  changes.push_back(
      BuildChange(syncer::SyncChange::ACTION_UPDATE, kAddr1SyncTag,
                  sync_pb::WalletMetadataSpecifics::ADDRESS, kAddr1Utf8, 0, 0));
  changes.push_back(
      BuildChange(syncer::SyncChange::ACTION_UPDATE, kCard1SyncTag,
                  sync_pb::WalletMetadataSpecifics::CARD, kCard1Utf8, 0, 0));

  EXPECT_CALL(local_, UpdateAddressStats(_)).Times(0);
  EXPECT_CALL(local_, UpdateCardStats(_)).Times(0);
  EXPECT_CALL(
      local_,
      SendChangesToSyncServer(UnorderedElementsAre(
          SyncChangeAndDataMatch(
              syncer::SyncChange::ACTION_UPDATE, kAddr1SyncTag,
              sync_pb::WalletMetadataSpecifics::ADDRESS, kAddr1Utf8, 1, 2),
          SyncChangeAndDataMatch(
              syncer::SyncChange::ACTION_UPDATE, kCard1SyncTag,
              sync_pb::WalletMetadataSpecifics::CARD, kCard1Utf8, 3, 4))));

  local_.ProcessSyncChanges(FROM_HERE, changes);
}

// Verify that addition of known metadata is treated the same as an update.
TEST_F(AutofillWalletMetadataSyncableServiceTest,
       TreatAdditionOfKnownMetadataAsUpdateOnSyncChange) {
  local_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  local_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  remote_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  remote_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  MergeMetadata(&local_, &remote_);
  syncer::SyncChangeList changes;
  changes.push_back(BuildChange(syncer::SyncChange::ACTION_ADD, kAddr1SyncTag,
                                sync_pb::WalletMetadataSpecifics::ADDRESS,
                                kAddr1Utf8, 0, 0));
  changes.push_back(BuildChange(syncer::SyncChange::ACTION_ADD, kCard1SyncTag,
                                sync_pb::WalletMetadataSpecifics::CARD,
                                kCard1Utf8, 0, 0));

  EXPECT_CALL(local_, UpdateAddressStats(_)).Times(0);
  EXPECT_CALL(local_, UpdateCardStats(_)).Times(0);
  EXPECT_CALL(
      local_,
      SendChangesToSyncServer(UnorderedElementsAre(
          SyncChangeAndDataMatch(
              syncer::SyncChange::ACTION_UPDATE, kAddr1SyncTag,
              sync_pb::WalletMetadataSpecifics::ADDRESS, kAddr1Utf8, 1, 2),
          SyncChangeAndDataMatch(
              syncer::SyncChange::ACTION_UPDATE, kCard1SyncTag,
              sync_pb::WalletMetadataSpecifics::CARD, kCard1Utf8, 3, 4))));

  local_.ProcessSyncChanges(FROM_HERE, changes);
}

// Verify that an update of locally unknown metadata is ignored. There should be
// no disk writes and no messages sent to the server.
TEST_F(AutofillWalletMetadataSyncableServiceTest,
       IgnoreUpdateOfUnknownMetadataOnSyncChange) {
  local_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  local_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  remote_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  remote_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  MergeMetadata(&local_, &remote_);
  syncer::SyncChangeList changes;
  changes.push_back(
      BuildChange(syncer::SyncChange::ACTION_UPDATE, kAddr2SyncTag,
                  sync_pb::WalletMetadataSpecifics::ADDRESS, kAddr2Utf8, 0, 0));
  changes.push_back(
      BuildChange(syncer::SyncChange::ACTION_UPDATE, kCard2SyncTag,
                  sync_pb::WalletMetadataSpecifics::CARD, kCard2Utf8, 0, 0));

  EXPECT_CALL(local_, UpdateAddressStats(_)).Times(0);
  EXPECT_CALL(local_, UpdateCardStats(_)).Times(0);
  EXPECT_CALL(local_, SendChangesToSyncServer(_)).Times(0);

  local_.ProcessSyncChanges(FROM_HERE, changes);
}

// Verify that deletion from the sync server of locally unknown metadata is
// ignored. There should be no disk writes and no messages sent to the server.
TEST_F(AutofillWalletMetadataSyncableServiceTest,
       IgnoreDeleteOfUnknownMetadataOnSyncChange) {
  local_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  local_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  remote_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  remote_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  MergeMetadata(&local_, &remote_);
  syncer::SyncChangeList changes;
  changes.push_back(
      BuildChange(syncer::SyncChange::ACTION_DELETE, kAddr2SyncTag,
                  sync_pb::WalletMetadataSpecifics::ADDRESS, kAddr2Utf8, 0, 0));
  changes.push_back(
      BuildChange(syncer::SyncChange::ACTION_DELETE, kCard2SyncTag,
                  sync_pb::WalletMetadataSpecifics::CARD, kCard2Utf8, 0, 0));

  EXPECT_CALL(local_, UpdateAddressStats(_)).Times(0);
  EXPECT_CALL(local_, UpdateCardStats(_)).Times(0);
  EXPECT_CALL(local_, SendChangesToSyncServer(_)).Times(0);

  local_.ProcessSyncChanges(FROM_HERE, changes);
}

// Verify that deletion from the sync server of locally existing metadata will
// trigger an undelete message sent to the server.
TEST_F(AutofillWalletMetadataSyncableServiceTest,
       UndeleteExistingMetadataOnSyncChange) {
  local_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  local_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  remote_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  remote_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  MergeMetadata(&local_, &remote_);
  syncer::SyncChangeList changes;
  changes.push_back(
      BuildChange(syncer::SyncChange::ACTION_DELETE, kAddr1SyncTag,
                  sync_pb::WalletMetadataSpecifics::ADDRESS, kAddr1Utf8, 0, 0));
  changes.push_back(
      BuildChange(syncer::SyncChange::ACTION_DELETE, kCard1SyncTag,
                  sync_pb::WalletMetadataSpecifics::CARD, kCard1Utf8, 0, 0));

  EXPECT_CALL(local_, UpdateAddressStats(_)).Times(0);
  EXPECT_CALL(local_, UpdateCardStats(_)).Times(0);
  EXPECT_CALL(
      local_,
      SendChangesToSyncServer(UnorderedElementsAre(
          SyncChangeAndDataMatch(syncer::SyncChange::ACTION_ADD, kAddr1SyncTag,
                                 sync_pb::WalletMetadataSpecifics::ADDRESS,
                                 kAddr1Utf8, 1, 2),
          SyncChangeAndDataMatch(syncer::SyncChange::ACTION_ADD, kCard1SyncTag,
                                 sync_pb::WalletMetadataSpecifics::CARD,
                                 kCard1Utf8, 3, 4))));

  local_.ProcessSyncChanges(FROM_HERE, changes);
}

// Verify that processing sync changes maintains the local cache of sync server
// data, which is used to avoid calling the expensive GetAllSyncData() function.
TEST_F(AutofillWalletMetadataSyncableServiceTest,
       CacheIsUpToDateAfterSyncChange) {
  local_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  local_.UpdateAddressStats(BuildAddress(kAddr2, 3, 4));
  local_.UpdateCardStats(BuildCard(kCard1, 5, 6));
  local_.UpdateCardStats(BuildCard(kCard2, 7, 8));
  remote_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  remote_.UpdateAddressStats(BuildAddress(kAddr2, 3, 4));
  remote_.UpdateCardStats(BuildCard(kCard1, 5, 6));
  remote_.UpdateCardStats(BuildCard(kCard2, 7, 8));
  MergeMetadata(&local_, &remote_);
  syncer::SyncChangeList changes;
  changes.push_back(BuildChange(
      syncer::SyncChange::ACTION_UPDATE, kAddr1SyncTag,
      sync_pb::WalletMetadataSpecifics::ADDRESS, kAddr1Utf8, 10, 20));
  changes.push_back(
      BuildChange(syncer::SyncChange::ACTION_UPDATE, kCard1SyncTag,
                  sync_pb::WalletMetadataSpecifics::CARD, kCard1Utf8, 50, 60));
  local_.ProcessSyncChanges(FROM_HERE, changes);
  // This method dooes not trigger notifications or sync:
  local_.ClearServerData();

  EXPECT_CALL(
      local_,
      SendChangesToSyncServer(UnorderedElementsAre(
          SyncChangeMatches(syncer::SyncChange::ACTION_DELETE, kAddr1SyncTag),
          SyncChangeMatches(syncer::SyncChange::ACTION_DELETE, kAddr2SyncTag),
          SyncChangeMatches(syncer::SyncChange::ACTION_DELETE, kCard1SyncTag),
          SyncChangeMatches(syncer::SyncChange::ACTION_DELETE,
                            kCard2SyncTag))));

  local_.AutofillMultipleChanged();
}

// Verify that Wallet data arriving after metadata will not send lower metadata
// values to the sync server.
TEST_F(AutofillWalletMetadataSyncableServiceTest,
       SaveHigherValuesLocallyOnLateDataArrival) {
  remote_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  remote_.UpdateCardStats(BuildCard(kCard1, 3, 4));
  MergeMetadata(&local_, &remote_);
  syncer::SyncChangeList changes;
  changes.push_back(
      BuildChange(syncer::SyncChange::ACTION_UPDATE, kAddr1SyncTag,
                  sync_pb::WalletMetadataSpecifics::ADDRESS, kAddr1Utf8, 5, 6));
  changes.push_back(
      BuildChange(syncer::SyncChange::ACTION_UPDATE, kCard1SyncTag,
                  sync_pb::WalletMetadataSpecifics::CARD, kCard1Utf8, 7, 8));
  local_.ProcessSyncChanges(FROM_HERE, changes);
  local_.UpdateAddressStats(BuildAddress(kAddr1, 0, 0));
  local_.UpdateCardStats(BuildCard(kCard1, 0, 0));

  EXPECT_CALL(local_,
              UpdateAddressStats(AutofillMetadataMatches(kAddr1, 5, 6)));
  EXPECT_CALL(local_, UpdateCardStats(AutofillMetadataMatches(kCard1, 7, 8)));
  EXPECT_CALL(local_, SendChangesToSyncServer(_)).Times(0);

  local_.AutofillMultipleChanged();
}

// Verify that processing a small subset of metadata changes before any Wallet
// data arrived will not cause sending lower metadata values to the sync server
// once the data finally arrives.
TEST_F(AutofillWalletMetadataSyncableServiceTest,
       SaveHigherValuesLocallyOnLateDataArrivalAfterPartialUpdates) {
  remote_.UpdateAddressStats(BuildAddress(kAddr1, 1, 2));
  remote_.UpdateAddressStats(BuildAddress(kAddr2, 3, 4));
  remote_.UpdateCardStats(BuildCard(kCard1, 5, 6));
  remote_.UpdateCardStats(BuildCard(kCard2, 7, 8));
  MergeMetadata(&local_, &remote_);
  syncer::SyncChangeList changes;
  changes.push_back(BuildChange(
      syncer::SyncChange::ACTION_UPDATE, kAddr1SyncTag,
      sync_pb::WalletMetadataSpecifics::ADDRESS, kAddr1Utf8, 9, 10));
  changes.push_back(
      BuildChange(syncer::SyncChange::ACTION_UPDATE, kCard1SyncTag,
                  sync_pb::WalletMetadataSpecifics::CARD, kCard1Utf8, 11, 12));
  changes.push_back(BuildChange(
      syncer::SyncChange::ACTION_UPDATE, kAddr2SyncTag,
      sync_pb::WalletMetadataSpecifics::ADDRESS, kAddr2Utf8, 13, 14));
  changes.push_back(
      BuildChange(syncer::SyncChange::ACTION_UPDATE, kCard2SyncTag,
                  sync_pb::WalletMetadataSpecifics::CARD, kCard2Utf8, 15, 16));
  local_.ProcessSyncChanges(FROM_HERE, changes);
  changes.resize(2);
  local_.ProcessSyncChanges(FROM_HERE, changes);
  local_.UpdateAddressStats(BuildAddress(kAddr1, 0, 0));
  local_.UpdateAddressStats(BuildAddress(kAddr2, 0, 0));
  local_.UpdateCardStats(BuildCard(kCard1, 0, 0));
  local_.UpdateCardStats(BuildCard(kCard2, 0, 0));

  EXPECT_CALL(local_,
              UpdateAddressStats(AutofillMetadataMatches(kAddr1, 9, 10)));
  EXPECT_CALL(local_, UpdateCardStats(AutofillMetadataMatches(kCard1, 11, 12)));
  EXPECT_CALL(local_,
              UpdateAddressStats(AutofillMetadataMatches(kAddr2, 13, 14)));
  EXPECT_CALL(local_, UpdateCardStats(AutofillMetadataMatches(kCard2, 15, 16)));
  EXPECT_CALL(local_, SendChangesToSyncServer(_)).Times(0);

  local_.AutofillMultipleChanged();
}

}  // namespace
}  // namespace autofill
