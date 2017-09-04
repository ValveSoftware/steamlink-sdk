// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/helium/lww_register.h"

#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "blimp/helium/helium_test.h"
#include "blimp/helium/revision_generator.h"
#include "blimp/helium/version_vector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace blimp {
namespace helium {
namespace {

class LwwRegisterTest : public HeliumTest {
 public:
  LwwRegisterTest() {}
  ~LwwRegisterTest() override = default;

  MOCK_METHOD0(OnEngineCallbackCalled, void());
  MOCK_METHOD0(OnClientCallbackCalled, void());

 protected:
  void Initialize(Peer bias) {
    client_ = base::MakeUnique<LwwRegister<int>>(bias, Peer::CLIENT);
    engine_ = base::MakeUnique<LwwRegister<int>>(bias, Peer::ENGINE);

    client_->SetLocalUpdateCallback(base::Bind(
        &LwwRegisterTest::OnClientCallbackCalled, base::Unretained(this)));
    engine_->SetLocalUpdateCallback(base::Bind(
        &LwwRegisterTest::OnEngineCallbackCalled, base::Unretained(this)));
  }

  void SyncFromClient() {
    Sync(client_.get(), engine_.get(), client_->GetRevision());
  }

  void SyncFromEngine() {
    Sync(engine_.get(), client_.get(), engine_->GetRevision());
  }

  void Sync(LwwRegister<int>* from_lww_register,
            LwwRegister<int>* to_lww_register,
            Revision from);

  std::unique_ptr<LwwRegister<int>> client_;
  std::unique_ptr<LwwRegister<int>> engine_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LwwRegisterTest);
};

// Takes a changeset from |from_lww_register| and applies it to
// |to_lww_register|.
void LwwRegisterTest::Sync(LwwRegister<int>* from_lww_register,
                           LwwRegister<int>* to_lww_register,
                           Revision from) {
  // Create a changeset from |from_lww_register|.
  std::string changeset;
  google::protobuf::io::StringOutputStream raw_output_stream(&changeset);
  google::protobuf::io::CodedOutputStream output_stream(&raw_output_stream);
  from_lww_register->CreateChangesetToCurrent(from, &output_stream);

  // Apply the changeset to |to_lww_register|.
  google::protobuf::io::ArrayInputStream raw_input_stream(changeset.data(),
                                                          changeset.size());
  google::protobuf::io::CodedInputStream input_stream(&raw_input_stream);
  to_lww_register->ApplyChangeset(&input_stream);
}

TEST_F(LwwRegisterTest, SetIncrementsLocalVersion) {
  Initialize(Peer::CLIENT);

  EXPECT_CALL(*this, OnClientCallbackCalled()).Times(1);
  EXPECT_CALL(*this, OnEngineCallbackCalled()).Times(0);

  Revision earlier_version = client_->GetRevision();
  client_->Set(42);
  Revision current_version = client_->GetRevision();

  EXPECT_EQ(42, client_->Get());
  EXPECT_LT(earlier_version, current_version);
}

TEST_F(LwwRegisterTest, ApplyLaterChangeset) {
  Initialize(Peer::CLIENT);

  EXPECT_CALL(*this, OnClientCallbackCalled()).Times(1);
  EXPECT_CALL(*this, OnEngineCallbackCalled()).Times(0);

  client_->Set(123);
  SyncFromClient();

  EXPECT_EQ(123, engine_->Get());
}

TEST_F(LwwRegisterTest, ApplyEarlierChangeset) {
  Initialize(Peer::CLIENT);

  EXPECT_CALL(*this, OnClientCallbackCalled()).Times(1);
  EXPECT_CALL(*this, OnEngineCallbackCalled()).Times(1);

  client_->Set(123);
  SyncFromClient();

  engine_->Set(456);
  SyncFromClient();

  EXPECT_EQ(456, engine_->Get());
}

TEST_F(LwwRegisterTest, ClientApplyChangesetConflictClientWins) {
  Initialize(Peer::CLIENT);

  EXPECT_CALL(*this, OnClientCallbackCalled()).Times(1);
  EXPECT_CALL(*this, OnEngineCallbackCalled()).Times(1);

  client_->Set(123);
  engine_->Set(456);
  SyncFromEngine();

  EXPECT_EQ(123, client_->Get());
}

TEST_F(LwwRegisterTest, EngineApplyChangesetConflictClientWins) {
  Initialize(Peer::CLIENT);

  EXPECT_CALL(*this, OnClientCallbackCalled()).Times(1);
  EXPECT_CALL(*this, OnEngineCallbackCalled()).Times(1);

  client_->Set(123);
  engine_->Set(456);
  SyncFromClient();

  EXPECT_EQ(123, engine_->Get());
}

TEST_F(LwwRegisterTest, ClientApplyChangesetConflictEngineWins) {
  Initialize(Peer::ENGINE);

  EXPECT_CALL(*this, OnClientCallbackCalled()).Times(1);
  EXPECT_CALL(*this, OnEngineCallbackCalled()).Times(1);

  client_->Set(123);
  engine_->Set(456);
  SyncFromEngine();

  EXPECT_EQ(456, client_->Get());
}

TEST_F(LwwRegisterTest, EngineApplyChangesetConflictEngineWins) {
  Initialize(Peer::ENGINE);

  EXPECT_CALL(*this, OnClientCallbackCalled()).Times(1);
  EXPECT_CALL(*this, OnEngineCallbackCalled()).Times(1);

  client_->Set(123);
  engine_->Set(456);
  SyncFromClient();

  EXPECT_EQ(456, engine_->Get());
}

}  // namespace
}  // namespace helium
}  // namespace blimp
