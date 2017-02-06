// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/webcam_private/visca_webcam.h"

#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class TestSerialConnection : public SerialConnection {
 public:
  TestSerialConnection() : SerialConnection("dummy_path", "dummy_id") {}
  ~TestSerialConnection() override {}

  void SetReceiveBuffer(const std::vector<char>& receive_buffer) {
    receive_buffer_ = receive_buffer;
  }

  void CheckSendBufferAndClear(const std::vector<char>& expectations) {
    EXPECT_EQ(send_buffer_, expectations);
    send_buffer_.clear();
  }

 private:
  // SerialConnection:
  void Open(const api::serial::ConnectionOptions& options,
            const OpenCompleteCallback& callback) override {
    NOTREACHED();
  }

  bool Receive(const ReceiveCompleteCallback& callback) override {
    callback.Run(receive_buffer_, api::serial::RECEIVE_ERROR_NONE);
    receive_buffer_.clear();
    return true;
  }

  bool Send(const std::vector<char>& data,
            const SendCompleteCallback& callback) override {
    send_buffer_.insert(send_buffer_.end(), data.begin(), data.end());
    callback.Run(data.size(), api::serial::SEND_ERROR_NONE);
    return true;
  }

  std::vector<char> receive_buffer_;
  std::vector<char> send_buffer_;

  DISALLOW_COPY_AND_ASSIGN(TestSerialConnection);
};

class GetPTZExpectations {
 public:
  GetPTZExpectations(bool expected_success, int expected_value)
      : expected_success_(expected_success), expected_value_(expected_value) {}

  void OnCallback(bool success, int value) {
    EXPECT_EQ(expected_success_, success);
    EXPECT_EQ(expected_value_, value);
  }

 private:
  const bool expected_success_;
  const int expected_value_;

  DISALLOW_COPY_AND_ASSIGN(GetPTZExpectations);
};

class SetPTZExpectations {
 public:
  explicit SetPTZExpectations(bool expected_success)
      : expected_success_(expected_success) {}

  void OnCallback(bool success) { EXPECT_EQ(expected_success_, success); }

 private:
  const bool expected_success_;

  DISALLOW_COPY_AND_ASSIGN(SetPTZExpectations);
};

#define CHAR_VECTOR_FROM_ARRAY(array) \
  std::vector<char>(array, array + arraysize(array))

}  // namespace

class ViscaWebcamTest : public testing::Test {
 protected:
  ViscaWebcamTest() {
    webcam_ = new ViscaWebcam;
    webcam_->OpenForTesting(base::WrapUnique(new TestSerialConnection));
  }
  ~ViscaWebcamTest() override {}

  Webcam* webcam() { return webcam_.get(); }

  TestSerialConnection* serial_connection() {
    return static_cast<TestSerialConnection*>(
        webcam_->GetSerialConnectionForTesting());
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<ViscaWebcam> webcam_;
};

TEST_F(ViscaWebcamTest, Zoom) {
  // Check getting the zoom.
  const char kGetZoomCommand[] = {0x81, 0x09, 0x04, 0x47, 0xFF};
  const char kGetZoomResponse[] = {0x00, 0x50, 0x01, 0x02, 0x03, 0x04, 0xFF};
  serial_connection()->SetReceiveBuffer(
      CHAR_VECTOR_FROM_ARRAY(kGetZoomResponse));
  Webcam::GetPTZCompleteCallback receive_callback =
      base::Bind(&GetPTZExpectations::OnCallback,
                 base::Owned(new GetPTZExpectations(true, 0x1234)));
  webcam()->GetZoom(receive_callback);
  serial_connection()->CheckSendBufferAndClear(
      CHAR_VECTOR_FROM_ARRAY(kGetZoomCommand));

  // Check setting the zoom.
  const char kSetZoomCommand[] = {0x81, 0x01, 0x04, 0x47, 0x06,
                                  0x02, 0x05, 0x03, 0xFF};
  // Note: this is a valid, but empty value because nothing is checking it.
  const char kSetZoomResponse[] = {0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0xFF};
  serial_connection()->SetReceiveBuffer(
      CHAR_VECTOR_FROM_ARRAY(kSetZoomResponse));
  Webcam::SetPTZCompleteCallback send_callback =
      base::Bind(&SetPTZExpectations::OnCallback,
                 base::Owned(new SetPTZExpectations(true)));
  serial_connection()->SetReceiveBuffer(
      CHAR_VECTOR_FROM_ARRAY(kSetZoomResponse));
  webcam()->SetZoom(0x6253, send_callback);
  serial_connection()->CheckSendBufferAndClear(
      CHAR_VECTOR_FROM_ARRAY(kSetZoomCommand));
}

}  // namespace extensions
