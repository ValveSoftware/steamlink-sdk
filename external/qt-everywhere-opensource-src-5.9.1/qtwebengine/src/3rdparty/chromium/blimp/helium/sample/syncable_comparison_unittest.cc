// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Sample test suite which defines some basic workflows on a sample Input
// feature in a Syncable-agnostic manner. The workflows are applied equally to
// two types of Syncable-derived objects to ensure parity.

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "blimp/helium/compound_syncable.h"
#include "blimp/helium/lww_register.h"
#include "blimp/helium/sample/proto_lww_register.h"
#include "blimp/helium/sample/proto_syncable.h"
#include "blimp/helium/sample/sample.pb.h"
#include "blimp/helium/syncable_common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace blimp {
namespace helium {
namespace {

// Abstract accessors and setters for State object members.
class InputMethodState {
 public:
  virtual void SetInputGeneration(int generation) = 0;
  virtual int GetInputGeneration() const = 0;

  virtual void SetInputType(int type) = 0;
  virtual int GetInputType() const = 0;

  virtual void SetResponseGeneration(int generation) = 0;
  virtual int GetResponseGeneration() const = 0;

  virtual void SetResponse(const std::string& response) = 0;
  virtual std::string GetResponse() const = 0;
};

// Presents abstractions of Syncable objects and an impl-agnostic hook for
// synchronizing peers.
class SyncableConfig {
 public:
  enum SyncableType { CODED_STREAM, TEMPLATED };

  virtual ~SyncableConfig() {}

  virtual InputMethodState* GetClientSyncable() = 0;
  virtual InputMethodState* GetEngineSyncable() = 0;
  virtual void Synchronize() = 0;

  virtual size_t BytesSent() = 0;
  virtual size_t BytesReceived() = 0;
};

// SYNCABLE CONFIG IMPLEMENTATIONS

class CodedStreamSyncableConfig : public SyncableConfig {
 public:
  CodedStreamSyncableConfig()
      : client_syncable_(Peer::CLIENT), engine_syncable_(Peer::ENGINE) {}

  ~CodedStreamSyncableConfig() override {}

  InputMethodState* GetClientSyncable() override { return &client_syncable_; }

  InputMethodState* GetEngineSyncable() override { return &engine_syncable_; }

  void Synchronize() override {
    bytes_sent_ += SynchronizeOneWay(last_sync_client_, &client_syncable_,
                                     &engine_syncable_);
    bytes_received_ += SynchronizeOneWay(last_sync_engine_, &engine_syncable_,
                                         &client_syncable_);

    last_sync_client_ = RevisionGenerator::GetInstance()->current();
    last_sync_engine_ = RevisionGenerator::GetInstance()->current();
  }

  size_t BytesSent() override { return bytes_sent_; }

  size_t BytesReceived() override { return bytes_received_; }

 private:
  class InputMethodStateSyncable : public CompoundSyncable,
                                   public InputMethodState {
   public:
    explicit InputMethodStateSyncable(Peer running_as)
        : input_generation_(
              CreateAndRegister<LwwRegister<int>>(Peer::ENGINE, running_as)),
          input_type_(
              CreateAndRegister<LwwRegister<int>>(Peer::ENGINE, running_as)),
          response_generation_(
              CreateAndRegister<LwwRegister<int>>(Peer::CLIENT, running_as)),
          response_(CreateAndRegister<LwwRegister<std::string>>(Peer::CLIENT,
                                                                running_as)) {
      SetLocalUpdateCallback(base::Bind(&base::DoNothing));
    }
    ~InputMethodStateSyncable() override {}

    void SetInputGeneration(int generation) override {
      input_generation_->Set(generation);
    }
    int GetInputGeneration() const override { return input_generation_->Get(); }
    void SetInputType(int type) override { input_type_->Set(type); }
    int GetInputType() const override { return input_type_->Get(); }
    void SetResponseGeneration(int generation) override {
      response_generation_->Set(generation);
    }
    int GetResponseGeneration() const override {
      return response_generation_->Get();
    }
    void SetResponse(const std::string& response) override {
      response_->Set(response);
    }
    std::string GetResponse() const override { return response_->Get(); }

   private:
    RegisteredMember<LwwRegister<int>> input_generation_;
    RegisteredMember<LwwRegister<int>> input_type_;
    RegisteredMember<LwwRegister<int>> response_generation_;
    RegisteredMember<LwwRegister<std::string>> response_;

    DISALLOW_COPY_AND_ASSIGN(InputMethodStateSyncable);
  };

  // Synchronizes changes from |source| to |dest|.
  size_t SynchronizeOneWay(Revision last_sync,
                           InputMethodStateSyncable* source,
                           InputMethodStateSyncable* dest) {
    if (source->GetRevision() <= last_sync) {
      return 0u;  // Changeset not computed; no bytes transferred.
    }

    std::string changeset;
    google::protobuf::io::StringOutputStream raw_output_stream(&changeset);
    google::protobuf::io::CodedOutputStream output_stream(&raw_output_stream);
    source->CreateChangesetToCurrent(last_sync_engine_, &output_stream);
    CHECK(!changeset.empty());
    output_stream.Trim();

    google::protobuf::io::ArrayInputStream raw_input_stream(changeset.data(),
                                                            changeset.size());
    google::protobuf::io::CodedInputStream input_stream(&raw_input_stream);
    CHECK_EQ(Result::SUCCESS, dest->ApplyChangeset(&input_stream));
    return changeset.size();
  }

  InputMethodStateSyncable client_syncable_;
  InputMethodStateSyncable engine_syncable_;
  Revision last_sync_client_ = 0u;
  Revision last_sync_engine_ = 0u;
  size_t bytes_sent_ = 0u;
  size_t bytes_received_ = 0u;

  DISALLOW_COPY_AND_ASSIGN(CodedStreamSyncableConfig);
};

class TemplatedSyncableConfig : public SyncableConfig {
 public:
  TemplatedSyncableConfig()
      : client_syncable_(Peer::CLIENT), engine_syncable_(Peer::ENGINE) {}

  ~TemplatedSyncableConfig() override {}

  InputMethodState* GetClientSyncable() override { return &client_syncable_; }

  InputMethodState* GetEngineSyncable() override { return &engine_syncable_; }

  void Synchronize() override {
    bytes_sent_ += SynchronizeOneWay(last_sync_client_, &client_syncable_,
                                     &engine_syncable_);
    bytes_received_ += SynchronizeOneWay(last_sync_engine_, &engine_syncable_,
                                         &client_syncable_);

    last_sync_client_ = RevisionGenerator::GetInstance()->current();
    last_sync_engine_ = RevisionGenerator::GetInstance()->current();
  }

  size_t BytesSent() override { return bytes_sent_; }

  size_t BytesReceived() override { return bytes_received_; }

 private:
  class InputMethodStateSyncable
      : public ProtoSyncable<InputMethodChangesetMessage>,
        public InputMethodState {
   public:
    explicit InputMethodStateSyncable(Peer running_as)
        : input_generation_(
              base::MakeUnique<ProtoLwwRegister<int>>(Peer::ENGINE,
                                                      running_as)),
          input_type_(base::MakeUnique<ProtoLwwRegister<int>>(Peer::ENGINE,
                                                              running_as)),
          response_generation_(
              base::MakeUnique<ProtoLwwRegister<int>>(Peer::CLIENT,
                                                      running_as)),
          response_(
              base::MakeUnique<ProtoLwwRegister<std::string>>(Peer::CLIENT,
                                                              running_as)) {
      members_.push_back(input_generation_.get());
      members_.push_back(input_type_.get());
      members_.push_back(response_generation_.get());
      members_.push_back(response_.get());
    }
    ~InputMethodStateSyncable() override {}

    // ProtoSyncable implementation.
    std::unique_ptr<InputMethodChangesetMessage> CreateChangesetToCurrent(
        Revision from) override {
      std::unique_ptr<InputMethodChangesetMessage> changeset =
          base::MakeUnique<InputMethodChangesetMessage>();
      changeset->set_allocated_input_generation(
          input_generation_->CreateChangesetToCurrent(from).release());
      changeset->set_allocated_input_type(
          input_type_->CreateChangesetToCurrent(from).release());
      changeset->set_allocated_response_generation(
          response_generation_->CreateChangesetToCurrent(from).release());
      changeset->set_allocated_response(
          response_->CreateChangesetToCurrent(from).release());
      return changeset;
    }

    Result ApplyChangeset(
        Revision to,
        std::unique_ptr<InputMethodChangesetMessage> changeset) override {
      std::vector<Result> results;
      results.push_back(input_generation_->ApplyChangeset(
          to, base::WrapUnique<LwwRegisterChangesetMessage>(
                  changeset->release_input_generation())));
      results.push_back(input_type_->ApplyChangeset(
          to, base::WrapUnique<LwwRegisterChangesetMessage>(
                  changeset->release_input_type())));
      results.push_back(response_generation_->ApplyChangeset(
          to, base::WrapUnique<LwwRegisterChangesetMessage>(
                  changeset->release_response_generation())));
      results.push_back(response_->ApplyChangeset(
          to, base::WrapUnique<LwwRegisterChangesetMessage>(
                  changeset->release_response())));
      for (Result result : results) {
        if (result != Result::SUCCESS) {
          return result;
        }
      }
      return Result::SUCCESS;
    }

    void ReleaseBefore(Revision checkpoint) override {
      for (auto& member : members_) {
        member->ReleaseBefore(checkpoint);
      }
    }

    VersionVector GetVersionVector() const override {
      VersionVector merged;
      for (const auto& member : members_) {
        merged = merged.MergeWith(member->GetVersionVector());
      }
      return merged;
    }

    void SetInputGeneration(int generation) override {
      input_generation_->Set(generation);
    }
    int GetInputGeneration() const override { return input_generation_->Get(); }
    void SetInputType(int type) override { input_type_->Set(type); }
    int GetInputType() const override { return input_type_->Get(); }
    void SetResponseGeneration(int generation) override {
      response_generation_->Set(generation);
    }
    int GetResponseGeneration() const override {
      return response_generation_->Get();
    }
    void SetResponse(const std::string& response) override {
      response_->Set(response);
    }
    std::string GetResponse() const override { return response_->Get(); }

   private:
    std::unique_ptr<ProtoLwwRegister<int>> input_generation_;
    std::unique_ptr<ProtoLwwRegister<int>> input_type_;
    std::unique_ptr<ProtoLwwRegister<int>> response_generation_;
    std::unique_ptr<ProtoLwwRegister<std::string>> response_;

    std::vector<ProtoSyncable<LwwRegisterChangesetMessage>*> members_;

    DISALLOW_COPY_AND_ASSIGN(InputMethodStateSyncable);
  };

  // Synchronizes changes from |source| to |dest|.
  size_t SynchronizeOneWay(Revision last_sync,
                           InputMethodStateSyncable* source,
                           InputMethodStateSyncable* dest) {
    if (source->GetVersionVector().local_revision() <= last_sync) {
      return 0u;  // Changeset not computed; no bytes transferred.
    }

    std::unique_ptr<InputMethodChangesetMessage> changeset =
        source->CreateChangesetToCurrent(last_sync_engine_);
    size_t changeset_size = changeset->ByteSize();
    CHECK_EQ(Result::SUCCESS,
             dest->ApplyChangeset(last_sync_engine_, std::move(changeset)));
    return changeset_size;
  }

  InputMethodStateSyncable client_syncable_;
  InputMethodStateSyncable engine_syncable_;
  Revision last_sync_client_ = 0u;
  Revision last_sync_engine_ = 0u;
  size_t bytes_sent_ = 0u;
  size_t bytes_received_ = 0u;

  DISALLOW_COPY_AND_ASSIGN(TemplatedSyncableConfig);
};

class InputMethodSampleTest
    : public testing::TestWithParam<SyncableConfig::SyncableType> {
 public:
  InputMethodSampleTest() {}
  ~InputMethodSampleTest() override {}

  void SetUp() override {
    // Use the test param provided by INSTANTIATE_TEST_CASE_P to configure this
    // test suite to use either templated proto or coded stream definitions.
    switch (GetParam()) {
      case SyncableConfig::TEMPLATED:
        config_ = base::MakeUnique<TemplatedSyncableConfig>();
        break;
      case SyncableConfig::CODED_STREAM:
        config_ = base::MakeUnique<CodedStreamSyncableConfig>();
        break;
    }
  }

  void TearDown() override {
    VLOG(0) << "Bytes sent: " << config_->BytesSent();
    VLOG(0) << "Bytes received: " << config_->BytesReceived();
  }

 protected:
  std::unique_ptr<SyncableConfig> config_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMethodSampleTest);
};

// TEST SCENARIOS

TEST_P(InputMethodSampleTest, Collisions) {
  // Client wins; should synchronize to 123.
  config_->GetClientSyncable()->SetResponseGeneration(123);
  config_->GetEngineSyncable()->SetResponseGeneration(456);

  // Engine wins; should synchronize to 456.
  config_->GetClientSyncable()->SetInputGeneration(123);
  config_->GetEngineSyncable()->SetInputGeneration(456);

  config_->Synchronize();

  EXPECT_EQ(123, config_->GetClientSyncable()->GetResponseGeneration());
  EXPECT_EQ(123, config_->GetEngineSyncable()->GetResponseGeneration());

  EXPECT_EQ(456, config_->GetClientSyncable()->GetInputGeneration());
  EXPECT_EQ(456, config_->GetEngineSyncable()->GetInputGeneration());
}

// Run the entire test suite for CODED_STREAM and TEMPLATED configs.
INSTANTIATE_TEST_CASE_P(SyncableComparisonSuite,
                        InputMethodSampleTest,
                        ::testing::Values(SyncableConfig::CODED_STREAM,
                                          SyncableConfig::TEMPLATED));

}  // namespace
}  // namespace helium
}  // namespace blimp
