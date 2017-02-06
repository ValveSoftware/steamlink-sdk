// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "device/serial/buffer.h"
#include "device/serial/data_receiver.h"
#include "device/serial/data_source_sender.h"
#include "device/serial/data_stream.mojom.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class DataSourceTest : public testing::Test {
 public:
  enum Event {
    EVENT_NONE,
    EVENT_WRITE_BUFFER_READY,
    EVENT_RECEIVE_COMPLETE,
    EVENT_ERROR,
  };

  DataSourceTest()
      : error_(0), seen_connection_error_(false), expected_event_(EVENT_NONE) {}

  void SetUp() override {
    message_loop_.reset(new base::MessageLoop);
    mojo::InterfacePtr<serial::DataSource> source_sender_handle;
    mojo::InterfacePtr<serial::DataSourceClient> source_sender_client_handle;
    mojo::InterfaceRequest<serial::DataSourceClient>
        source_sender_client_request =
            mojo::GetProxy(&source_sender_client_handle);
    source_sender_ = new DataSourceSender(
        mojo::GetProxy(&source_sender_handle),
        std::move(source_sender_client_handle),
        base::Bind(&DataSourceTest::CanWriteData, base::Unretained(this)),
        base::Bind(&DataSourceTest::OnError, base::Unretained(this)));
    receiver_ = new DataReceiver(std::move(source_sender_handle),
                                 std::move(source_sender_client_request), 100,
                                 kFatalError);
  }

  void TearDown() override {
    write_buffer_.reset();
    buffer_.reset();
    message_loop_.reset();
  }

  void OnError() {
    seen_connection_error_ = true;
    EventReceived(EVENT_ERROR);
  }

  void WaitForEvent(Event event) {
    expected_event_ = event;
    base::RunLoop run_loop;
    stop_run_loop_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void EventReceived(Event event) {
    if (event == expected_event_ && !stop_run_loop_.is_null())
      stop_run_loop_.Run();
  }

  bool Receive() {
    return receiver_->Receive(
        base::Bind(&DataSourceTest::OnDataReceived, base::Unretained(this)),
        base::Bind(&DataSourceTest::OnReceiveError, base::Unretained(this)));
  }

  void FillWriteBuffer(const base::StringPiece& data, int32_t error) {
    if (!write_buffer_) {
      // Run the message loop until CanWriteData is called.
      WaitForEvent(EVENT_WRITE_BUFFER_READY);
    }
    ASSERT_TRUE(write_buffer_);
    ASSERT_GE(write_buffer_->GetSize(), static_cast<uint32_t>(data.size()));
    memcpy(write_buffer_->GetData(), data.data(), data.size());
    if (error)
      write_buffer_->DoneWithError(static_cast<uint32_t>(data.size()), error);
    else
      write_buffer_->Done(static_cast<uint32_t>(data.size()));
    write_buffer_.reset();
  }

  void ReceiveAndWait() {
    ASSERT_TRUE(Receive());
    // Run the message loop until OnDataReceived or OnReceiveError is called.
    WaitForEvent(EVENT_RECEIVE_COMPLETE);
  }

  void OnDataReceived(std::unique_ptr<ReadOnlyBuffer> buffer) {
    ASSERT_TRUE(buffer);
    error_ = 0;
    buffer_ = std::move(buffer);
    buffer_contents_ = std::string(buffer_->GetData(), buffer_->GetSize());
    EventReceived(EVENT_RECEIVE_COMPLETE);
  }

  void OnReceiveError(int32_t error) {
    buffer_contents_.clear();
    error_ = error;
    EventReceived(EVENT_RECEIVE_COMPLETE);
  }

  void CanWriteData(std::unique_ptr<WritableBuffer> buffer) {
    write_buffer_ = std::move(buffer);
    EventReceived(EVENT_WRITE_BUFFER_READY);
  }

 protected:
  static const int32_t kFatalError;
  std::unique_ptr<base::MessageLoop> message_loop_;
  base::Closure stop_run_loop_;

  scoped_refptr<DataSourceSender> source_sender_;
  scoped_refptr<DataReceiver> receiver_;

  std::unique_ptr<ReadOnlyBuffer> buffer_;
  std::string buffer_contents_;
  int32_t error_;
  std::unique_ptr<WritableBuffer> write_buffer_;

  bool seen_connection_error_;

  Event expected_event_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DataSourceTest);
};

const int32_t DataSourceTest::kFatalError = -10;

// Test that data is successfully transmitted from the source to the receiver.
TEST_F(DataSourceTest, Basic) {
  ASSERT_NO_FATAL_FAILURE(FillWriteBuffer("a", 0));

  ASSERT_NO_FATAL_FAILURE(ReceiveAndWait());
  EXPECT_EQ(0, error_);
  ASSERT_TRUE(buffer_);
  EXPECT_EQ("a", buffer_contents_);
  buffer_->Done(1);

  ASSERT_NO_FATAL_FAILURE(FillWriteBuffer("b", 0));

  ASSERT_NO_FATAL_FAILURE(ReceiveAndWait());
  EXPECT_EQ(0, error_);
  ASSERT_TRUE(buffer_);
  EXPECT_EQ("b", buffer_contents_);
}

// Test that the receiver does not discard any data that is not read by the
// client.
TEST_F(DataSourceTest, PartialReceive) {
  ASSERT_NO_FATAL_FAILURE(FillWriteBuffer("ab", 0));

  ASSERT_NO_FATAL_FAILURE(ReceiveAndWait());
  EXPECT_EQ(0, error_);
  ASSERT_TRUE(buffer_);
  EXPECT_EQ("ab", buffer_contents_);
  buffer_->Done(1);

  ASSERT_NO_FATAL_FAILURE(ReceiveAndWait());
  EXPECT_EQ(0, error_);
  ASSERT_TRUE(buffer_);
  EXPECT_EQ("b", buffer_contents_);
}

// Test that an error is correctly reported to the Receive() call immediately
// after the data has been read by the client.
TEST_F(DataSourceTest, ErrorAndData) {
  ASSERT_NO_FATAL_FAILURE(FillWriteBuffer("abc", -1));

  ASSERT_NO_FATAL_FAILURE(ReceiveAndWait());
  ASSERT_TRUE(buffer_);
  EXPECT_EQ("abc", buffer_contents_);
  buffer_->Done(1);
  EXPECT_EQ(0, error_);
  ASSERT_NO_FATAL_FAILURE(ReceiveAndWait());
  ASSERT_TRUE(buffer_);
  EXPECT_EQ("bc", buffer_contents_);
  buffer_->Done(1);
  EXPECT_EQ(0, error_);
  ASSERT_NO_FATAL_FAILURE(ReceiveAndWait());
  ASSERT_TRUE(buffer_);
  EXPECT_EQ("c", buffer_contents_);
  buffer_->Done(1);
  EXPECT_EQ(0, error_);
  ASSERT_NO_FATAL_FAILURE(ReceiveAndWait());
  EXPECT_EQ(-1, error_);
  ASSERT_FALSE(write_buffer_);

  ASSERT_TRUE(Receive());
  ASSERT_NO_FATAL_FAILURE(FillWriteBuffer("d", 0));

  WaitForEvent(EVENT_RECEIVE_COMPLETE);
  ASSERT_TRUE(buffer_);
  EXPECT_EQ("d", buffer_contents_);
  buffer_->Done(1);
  EXPECT_EQ(0, error_);

  ASSERT_NO_FATAL_FAILURE(FillWriteBuffer("e", -2));
  ASSERT_NO_FATAL_FAILURE(ReceiveAndWait());
  ASSERT_TRUE(buffer_);
  EXPECT_EQ("e", buffer_contents_);
  buffer_->Done(1);
  EXPECT_EQ(0, error_);

  ASSERT_NO_FATAL_FAILURE(ReceiveAndWait());
  EXPECT_EQ(-2, error_);
}

// Test that an error is correctly reported when the source encounters an error
// without sending any data.
TEST_F(DataSourceTest, ErrorOnly) {
  ASSERT_NO_FATAL_FAILURE(FillWriteBuffer("", -1));

  ASSERT_NO_FATAL_FAILURE(ReceiveAndWait());
  EXPECT_FALSE(buffer_);
  EXPECT_EQ(-1, error_);
  ASSERT_FALSE(write_buffer_);

  ASSERT_TRUE(Receive());
  ASSERT_NO_FATAL_FAILURE(FillWriteBuffer("", -2));

  WaitForEvent(EVENT_RECEIVE_COMPLETE);
  EXPECT_FALSE(buffer_);
  EXPECT_EQ(-2, error_);
  ASSERT_FALSE(write_buffer_);
}

// Test that the source shutting down is correctly reported to the client.
TEST_F(DataSourceTest, SourceShutdown) {
  source_sender_->ShutDown();
  source_sender_ = NULL;
  ASSERT_NO_FATAL_FAILURE(ReceiveAndWait());
  EXPECT_FALSE(buffer_);
  EXPECT_EQ(kFatalError, error_);
  ASSERT_FALSE(write_buffer_);
  ASSERT_FALSE(Receive());
}

// Test that the receiver shutting down is correctly reported to the source.
TEST_F(DataSourceTest, ReceiverShutdown) {
  Receive();
  receiver_ = NULL;
  EXPECT_EQ(kFatalError, error_);
  WaitForEvent(EVENT_ERROR);
  EXPECT_TRUE(seen_connection_error_);
}

}  // namespace device
