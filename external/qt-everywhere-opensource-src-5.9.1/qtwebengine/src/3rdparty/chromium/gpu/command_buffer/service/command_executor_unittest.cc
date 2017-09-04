// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/command_executor.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/run_loop.h"
#include "gpu/command_buffer/common/command_buffer_mock.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_mock.h"
#include "gpu/command_buffer/service/mocks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;

namespace gpu {

const size_t kRingBufferSize = 1024;

class CommandExecutorTest : public testing::Test {
 protected:
  static const int32_t kTransferBufferId = 123;

  void SetUp() override {
    std::unique_ptr<base::SharedMemory> shared_memory(new ::base::SharedMemory);
    shared_memory->CreateAndMapAnonymous(kRingBufferSize);
    buffer_ = static_cast<int32_t*>(shared_memory->memory());
    shared_memory_buffer_ =
        MakeBufferFromSharedMemory(std::move(shared_memory), kRingBufferSize);
    memset(buffer_, 0, kRingBufferSize);

    command_buffer_.reset(new MockCommandBuffer);

    CommandBuffer::State default_state;
    ON_CALL(*command_buffer_.get(), GetLastState())
        .WillByDefault(Return(default_state));
    ON_CALL(*command_buffer_.get(), GetPutOffset()).WillByDefault(Return(0));

    decoder_.reset(new gles2::MockGLES2Decoder());
    // Install FakeDoCommands handler so we can use individual DoCommand()
    // expectations.
    EXPECT_CALL(*decoder_, DoCommands(_, _, _, _))
        .WillRepeatedly(
            Invoke(decoder_.get(), &gles2::MockGLES2Decoder::FakeDoCommands));

    executor_.reset(new gpu::CommandExecutor(command_buffer_.get(),
                                             decoder_.get(), decoder_.get()));
    EXPECT_CALL(*command_buffer_, GetTransferBuffer(kTransferBufferId))
        .WillOnce(Return(shared_memory_buffer_));
    EXPECT_CALL(*command_buffer_, SetGetOffset(0));
    EXPECT_TRUE(executor_->SetGetBuffer(kTransferBufferId));
  }

  void TearDown() override {
    // Ensure that any unexpected tasks posted by the GPU scheduler are executed
    // in order to fail the test.
    base::RunLoop().RunUntilIdle();
  }

  error::Error GetError() { return command_buffer_->GetLastState().error; }

  std::unique_ptr<MockCommandBuffer> command_buffer_;
  scoped_refptr<Buffer> shared_memory_buffer_;
  int32_t* buffer_;
  std::unique_ptr<gles2::MockGLES2Decoder> decoder_;
  std::unique_ptr<CommandExecutor> executor_;
  base::MessageLoop message_loop_;
};

TEST_F(CommandExecutorTest, ExecutorDoesNothingIfRingBufferIsEmpty) {
  CommandBuffer::State state;

  EXPECT_CALL(*command_buffer_, GetLastState()).WillRepeatedly(Return(state));

  EXPECT_CALL(*command_buffer_, SetParseError(_)).Times(0);

  executor_->PutChanged();
}

TEST_F(CommandExecutorTest, GetSetBuffer) {
  CommandBuffer::State state;

  // Set the get offset to something not 0.
  EXPECT_CALL(*command_buffer_, SetGetOffset(2));
  executor_->SetGetOffset(2);
  EXPECT_EQ(2, executor_->GetGetOffset());

  // Set the buffer.
  EXPECT_CALL(*command_buffer_, GetTransferBuffer(kTransferBufferId))
      .WillOnce(Return(shared_memory_buffer_));
  EXPECT_CALL(*command_buffer_, SetGetOffset(0));
  EXPECT_TRUE(executor_->SetGetBuffer(kTransferBufferId));

  // Check the get offset was reset.
  EXPECT_EQ(0, executor_->GetGetOffset());
}

TEST_F(CommandExecutorTest, ProcessesOneCommand) {
  CommandHeader* header = reinterpret_cast<CommandHeader*>(&buffer_[0]);
  header[0].command = 7;
  header[0].size = 2;
  buffer_[1] = 123;

  CommandBuffer::State state;

  EXPECT_CALL(*command_buffer_, GetLastState()).WillRepeatedly(Return(state));
  EXPECT_CALL(*command_buffer_, GetPutOffset()).WillRepeatedly(Return(2));
  EXPECT_CALL(*command_buffer_, SetGetOffset(2));

  EXPECT_CALL(*decoder_, DoCommand(7, 1, &buffer_[0]))
      .WillOnce(Return(error::kNoError));

  EXPECT_CALL(*command_buffer_, SetParseError(_)).Times(0);

  executor_->PutChanged();
}

TEST_F(CommandExecutorTest, ProcessesTwoCommands) {
  CommandHeader* header = reinterpret_cast<CommandHeader*>(&buffer_[0]);
  header[0].command = 7;
  header[0].size = 2;
  buffer_[1] = 123;
  header[2].command = 8;
  header[2].size = 1;

  CommandBuffer::State state;

  EXPECT_CALL(*command_buffer_, GetLastState()).WillRepeatedly(Return(state));
  EXPECT_CALL(*command_buffer_, GetPutOffset()).WillRepeatedly(Return(3));

  EXPECT_CALL(*decoder_, DoCommand(7, 1, &buffer_[0]))
      .WillOnce(Return(error::kNoError));

  EXPECT_CALL(*decoder_, DoCommand(8, 0, &buffer_[2]))
      .WillOnce(Return(error::kNoError));
  EXPECT_CALL(*command_buffer_, SetGetOffset(3));

  executor_->PutChanged();
}

TEST_F(CommandExecutorTest, SetsErrorCodeOnCommandBuffer) {
  CommandHeader* header = reinterpret_cast<CommandHeader*>(&buffer_[0]);
  header[0].command = 7;
  header[0].size = 1;

  CommandBuffer::State state;

  EXPECT_CALL(*command_buffer_, GetLastState()).WillRepeatedly(Return(state));
  EXPECT_CALL(*command_buffer_, GetPutOffset()).WillRepeatedly(Return(1));

  EXPECT_CALL(*decoder_, DoCommand(7, 0, &buffer_[0]))
      .WillOnce(Return(error::kUnknownCommand));
  EXPECT_CALL(*command_buffer_, SetGetOffset(1));

  EXPECT_CALL(*command_buffer_, SetContextLostReason(_));
  EXPECT_CALL(*decoder_, GetContextLostReason())
      .WillOnce(Return(error::kUnknown));
  EXPECT_CALL(*command_buffer_, SetParseError(error::kUnknownCommand));

  executor_->PutChanged();
}

TEST_F(CommandExecutorTest, ProcessCommandsDoesNothingAfterError) {
  CommandBuffer::State state;
  state.error = error::kGenericError;

  EXPECT_CALL(*command_buffer_, GetLastState()).WillRepeatedly(Return(state));

  executor_->PutChanged();
}

TEST_F(CommandExecutorTest, CanGetAddressOfSharedMemory) {
  EXPECT_CALL(*command_buffer_.get(), GetTransferBuffer(7))
      .WillOnce(Return(shared_memory_buffer_));

  EXPECT_EQ(&buffer_[0], executor_->GetSharedMemoryBuffer(7)->memory());
}

ACTION_P2(SetPointee, address, value) {
  *address = value;
}

TEST_F(CommandExecutorTest, CanGetSizeOfSharedMemory) {
  EXPECT_CALL(*command_buffer_.get(), GetTransferBuffer(7))
      .WillOnce(Return(shared_memory_buffer_));

  EXPECT_EQ(kRingBufferSize, executor_->GetSharedMemoryBuffer(7)->size());
}

TEST_F(CommandExecutorTest, SetTokenForwardsToCommandBuffer) {
  EXPECT_CALL(*command_buffer_, SetToken(7));
  executor_->set_token(7);
}

}  // namespace gpu
