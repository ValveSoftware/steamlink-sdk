// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blimp_connection.h"

#include <stddef.h>

#include <string>

#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/common.h"
#include "blimp/net/connection_error_observer.h"
#include "blimp/net/test_common.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;
using testing::Return;
using testing::SaveArg;

namespace blimp {
namespace {

class BlimpConnectionTest : public testing::Test {
 public:
  BlimpConnectionTest() {
    std::unique_ptr<testing::StrictMock<MockPacketWriter>> writer(
        new testing::StrictMock<MockPacketWriter>);
    writer_ = writer.get();
    std::unique_ptr<testing::StrictMock<MockPacketReader>> reader(
        new testing::StrictMock<MockPacketReader>);
    reader_ = reader.get();
    connection_.reset(
        new BlimpConnection(std::move(reader), std::move(writer)));
    connection_->AddConnectionErrorObserver(&error_observer1_);
    connection_->AddConnectionErrorObserver(&error_observer2_);
    connection_->AddConnectionErrorObserver(&error_observer3_);
    connection_->RemoveConnectionErrorObserver(&error_observer3_);
  }

  ~BlimpConnectionTest() override {}

  void DropConnection() { connection_.reset(); }

 protected:
  std::unique_ptr<BlimpMessage> CreateInputMessage() {
    InputMessage* input;
    return CreateBlimpMessage(&input);
  }

  std::unique_ptr<BlimpMessage> CreateControlMessage() {
    TabControlMessage* control;
    return CreateBlimpMessage(&control);
  }

  base::MessageLoop message_loop_;
  testing::StrictMock<MockPacketReader>* reader_;
  testing::StrictMock<MockPacketWriter>* writer_;
  testing::StrictMock<MockConnectionErrorObserver> error_observer1_;
  testing::StrictMock<MockConnectionErrorObserver> error_observer2_;

  // This error observer is Removed() immediately after it's added;
  // it should never be called.
  testing::StrictMock<MockConnectionErrorObserver> error_observer3_;

  testing::StrictMock<MockBlimpMessageProcessor> receiver_;
  std::unique_ptr<BlimpConnection> connection_;
};

// Write completes writing two packets asynchronously.
TEST_F(BlimpConnectionTest, AsyncTwoPacketsWrite) {
  net::CompletionCallback write_packet_cb;

  InSequence s;
  EXPECT_CALL(*writer_,
              WritePacket(BufferEqualsProto(*CreateInputMessage()), _))
      .WillOnce(SaveArg<1>(&write_packet_cb))
      .RetiresOnSaturation();
  EXPECT_CALL(*writer_,
              WritePacket(BufferEqualsProto(*CreateControlMessage()), _))
      .WillOnce(SaveArg<1>(&write_packet_cb))
      .RetiresOnSaturation();
  EXPECT_CALL(error_observer1_, OnConnectionError(_)).Times(0);
  EXPECT_CALL(error_observer2_, OnConnectionError(_)).Times(0);
  EXPECT_CALL(error_observer3_, OnConnectionError(_)).Times(0);

  BlimpMessageProcessor* sender = connection_->GetOutgoingMessageProcessor();
  net::TestCompletionCallback complete_cb_1;
  ASSERT_TRUE(write_packet_cb.is_null());
  sender->ProcessMessage(CreateInputMessage(),
                         complete_cb_1.callback());
  ASSERT_FALSE(write_packet_cb.is_null());
  base::ResetAndReturn(&write_packet_cb).Run(net::OK);
  EXPECT_EQ(net::OK, complete_cb_1.WaitForResult());

  net::TestCompletionCallback complete_cb_2;
  ASSERT_TRUE(write_packet_cb.is_null());
  sender->ProcessMessage(CreateControlMessage(),
                         complete_cb_2.callback());
  ASSERT_FALSE(write_packet_cb.is_null());
  base::ResetAndReturn(&write_packet_cb).Run(net::OK);
  EXPECT_EQ(net::OK, complete_cb_2.WaitForResult());
}

// Writer completes writing two packets asynchronously.
// First write succeeds, second fails.
TEST_F(BlimpConnectionTest, AsyncTwoPacketsWriteWithError) {
  net::CompletionCallback write_packet_cb;

  InSequence s;
  EXPECT_CALL(*writer_,
              WritePacket(BufferEqualsProto(*CreateInputMessage()), _))
      .WillOnce(SaveArg<1>(&write_packet_cb))
      .RetiresOnSaturation();
  EXPECT_CALL(*writer_,
              WritePacket(BufferEqualsProto(*CreateControlMessage()), _))
      .WillOnce(SaveArg<1>(&write_packet_cb))
      .RetiresOnSaturation();
  EXPECT_CALL(error_observer1_, OnConnectionError(net::ERR_FAILED));
  EXPECT_CALL(error_observer2_, OnConnectionError(net::ERR_FAILED));
  EXPECT_CALL(error_observer3_, OnConnectionError(_)).Times(0);

  BlimpMessageProcessor* sender = connection_->GetOutgoingMessageProcessor();
  net::TestCompletionCallback complete_cb_1;
  sender->ProcessMessage(CreateInputMessage(),
                         complete_cb_1.callback());
  base::ResetAndReturn(&write_packet_cb).Run(net::OK);
  EXPECT_EQ(net::OK, complete_cb_1.WaitForResult());

  net::TestCompletionCallback complete_cb_2;
  sender->ProcessMessage(CreateControlMessage(),
                         complete_cb_2.callback());
  base::ResetAndReturn(&write_packet_cb).Run(net::ERR_FAILED);
  EXPECT_EQ(net::ERR_FAILED, complete_cb_2.WaitForResult());
}

TEST_F(BlimpConnectionTest, DeleteHappyObserversAreOK) {
  net::CompletionCallback write_packet_cb;

  InSequence s;
  EXPECT_CALL(*writer_,
              WritePacket(BufferEqualsProto(*CreateInputMessage()), _))
      .WillOnce(SaveArg<1>(&write_packet_cb))
      .RetiresOnSaturation();
  EXPECT_CALL(error_observer1_, OnConnectionError(net::ERR_FAILED))
      .WillOnce(testing::InvokeWithoutArgs(
          this, &BlimpConnectionTest::DropConnection));

  BlimpMessageProcessor* sender = connection_->GetOutgoingMessageProcessor();
  net::TestCompletionCallback complete_cb_1;
  sender->ProcessMessage(CreateInputMessage(), complete_cb_1.callback());
  base::ResetAndReturn(&write_packet_cb).Run(net::ERR_FAILED);
  EXPECT_EQ(net::ERR_FAILED, complete_cb_1.WaitForResult());
}

// Verifies that a ReadPacket error causes ErrorObservers to be notified.
TEST_F(BlimpConnectionTest, ReadPacketErrorInvokesErrorObservers) {
  scoped_refptr<net::GrowableIOBuffer> read_packet_buffer;
  net::CompletionCallback read_packet_cb;

  EXPECT_CALL(*reader_, ReadPacket(_, _))
      .WillOnce(
          DoAll(SaveArg<0>(&read_packet_buffer), SaveArg<1>(&read_packet_cb)))
      .RetiresOnSaturation();

  EXPECT_CALL(error_observer1_, OnConnectionError(net::ERR_FAILED));
  EXPECT_CALL(error_observer2_, OnConnectionError(net::ERR_FAILED));
  EXPECT_CALL(error_observer3_, OnConnectionError(_)).Times(0);

  EXPECT_CALL(receiver_, MockableProcessMessage(_, _)).Times(0);

  // Trigger the first ReadPacket() call by setting the MessageProcessor.
  connection_->SetIncomingMessageProcessor(&receiver_);
  EXPECT_TRUE(read_packet_buffer);
  EXPECT_FALSE(read_packet_cb.is_null());

  // Signal an error back from the ReadPacket operation.
  base::ResetAndReturn(&read_packet_cb).Run(net::ERR_FAILED);
}

// Verifies that EndConnection messages received from the peer are
// routed through to registered ConnectionErrorObservers as errors.
TEST_F(BlimpConnectionTest, EndConnectionInvokesErrorObservers) {
  scoped_refptr<net::GrowableIOBuffer> read_packet_buffer;
  net::CompletionCallback read_packet_cb;

  EXPECT_CALL(*reader_, ReadPacket(_, _))
      .WillOnce(
          DoAll(SaveArg<0>(&read_packet_buffer), SaveArg<1>(&read_packet_cb)))
      .WillOnce(Return())
      .RetiresOnSaturation();

  EXPECT_CALL(error_observer1_,
              OnConnectionError(EndConnectionMessage::PROTOCOL_MISMATCH));
  EXPECT_CALL(error_observer2_,
              OnConnectionError(EndConnectionMessage::PROTOCOL_MISMATCH));
  EXPECT_CALL(error_observer3_, OnConnectionError(_)).Times(0);

  EXPECT_CALL(receiver_, MockableProcessMessage(_, _)).Times(0);

  // Trigger the first ReadPacket() call by setting the MessageProcessor.
  connection_->SetIncomingMessageProcessor(&receiver_);
  EXPECT_TRUE(read_packet_buffer);
  EXPECT_FALSE(read_packet_cb.is_null());

  // Create an EndConnection message to return from ReadPacket.
  std::unique_ptr<BlimpMessage> message =
      CreateEndConnectionMessage(EndConnectionMessage::PROTOCOL_MISMATCH);

  // Put the EndConnection message in the buffer and invoke the read callback.
  read_packet_buffer->SetCapacity(message->ByteSize());
  ASSERT_TRUE(message->SerializeToArray(read_packet_buffer->data(),
                                        message->GetCachedSize()));
  base::ResetAndReturn(&read_packet_cb).Run(message->ByteSize());
}

}  // namespace
}  // namespace blimp
