// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/shared_memory_received_data_factory.h"

#include <stddef.h>
#include <tuple>

#include "content/common/resource_messages.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;

using Checkpoint = StrictMock<MockFunction<void(int)>>;
using ReceivedData = RequestPeer::ReceivedData;

class MockSender : public IPC::Sender {
 public:
  bool Send(IPC::Message* message) override {
    bool result = false;
    if (message->type() == ResourceHostMsg_DataReceived_ACK::ID) {
      std::tuple<int> args;
      ResourceHostMsg_DataReceived_ACK::Read(message, &args);
      result = SendAck(std::get<0>(args));
    } else {
      result = SendOtherwise(message);
    }
    delete message;
    return result;
  }
  MOCK_METHOD1(SendAck, bool(int));
  MOCK_METHOD1(SendOtherwise, bool(IPC::Message*));
};

class SharedMemoryReceivedDataFactoryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    sender_.reset(new StrictMock<MockSender>);
    request_id_ = 0xdeadbeaf;
    memory_.reset(new base::SharedMemory);
    factory_ = make_scoped_refptr(new SharedMemoryReceivedDataFactory(
        sender_.get(), request_id_, memory_));
    ASSERT_TRUE(memory_->CreateAndMapAnonymous(memory_size));

    ON_CALL(*sender_, SendAck(_)).WillByDefault(Return(true));
    ON_CALL(*sender_, SendOtherwise(_)).WillByDefault(Return(true));
  }

  static const size_t memory_size = 4 * 1024;
  std::unique_ptr<MockSender> sender_;
  int request_id_;
  linked_ptr<base::SharedMemory> memory_;
  scoped_refptr<SharedMemoryReceivedDataFactory> factory_;
};

TEST_F(SharedMemoryReceivedDataFactoryTest, Create) {
  Checkpoint checkpoint;
  InSequence s;
  EXPECT_CALL(checkpoint, Call(0));
  EXPECT_CALL(*sender_, SendAck(request_id_));
  EXPECT_CALL(checkpoint, Call(1));

  std::unique_ptr<ReceivedData> data = factory_->Create(12, 34, 56);
  const char* memory_start = static_cast<const char*>(memory_->memory());

  ASSERT_TRUE(data);
  EXPECT_EQ(memory_start + 12, data->payload());
  EXPECT_EQ(34, data->length());
  EXPECT_EQ(56, data->encoded_length());

  checkpoint.Call(0);
  data.reset();
  checkpoint.Call(1);
}

TEST_F(SharedMemoryReceivedDataFactoryTest, CreateMultiple) {
  Checkpoint checkpoint;
  InSequence s;
  EXPECT_CALL(checkpoint, Call(0));
  EXPECT_CALL(*sender_, SendAck(request_id_));
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*sender_, SendAck(request_id_));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*sender_, SendAck(request_id_));
  EXPECT_CALL(checkpoint, Call(3));

  std::unique_ptr<ReceivedData> data1 = factory_->Create(0, 1, 1);
  std::unique_ptr<ReceivedData> data2 = factory_->Create(1, 1, 1);
  std::unique_ptr<ReceivedData> data3 = factory_->Create(2, 1, 1);

  EXPECT_TRUE(data1);
  EXPECT_TRUE(data2);
  EXPECT_TRUE(data3);

  checkpoint.Call(0);
  data1.reset();
  checkpoint.Call(1);
  data2.reset();
  checkpoint.Call(2);
  data3.reset();
  checkpoint.Call(3);
}

TEST_F(SharedMemoryReceivedDataFactoryTest, ReclaimOutOfOrder) {
  Checkpoint checkpoint;
  InSequence s;
  EXPECT_CALL(checkpoint, Call(0));
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(*sender_, SendAck(request_id_));
  EXPECT_CALL(*sender_, SendAck(request_id_));
  EXPECT_CALL(*sender_, SendAck(request_id_));
  EXPECT_CALL(checkpoint, Call(3));

  std::unique_ptr<ReceivedData> data1 = factory_->Create(0, 1, 1);
  std::unique_ptr<ReceivedData> data2 = factory_->Create(1, 1, 1);
  std::unique_ptr<ReceivedData> data3 = factory_->Create(2, 1, 1);

  EXPECT_TRUE(data1);
  EXPECT_TRUE(data2);
  EXPECT_TRUE(data3);

  checkpoint.Call(0);
  data3.reset();
  checkpoint.Call(1);
  data2.reset();
  checkpoint.Call(2);
  data1.reset();
  checkpoint.Call(3);
}

TEST_F(SharedMemoryReceivedDataFactoryTest, ReclaimOutOfOrderPartially) {
  Checkpoint checkpoint;
  InSequence s;
  EXPECT_CALL(checkpoint, Call(0));
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*sender_, SendAck(request_id_));
  EXPECT_CALL(*sender_, SendAck(request_id_));
  EXPECT_CALL(*sender_, SendAck(request_id_));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*sender_, SendAck(request_id_));
  EXPECT_CALL(*sender_, SendAck(request_id_));
  EXPECT_CALL(*sender_, SendAck(request_id_));
  EXPECT_CALL(checkpoint, Call(4));

  std::unique_ptr<ReceivedData> data1 = factory_->Create(0, 1, 1);
  std::unique_ptr<ReceivedData> data2 = factory_->Create(1, 1, 1);
  std::unique_ptr<ReceivedData> data3 = factory_->Create(2, 1, 1);
  std::unique_ptr<ReceivedData> data4 = factory_->Create(3, 1, 1);
  std::unique_ptr<ReceivedData> data5 = factory_->Create(4, 1, 1);
  std::unique_ptr<ReceivedData> data6 = factory_->Create(5, 1, 1);

  EXPECT_TRUE(data1);
  EXPECT_TRUE(data2);
  EXPECT_TRUE(data3);
  EXPECT_TRUE(data4);
  EXPECT_TRUE(data5);
  EXPECT_TRUE(data6);

  checkpoint.Call(0);
  data3.reset();
  data6.reset();
  data2.reset();
  checkpoint.Call(1);
  data1.reset();
  checkpoint.Call(2);
  data5.reset();
  checkpoint.Call(3);
  data4.reset();
  checkpoint.Call(4);
}

TEST_F(SharedMemoryReceivedDataFactoryTest, Stop) {
  Checkpoint checkpoint;
  InSequence s;
  EXPECT_CALL(checkpoint, Call(0));
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*sender_, SendAck(request_id_));
  EXPECT_CALL(*sender_, SendAck(request_id_));
  EXPECT_CALL(*sender_, SendAck(request_id_));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));

  std::unique_ptr<ReceivedData> data1 = factory_->Create(0, 1, 1);
  std::unique_ptr<ReceivedData> data2 = factory_->Create(1, 1, 1);
  std::unique_ptr<ReceivedData> data3 = factory_->Create(2, 1, 1);
  std::unique_ptr<ReceivedData> data4 = factory_->Create(3, 1, 1);
  std::unique_ptr<ReceivedData> data5 = factory_->Create(4, 1, 1);
  std::unique_ptr<ReceivedData> data6 = factory_->Create(5, 1, 1);

  EXPECT_TRUE(data1);
  EXPECT_TRUE(data2);
  EXPECT_TRUE(data3);
  EXPECT_TRUE(data4);
  EXPECT_TRUE(data5);
  EXPECT_TRUE(data6);

  checkpoint.Call(0);
  data3.reset();
  data6.reset();
  data2.reset();
  checkpoint.Call(1);
  data1.reset();
  checkpoint.Call(2);
  factory_->Stop();
  data5.reset();
  data4.reset();
  checkpoint.Call(3);
}

}  // namespace

}  // namespace content
