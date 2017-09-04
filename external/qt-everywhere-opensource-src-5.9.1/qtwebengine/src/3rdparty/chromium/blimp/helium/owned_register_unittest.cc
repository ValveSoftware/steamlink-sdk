// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/helium/owned_register.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "blimp/helium/coded_value_serializer.h"
#include "blimp/helium/helium_test.h"
#include "blimp/helium/syncable_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace blimp {
namespace helium {
namespace {

class OwnedRegisterTest : public HeliumTest {
 public:
  OwnedRegisterTest() {}

  MOCK_METHOD0(OnEngineCallbackCalled, void());
  MOCK_METHOD0(OnClientCallbackCalled, void());

 protected:
  void InitRegisters(Peer owner) {
    client_reg_ = base::MakeUnique<OwnedRegister<int>>(Peer::CLIENT, owner);
    engine_reg_ = base::MakeUnique<OwnedRegister<int>>(Peer::ENGINE, owner);
    client_reg_->SetLocalUpdateCallback(base::Bind(
        &OwnedRegisterTest::OnClientCallbackCalled, base::Unretained(this)));
    engine_reg_->SetLocalUpdateCallback(base::Bind(
        &OwnedRegisterTest::OnEngineCallbackCalled, base::Unretained(this)));
  }

  Result SyncFromClient() {
    return Sync(client_reg_.get(), engine_reg_.get(),
                client_reg_->GetRevision());
  }

  Result SyncFromEngine() {
    return Sync(engine_reg_.get(), client_reg_.get(),
                engine_reg_->GetRevision());
  }

  Result Sync(OwnedRegister<int>* from_register,
              OwnedRegister<int>* to_register,
              Revision from);

  Result ApplyMockData(OwnedRegister<int>* to_register,
                       Revision revision,
                       int value);

  std::unique_ptr<OwnedRegister<int>> client_reg_;
  std::unique_ptr<OwnedRegister<int>> engine_reg_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OwnedRegisterTest);
};

// Takes a changeset from |from_register| and applies it to
// |to_lww_register|.
Result OwnedRegisterTest::Sync(OwnedRegister<int>* from_register,
                               OwnedRegister<int>* to_register,
                               Revision from) {
  // Create a changeset from |from_register|.
  std::string changeset;
  google::protobuf::io::StringOutputStream raw_output_stream(&changeset);
  google::protobuf::io::CodedOutputStream output_stream(&raw_output_stream);
  from_register->CreateChangesetToCurrent(from, &output_stream);

  // Apply the changeset to |to_register|.
  google::protobuf::io::ArrayInputStream raw_input_stream(changeset.data(),
                                                          changeset.size());
  google::protobuf::io::CodedInputStream input_stream(&raw_input_stream);
  return to_register->ApplyChangeset(&input_stream);
}

Result OwnedRegisterTest::ApplyMockData(OwnedRegister<int>* to_register,
                                        Revision revision,
                                        int value) {
  // Create changeset
  std::string changeset;
  google::protobuf::io::StringOutputStream raw_output_stream(&changeset);
  google::protobuf::io::CodedOutputStream output_stream(&raw_output_stream);
  CodedValueSerializer::Serialize(revision, &output_stream);
  CodedValueSerializer::Serialize(value, &output_stream);

  // Apply the changeset.
  google::protobuf::io::ArrayInputStream raw_input_stream(changeset.data(),
                                                          changeset.size());
  google::protobuf::io::CodedInputStream input_stream(&raw_input_stream);
  return to_register->ApplyChangeset(&input_stream);
}

TEST_F(OwnedRegisterTest, SetIncrementsLocalVersion) {
  InitRegisters(Peer::ENGINE);

  EXPECT_CALL(*this, OnClientCallbackCalled()).Times(0);
  EXPECT_CALL(*this, OnEngineCallbackCalled()).Times(1);

  Revision client_rev = client_reg_->GetRevision();
  Revision engine_rev = engine_reg_->GetRevision();
  engine_reg_->Set(175);
  EXPECT_EQ(175, engine_reg_->Get());

  EXPECT_EQ(client_reg_->GetRevision(), client_rev);
  EXPECT_GT(engine_reg_->GetRevision(), engine_rev);
}

TEST_F(OwnedRegisterTest, SyncSuccessfully) {
  InitRegisters(Peer::CLIENT);

  EXPECT_CALL(*this, OnClientCallbackCalled()).Times(1);
  EXPECT_CALL(*this, OnEngineCallbackCalled()).Times(0);

  Revision client_rev = client_reg_->GetRevision();
  Revision engine_rev = engine_reg_->GetRevision();
  client_reg_->Set(123);

  EXPECT_EQ(Result::SUCCESS, SyncFromClient());
  EXPECT_EQ(123, engine_reg_->Get());

  EXPECT_GT(client_reg_->GetRevision(), client_rev);
  EXPECT_EQ(engine_reg_->GetRevision(), engine_rev);
}

TEST_F(OwnedRegisterTest, ChangesetIgnored) {
  InitRegisters(Peer::CLIENT);

  EXPECT_CALL(*this, OnClientCallbackCalled()).Times(1);
  EXPECT_CALL(*this, OnEngineCallbackCalled()).Times(0);

  Revision engine_rev = engine_reg_->GetRevision();
  client_reg_->Set(123);
  EXPECT_EQ(Result::SUCCESS, SyncFromClient());
  EXPECT_EQ(123, engine_reg_->Get());

  EXPECT_EQ(Result::SUCCESS, SyncFromClient());
  EXPECT_EQ(123, engine_reg_->Get());

  EXPECT_EQ(engine_reg_->GetRevision(), engine_rev);
}

TEST_F(OwnedRegisterTest, ChangesetsAppliedOutOfOrderFails) {
  InitRegisters(Peer::CLIENT);

  EXPECT_CALL(*this, OnClientCallbackCalled()).Times(1);
  EXPECT_CALL(*this, OnEngineCallbackCalled()).Times(0);

  // Set up the Engine to be at 0:1 revision
  client_reg_->Set(123);

  EXPECT_EQ(Result::SUCCESS, SyncFromClient());
  EXPECT_EQ(123, engine_reg_->Get());

  int value = 6;
  Revision lesser_revision = 0;

  EXPECT_EQ(Result::ERR_PROTOCOL_ERROR,
            ApplyMockData(engine_reg_.get(), lesser_revision, value));
}

TEST_F(OwnedRegisterTest, InvalidOperationForPeer) {
  InitRegisters(Peer::ENGINE);

  EXPECT_CALL(*this, OnClientCallbackCalled()).Times(0);
  EXPECT_CALL(*this, OnEngineCallbackCalled()).Times(0);

  int value = 123;
  Revision revision = 1;
  EXPECT_EQ(Result::ERR_PROTOCOL_ERROR,
            ApplyMockData(engine_reg_.get(), revision, value));
}

}  // namespace
}  // namespace helium
}  // namespace blimp
