// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <queue>
#include <utility>

#include "base/macros.h"
#include "device/serial/data_sink_receiver.h"
#include "device/serial/data_stream.mojom.h"
#include "extensions/renderer/api_test_base.h"
#include "grit/extensions_renderer_resources.h"

namespace extensions {

// Runs tests defined in extensions/test/data/data_sender_unittest.js
class DataSenderTest : public ApiTestBase {
 public:
  DataSenderTest() {}

  void SetUp() override {
    ApiTestBase::SetUp();
    service_provider()->AddService(
        base::Bind(&DataSenderTest::CreateDataSink, base::Unretained(this)));
  }

  void TearDown() override {
    if (receiver_.get()) {
      receiver_->ShutDown();
      receiver_ = NULL;
    }
    EXPECT_FALSE(buffer_);
    buffer_.reset();
    ApiTestBase::TearDown();
  }

  std::queue<int32_t> error_to_report_;
  std::queue<std::string> expected_data_;

 private:
  void CreateDataSink(
      mojo::InterfaceRequest<device::serial::DataSink> request) {
    receiver_ = new device::DataSinkReceiver(
        std::move(request),
        base::Bind(&DataSenderTest::ReadyToReceive, base::Unretained(this)),
        base::Bind(&DataSenderTest::OnCancel, base::Unretained(this)),
        base::Bind(base::DoNothing));
  }

  void ReadyToReceive(std::unique_ptr<device::ReadOnlyBuffer> buffer) {
    std::string data(buffer->GetData(), buffer->GetSize());
    if (expected_data_.empty()) {
      buffer_ = std::move(buffer);
      return;
    }

    std::string& expected = expected_data_.front();
    if (expected.size() > buffer->GetSize()) {
      EXPECT_EQ(expected.substr(0, buffer->GetSize()), data);
      expected = expected.substr(buffer->GetSize());
      buffer->Done(buffer->GetSize());
      return;
    }
    if (expected.size() < buffer->GetSize())
      data = data.substr(0, expected.size());
    EXPECT_EQ(expected, data);
    expected_data_.pop();
    int32_t error = 0;
    if (!error_to_report_.empty()) {
      error = error_to_report_.front();
      error_to_report_.pop();
    }
    if (error)
      buffer->DoneWithError(data.size(), error);
    else
      buffer->Done(data.size());
  }

  void OnCancel(int32_t error) {
    ASSERT_TRUE(buffer_);
    buffer_->DoneWithError(0, error);
    buffer_.reset();
  }

  scoped_refptr<device::DataSinkReceiver> receiver_;
  std::unique_ptr<device::ReadOnlyBuffer> buffer_;

  DISALLOW_COPY_AND_ASSIGN(DataSenderTest);
};

TEST_F(DataSenderTest, Send) {
  expected_data_.push("aa");
  RunTest("data_sender_unittest.js", "testSend");
}

// https://crbug.com/599898
#if defined(LEAK_SANITIZER)
#define MAYBE_LargeSend DISABLED_LargeSend
#else
#define MAYBE_LargeSend LargeSend
#endif
TEST_F(DataSenderTest, MAYBE_LargeSend) {
  std::string pattern = "123";
  std::string expected_data;
  for (int i = 0; i < 11; i++)
    expected_data += pattern;
  expected_data_.push(expected_data);
  RunTest("data_sender_unittest.js", "testLargeSend");
}

TEST_F(DataSenderTest, SendError) {
  expected_data_.push("");
  expected_data_.push("a");
  error_to_report_.push(1);
  RunTest("data_sender_unittest.js", "testSendError");
}

TEST_F(DataSenderTest, SendErrorPartialSuccess) {
  expected_data_.push(std::string(5, 'b'));
  expected_data_.push("a");
  error_to_report_.push(1);
  RunTest("data_sender_unittest.js", "testSendErrorPartialSuccess");
}

TEST_F(DataSenderTest, SendErrorBetweenPackets) {
  expected_data_.push(std::string(2, 'b'));
  expected_data_.push("a");
  error_to_report_.push(1);
  RunTest("data_sender_unittest.js", "testSendErrorBetweenPackets");
}

TEST_F(DataSenderTest, SendErrorInSecondPacket) {
  expected_data_.push(std::string(3, 'b'));
  expected_data_.push("a");
  error_to_report_.push(1);
  RunTest("data_sender_unittest.js", "testSendErrorInSecondPacket");
}

TEST_F(DataSenderTest, SendErrorInLargeSend) {
  expected_data_.push("123456789012");
  expected_data_.push("a");
  error_to_report_.push(1);
  RunTest("data_sender_unittest.js", "testSendErrorInLargeSend");
}

TEST_F(DataSenderTest, SendErrorBeforeLargeSend) {
  expected_data_.push(std::string(2, 'b'));
  expected_data_.push("a");
  error_to_report_.push(1);
  RunTest("data_sender_unittest.js", "testSendErrorBeforeLargeSend");
}

// https://crbug.com/599898
#if defined(LEAK_SANITIZER)
#define MAYBE_CancelWithoutSend DISABLED_CancelWithoutSend
#else
#define MAYBE_CancelWithoutSend CancelWithoutSend
#endif
TEST_F(DataSenderTest, MAYBE_CancelWithoutSend) {
  RunTest("data_sender_unittest.js", "testCancelWithoutSend");
}

TEST_F(DataSenderTest, Cancel) {
  RunTest("data_sender_unittest.js", "testCancel");
}

// https://crbug.com/599898
#if defined(LEAK_SANITIZER)
#define MAYBE_Close DISABLED_Close
#else
#define MAYBE_Close Close
#endif
TEST_F(DataSenderTest, MAYBE_Close) {
  RunTest("data_sender_unittest.js", "testClose");
}

TEST_F(DataSenderTest, SendAfterSerialization) {
  expected_data_.push("aa");
  RunTest("data_sender_unittest.js", "testSendAfterSerialization");
}

TEST_F(DataSenderTest, SendErrorAfterSerialization) {
  expected_data_.push("");
  expected_data_.push("a");
  error_to_report_.push(1);
  RunTest("data_sender_unittest.js", "testSendErrorAfterSerialization");
}

TEST_F(DataSenderTest, CancelAfterSerialization) {
  RunTest("data_sender_unittest.js", "testCancelAfterSerialization");
}

TEST_F(DataSenderTest, SerializeCancelsSendsInProgress) {
  RunTest("data_sender_unittest.js", "testSerializeCancelsSendsInProgress");
}

TEST_F(DataSenderTest, SerializeWaitsForCancel) {
  RunTest("data_sender_unittest.js", "testSerializeWaitsForCancel");
}

// https://crbug.com/599898
#if defined(LEAK_SANITIZER)
#define MAYBE_SerializeAfterClose DISABLED_SerializeAfterClose
#else
#define MAYBE_SerializeAfterClose SerializeAfterClose
#endif
TEST_F(DataSenderTest, MAYBE_SerializeAfterClose) {
  RunTest("data_sender_unittest.js", "testSerializeAfterClose");
}

}  // namespace extensions
