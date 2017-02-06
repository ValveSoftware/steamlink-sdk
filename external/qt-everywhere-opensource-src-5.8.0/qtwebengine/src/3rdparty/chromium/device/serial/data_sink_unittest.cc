// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "device/serial/data_sender.h"
#include "device/serial/data_sink_receiver.h"
#include "device/serial/data_stream.mojom.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class DataSinkTest : public testing::Test {
 public:
  enum Event {
    EVENT_NONE,
    EVENT_READ_BUFFER_READY,
    EVENT_CANCEL_RECEIVED,
    EVENT_SEND_SUCCESS,
    EVENT_SEND_ERROR,
    EVENT_CANCEL_COMPLETE,
    EVENT_ERROR,
  };

  DataSinkTest()
      : bytes_sent_(0),
        send_error_(0),
        has_send_error_(false),
        cancel_error_(0),
        seen_connection_error_(false),
        expected_event_(EVENT_NONE) {}

  void SetUp() override {
    message_loop_.reset(new base::MessageLoop);
    mojo::InterfacePtr<serial::DataSink> sink_handle;
    sink_receiver_ = new DataSinkReceiver(
        mojo::GetProxy(&sink_handle),
        base::Bind(&DataSinkTest::OnDataToRead, base::Unretained(this)),
        base::Bind(&DataSinkTest::OnCancel, base::Unretained(this)),
        base::Bind(&DataSinkTest::OnError, base::Unretained(this)));
    sender_.reset(
        new DataSender(std::move(sink_handle), kBufferSize, kFatalError));
  }

  void TearDown() override {
    read_buffer_.reset();
    message_loop_.reset();
    if (sink_receiver_.get())
      EXPECT_TRUE(sink_receiver_->HasOneRef());
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

  void OnError() {
    seen_connection_error_ = true;
    EventReceived(EVENT_ERROR);
  }

  void ExpectDataAndReadAll(const base::StringPiece& expected_data) {
    ExpectData(expected_data, static_cast<uint32_t>(expected_data.size()), 0);
  }

  void ExpectData(const base::StringPiece& expected_data,
                  uint32_t bytes_to_read,
                  int32_t error) {
    DCHECK(bytes_to_read <= static_cast<uint32_t>(expected_data.size()));
    std::string data;
    while (data.size() < static_cast<size_t>(expected_data.size())) {
      if (!read_buffer_)
        WaitForEvent(EVENT_READ_BUFFER_READY);
      ASSERT_TRUE(read_buffer_);
      data.append(read_buffer_->GetData(), read_buffer_->GetSize());
      if (bytes_to_read <= read_buffer_->GetSize()) {
        if (error)
          read_buffer_->DoneWithError(bytes_to_read, error);
        else
          read_buffer_->Done(bytes_to_read);
        read_buffer_.reset();
        break;
      } else {
        bytes_to_read -= read_buffer_->GetSize();
        read_buffer_->Done(read_buffer_->GetSize());
        read_buffer_.reset();
      }
    }
    // If we terminate early, we may not see all of the data. In that case,
    // check that the part we saw matches what we expected.
    if (static_cast<uint32_t>(data.size()) <
            static_cast<uint32_t>(expected_data.size()) &&
        data.size() >= bytes_to_read) {
      EXPECT_EQ(expected_data.substr(0, data.size()), data);
      return;
    }
    EXPECT_EQ(expected_data, data);
  }

  void ExpectSendSuccess(uint32_t expected_bytes_sent) {
    bytes_sent_ = 0;
    WaitForEvent(EVENT_SEND_SUCCESS);
    EXPECT_EQ(expected_bytes_sent, bytes_sent_);
    EXPECT_FALSE(has_send_error_);
  }

  void ExpectSendError(uint32_t expected_bytes_sent, int32_t expected_error) {
    bytes_sent_ = 0;
    has_send_error_ = 0;
    send_error_ = 0;
    WaitForEvent(EVENT_SEND_ERROR);
    EXPECT_EQ(expected_bytes_sent, bytes_sent_);
    EXPECT_TRUE(has_send_error_);
    EXPECT_EQ(expected_error, send_error_);
  }

  void ExpectCancel(int32_t expected_error) {
    cancel_error_ = 0;
    WaitForEvent(EVENT_CANCEL_RECEIVED);
    EXPECT_EQ(expected_error, cancel_error_);
  }

  void ExpectCancelResult() { WaitForEvent(EVENT_CANCEL_COMPLETE); }

  bool Send(const base::StringPiece& data) {
    return sender_->Send(
        data,
        base::Bind(&DataSinkTest::OnDataSent, base::Unretained(this)),
        base::Bind(&DataSinkTest::OnSendError, base::Unretained(this)));
  }

  void OnDataSent(uint32_t bytes_sent) {
    bytes_sent_ = bytes_sent;
    has_send_error_ = false;
    EventReceived(EVENT_SEND_SUCCESS);
  }

  void OnSendError(uint32_t bytes_sent, int32_t error) {
    bytes_sent_ = bytes_sent;
    send_error_ = error;
    has_send_error_ = true;
    EventReceived(EVENT_SEND_ERROR);
  }

  void OnDataToRead(std::unique_ptr<ReadOnlyBuffer> buffer) {
    read_buffer_ = std::move(buffer);
    read_buffer_contents_ =
        std::string(read_buffer_->GetData(), read_buffer_->GetSize());
    EventReceived(EVENT_READ_BUFFER_READY);
  }

  bool Cancel(int32_t error) {
    return sender_->Cancel(
        error, base::Bind(&DataSinkTest::CancelAck, base::Unretained(this)));
  }

  void CancelAck() { EventReceived(EVENT_CANCEL_COMPLETE); }

  void OnCancel(int32_t error) {
    cancel_error_ = error;
    EventReceived(EVENT_CANCEL_RECEIVED);
  }

 protected:
  static const int32_t kFatalError;
  static const uint32_t kBufferSize;
  std::unique_ptr<base::MessageLoop> message_loop_;
  base::Closure stop_run_loop_;

  scoped_refptr<DataSinkReceiver> sink_receiver_;
  std::unique_ptr<DataSender> sender_;

  uint32_t bytes_sent_;
  int32_t send_error_;
  bool has_send_error_;
  int32_t cancel_error_;
  std::unique_ptr<ReadOnlyBuffer> read_buffer_;
  std::string read_buffer_contents_;

  bool seen_connection_error_;

  Event expected_event_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DataSinkTest);
};

const int32_t DataSinkTest::kFatalError = -10;
const uint32_t DataSinkTest::kBufferSize = 100;

TEST_F(DataSinkTest, Basic) {
  ASSERT_TRUE(Send("a"));
  ASSERT_NO_FATAL_FAILURE(ExpectDataAndReadAll("a"));
  ExpectSendSuccess(1);
}

TEST_F(DataSinkTest, LargeSend) {
  std::string pattern = "1234567890";
  std::string data;
  for (uint32_t i = 0; i < kBufferSize; i++)
    data.append(pattern);
  ASSERT_TRUE(Send(data));
  ASSERT_NO_FATAL_FAILURE(ExpectDataAndReadAll(data));
  ExpectSendSuccess(static_cast<uint32_t>(data.size()));
}

TEST_F(DataSinkTest, ErrorAndAllData) {
  ASSERT_TRUE(Send("a"));
  ASSERT_NO_FATAL_FAILURE(ExpectData("a", 1, -1));
  ExpectSendError(1, -1);
}

TEST_F(DataSinkTest, ErrorAndPartialData) {
  ASSERT_TRUE(Send("ab"));
  ASSERT_NO_FATAL_FAILURE(ExpectData("ab", 1, -1));
  ExpectSendError(1, -1);

  ASSERT_TRUE(Send("c"));
  ASSERT_NO_FATAL_FAILURE(ExpectDataAndReadAll("c"));
  ExpectSendSuccess(1);
}

TEST_F(DataSinkTest, ErrorAndPartialDataAtPacketBoundary) {
  ASSERT_TRUE(Send("ab"));
  ASSERT_NO_FATAL_FAILURE(ExpectData("ab", 2, -1));
  ExpectSendError(2, -1);

  ASSERT_TRUE(Send("c"));
  ASSERT_NO_FATAL_FAILURE(ExpectDataAndReadAll("c"));
  ExpectSendSuccess(1);
}

TEST_F(DataSinkTest, ErrorInSecondPacket) {
  ASSERT_TRUE(Send("a"));
  ASSERT_TRUE(Send("b"));
  ASSERT_NO_FATAL_FAILURE(ExpectData("ab", 2, -1));
  ExpectSendSuccess(1);
  ExpectSendError(1, -1);

  ASSERT_TRUE(Send("c"));
  ASSERT_NO_FATAL_FAILURE(ExpectDataAndReadAll("c"));
  ExpectSendSuccess(1);
}

TEST_F(DataSinkTest, ErrorBeforeLargeSend) {
  ASSERT_TRUE(Send("a"));
  std::string pattern = "123456789";
  std::string data;
  for (int i = 0; i < 100; i++)
    data.append("1234567890");
  ASSERT_TRUE(Send(data));
  ASSERT_NO_FATAL_FAILURE(ExpectData("a" + data, 1, -1));
  ExpectSendError(1, -1);
  ExpectSendError(0, -1);
}

TEST_F(DataSinkTest, ErrorOnly) {
  ASSERT_TRUE(Send("a"));
  ASSERT_NO_FATAL_FAILURE(ExpectData("a", 0, -1));
  ExpectSendError(0, -1);

  ASSERT_TRUE(Send("b"));
  ASSERT_NO_FATAL_FAILURE(ExpectDataAndReadAll("b"));
  ExpectSendSuccess(1);
}

TEST_F(DataSinkTest, Cancel) {
  ASSERT_TRUE(Send("a"));
  WaitForEvent(EVENT_READ_BUFFER_READY);
  ASSERT_TRUE(Cancel(-2));
  // Check that sends fail until the cancel operation completes.
  EXPECT_FALSE(Send("b"));
  ASSERT_NO_FATAL_FAILURE(ExpectCancel(-2));
  ASSERT_NO_FATAL_FAILURE(ExpectData("a", 0, -1));
  // Check that the error reported by the sink implementation is reported to the
  // client - not the cancellation error.
  ExpectSendError(0, -1);
  ExpectCancelResult();

  EXPECT_TRUE(Send("c"));
  ASSERT_NO_FATAL_FAILURE(ExpectDataAndReadAll("c"));
  ExpectSendSuccess(1);
}

TEST_F(DataSinkTest, CancelSinkSucceeds) {
  ASSERT_TRUE(Send("a"));
  EXPECT_TRUE(Send("b"));
  WaitForEvent(EVENT_READ_BUFFER_READY);
  ASSERT_TRUE(Cancel(-2));
  ASSERT_NO_FATAL_FAILURE(ExpectCancel(-2));
  ASSERT_NO_FATAL_FAILURE(ExpectData("ab", 1, 0));
  ExpectSendError(1, -2);
  ExpectCancelResult();

  EXPECT_TRUE(Send("c"));
  ASSERT_NO_FATAL_FAILURE(ExpectDataAndReadAll("c"));
  ExpectSendSuccess(1);
}

TEST_F(DataSinkTest, CancelTwice) {
  ASSERT_TRUE(Send("a"));
  WaitForEvent(EVENT_READ_BUFFER_READY);
  ASSERT_TRUE(Cancel(-2));
  ASSERT_NO_FATAL_FAILURE(ExpectCancel(-2));
  ASSERT_NO_FATAL_FAILURE(ExpectData("a", 0, -2));
  ExpectSendError(0, -2);
  ExpectCancelResult();

  EXPECT_TRUE(Send("b"));
  WaitForEvent(EVENT_READ_BUFFER_READY);
  ASSERT_TRUE(Cancel(-3));
  ASSERT_NO_FATAL_FAILURE(ExpectCancel(-3));
  ASSERT_NO_FATAL_FAILURE(ExpectData("b", 0, -3));
  ExpectSendError(0, -3);
  ExpectCancelResult();

  EXPECT_TRUE(Send("c"));
  ASSERT_NO_FATAL_FAILURE(ExpectDataAndReadAll("c"));
  ExpectSendSuccess(1);
}

TEST_F(DataSinkTest, CancelTwiceWithNoSendBetween) {
  ASSERT_TRUE(Send("a"));
  WaitForEvent(EVENT_READ_BUFFER_READY);
  ASSERT_TRUE(Cancel(-2));
  ASSERT_NO_FATAL_FAILURE(ExpectCancel(-2));
  ASSERT_NO_FATAL_FAILURE(ExpectData("a", 0, -2));
  ExpectSendError(0, -2);
  ExpectCancelResult();
  ASSERT_TRUE(Cancel(-3));
  ExpectCancelResult();

  EXPECT_TRUE(Send("b"));
  ASSERT_NO_FATAL_FAILURE(ExpectDataAndReadAll("b"));
  ExpectSendSuccess(1);
}

TEST_F(DataSinkTest, CancelDuringError) {
  ASSERT_TRUE(Send("a"));
  WaitForEvent(EVENT_READ_BUFFER_READY);
  ASSERT_TRUE(Cancel(-2));
  ASSERT_NO_FATAL_FAILURE(ExpectData("a", 0, -1));
  ExpectSendError(0, -1);
  ExpectCancelResult();

  EXPECT_TRUE(Send("a"));
  ASSERT_NO_FATAL_FAILURE(ExpectDataAndReadAll("a"));
  ExpectSendSuccess(1);
}

TEST_F(DataSinkTest, CancelWithoutSend) {
  ASSERT_TRUE(Cancel(-2));
  ExpectCancelResult();

  EXPECT_TRUE(Send("a"));
  ASSERT_NO_FATAL_FAILURE(ExpectDataAndReadAll("a"));
  ExpectSendSuccess(1);
  EXPECT_EQ(0, cancel_error_);
}

TEST_F(DataSinkTest, CancelPartialPacket) {
  ASSERT_TRUE(Send("ab"));
  WaitForEvent(EVENT_READ_BUFFER_READY);
  ASSERT_TRUE(Cancel(-2));
  ASSERT_NO_FATAL_FAILURE(ExpectCancel(-2));
  ASSERT_NO_FATAL_FAILURE(ExpectData("ab", 1, -2));
  ExpectSendError(1, -2);
  ExpectCancelResult();

  EXPECT_TRUE(Send("c"));
  ASSERT_NO_FATAL_FAILURE(ExpectDataAndReadAll("c"));
  ExpectSendSuccess(1);
}

TEST_F(DataSinkTest, SinkReceiverShutdown) {
  ASSERT_TRUE(Send("a"));
  ASSERT_TRUE(Send(std::string(kBufferSize * 10, 'b')));
  ASSERT_TRUE(Cancel(-1));
  sink_receiver_->ShutDown();
  sink_receiver_ = NULL;
  ExpectSendError(0, kFatalError);
  ExpectSendError(0, kFatalError);
  ExpectCancelResult();
  ASSERT_FALSE(Send("a"));
  ASSERT_FALSE(Cancel(-1));
}

TEST_F(DataSinkTest, SenderShutdown) {
  ASSERT_TRUE(Send("a"));
  ASSERT_TRUE(Send(std::string(kBufferSize * 10, 'b')));
  ASSERT_TRUE(Cancel(-1));
  sender_.reset();
  ExpectSendError(0, kFatalError);
  ExpectSendError(0, kFatalError);
  ExpectCancelResult();
  if (!seen_connection_error_)
    WaitForEvent(EVENT_ERROR);
  EXPECT_TRUE(seen_connection_error_);
}

}  // namespace device
