// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/helium/compound_syncable.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "blimp/helium/helium_test.h"
#include "blimp/helium/lww_register.h"
#include "blimp/helium/revision_generator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace blimp {
namespace helium {
namespace {

class SampleCompoundSyncable : public CompoundSyncable {
 public:
  explicit SampleCompoundSyncable(Peer bias, Peer running_as)
      : child1_(
            CreateAndRegister<LwwRegister<int>>(bias, running_as)),
        child2_(
            CreateAndRegister<LwwRegister<int>>(bias, running_as)) {}

  LwwRegister<int>* mutable_child1() { return child1_.get(); }
  LwwRegister<int>* mutable_child2() { return child2_.get(); }

 private:
  RegisteredMember<LwwRegister<int>> child1_;
  RegisteredMember<LwwRegister<int>> child2_;

  DISALLOW_COPY_AND_ASSIGN(SampleCompoundSyncable);
};

class CompoundSyncableTest : public HeliumTest {
 public:
  CompoundSyncableTest()
      : last_sync_engine_(0),
        engine_(Peer::ENGINE, Peer::ENGINE),
        client_(Peer::ENGINE, Peer::CLIENT) {
    client_.SetLocalUpdateCallback(base::Bind(
        &CompoundSyncableTest::OnClientCallbackCalled, base::Unretained(this)));
    engine_.SetLocalUpdateCallback(base::Bind(
        &CompoundSyncableTest::OnEngineCallbackCalled, base::Unretained(this)));
  }

  ~CompoundSyncableTest() override {}

 public:
  MOCK_METHOD0(OnEngineCallbackCalled, void());
  MOCK_METHOD0(OnClientCallbackCalled, void());

 protected:
  // Propagates pending changes from |engine_| to |client_|.
  void Synchronize() {
    // Create the changeset stream from |engine_|.
    std::string changeset;
    google::protobuf::io::StringOutputStream raw_output_stream(&changeset);
    google::protobuf::io::CodedOutputStream output_stream(&raw_output_stream);

    engine_.CreateChangesetToCurrent(last_sync_engine_, &output_stream);
    EXPECT_FALSE(changeset.empty());
    output_stream.Trim();

    // Apply the changeset stream to |client_|.
    google::protobuf::io::ArrayInputStream raw_input_stream(changeset.data(),
                                                            changeset.size());
    google::protobuf::io::CodedInputStream input_stream(&raw_input_stream);

    last_sync_engine_ = RevisionGenerator::GetInstance()->current();
    EXPECT_EQ(Result::SUCCESS, client_.ApplyChangeset(&input_stream));
    engine_.ReleaseBefore(last_sync_engine_);

    // Ensure EOF.
    EXPECT_FALSE(input_stream.Skip(1));
  }

  Revision last_sync_engine_ = 0;
  SampleCompoundSyncable engine_;
  SampleCompoundSyncable client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CompoundSyncableTest);
};

TEST_F(CompoundSyncableTest, SequentialMutations) {
  EXPECT_CALL(*this, OnClientCallbackCalled()).Times(0);
  EXPECT_CALL(*this, OnEngineCallbackCalled()).Times(2);

  engine_.mutable_child1()->Set(123);
  Synchronize();
  EXPECT_EQ(123, client_.mutable_child1()->Get());

  engine_.mutable_child1()->Set(456);
  Synchronize();
  EXPECT_EQ(456, client_.mutable_child1()->Get());
}

TEST_F(CompoundSyncableTest, MutateMultiple) {
  EXPECT_CALL(*this, OnClientCallbackCalled()).Times(0);
  EXPECT_CALL(*this, OnEngineCallbackCalled()).Times(2);

  engine_.mutable_child1()->Set(123);
  engine_.mutable_child2()->Set(456);
  Synchronize();
  EXPECT_EQ(123, client_.mutable_child1()->Get());
  EXPECT_EQ(456, client_.mutable_child2()->Get());
}

TEST_F(CompoundSyncableTest, MutateMultipleDiscrete) {
  EXPECT_CALL(*this, OnClientCallbackCalled()).Times(0);
  EXPECT_CALL(*this, OnEngineCallbackCalled()).Times(2);

  engine_.mutable_child1()->Set(123);
  Synchronize();
  EXPECT_EQ(123, client_.mutable_child1()->Get());
  engine_.mutable_child2()->Set(456);
  Synchronize();
  EXPECT_EQ(123, client_.mutable_child1()->Get());
  EXPECT_EQ(456, client_.mutable_child2()->Get());
}

}  // namespace
}  // namespace helium
}  // namespace blimp
