// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_ctrl_response_buffer.h"

#include <string.h>

#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class FtpCtrlResponseBufferTest : public testing::Test {
 public:
  FtpCtrlResponseBufferTest() : buffer_(BoundNetLog()) {}

 protected:
  int PushDataToBuffer(const char* data) {
    return buffer_.ConsumeData(data, strlen(data));
  }

  FtpCtrlResponseBuffer buffer_;
};

TEST_F(FtpCtrlResponseBufferTest, Basic) {
  EXPECT_FALSE(buffer_.ResponseAvailable());

  EXPECT_EQ(OK, PushDataToBuffer("200 Status Text\r\n"));
  EXPECT_TRUE(buffer_.ResponseAvailable());

  FtpCtrlResponse response = buffer_.PopResponse();
  EXPECT_FALSE(buffer_.ResponseAvailable());
  EXPECT_EQ(200, response.status_code);
  ASSERT_EQ(1U, response.lines.size());
  EXPECT_EQ("Status Text", response.lines[0]);
}

TEST_F(FtpCtrlResponseBufferTest, Chunks) {
  EXPECT_EQ(OK, PushDataToBuffer("20"));
  EXPECT_FALSE(buffer_.ResponseAvailable());
  EXPECT_EQ(OK, PushDataToBuffer("0 Status"));
  EXPECT_FALSE(buffer_.ResponseAvailable());
  EXPECT_EQ(OK, PushDataToBuffer(" Text"));
  EXPECT_FALSE(buffer_.ResponseAvailable());
  EXPECT_EQ(OK, PushDataToBuffer("\r"));
  EXPECT_FALSE(buffer_.ResponseAvailable());
  EXPECT_EQ(OK, PushDataToBuffer("\n"));
  EXPECT_TRUE(buffer_.ResponseAvailable());

  FtpCtrlResponse response = buffer_.PopResponse();
  EXPECT_FALSE(buffer_.ResponseAvailable());
  EXPECT_EQ(200, response.status_code);
  ASSERT_EQ(1U, response.lines.size());
  EXPECT_EQ("Status Text", response.lines[0]);
}

TEST_F(FtpCtrlResponseBufferTest, Continuation) {
  EXPECT_EQ(OK, PushDataToBuffer("230-FirstLine\r\n"));
  EXPECT_FALSE(buffer_.ResponseAvailable());

  EXPECT_EQ(OK, PushDataToBuffer("230-SecondLine\r\n"));
  EXPECT_FALSE(buffer_.ResponseAvailable());

  EXPECT_EQ(OK, PushDataToBuffer("230 LastLine\r\n"));
  EXPECT_TRUE(buffer_.ResponseAvailable());

  FtpCtrlResponse response = buffer_.PopResponse();
  EXPECT_FALSE(buffer_.ResponseAvailable());
  EXPECT_EQ(230, response.status_code);
  ASSERT_EQ(3U, response.lines.size());
  EXPECT_EQ("FirstLine", response.lines[0]);
  EXPECT_EQ("SecondLine", response.lines[1]);
  EXPECT_EQ("LastLine", response.lines[2]);
}

TEST_F(FtpCtrlResponseBufferTest, MultilineContinuation) {
  EXPECT_EQ(OK, PushDataToBuffer("230-FirstLine\r\n"));
  EXPECT_FALSE(buffer_.ResponseAvailable());

  EXPECT_EQ(OK, PushDataToBuffer("Continued\r\n"));
  EXPECT_FALSE(buffer_.ResponseAvailable());

  EXPECT_EQ(OK, PushDataToBuffer("230-SecondLine\r\n"));
  EXPECT_FALSE(buffer_.ResponseAvailable());

  EXPECT_EQ(OK, PushDataToBuffer("215 Continued\r\n"));
  EXPECT_FALSE(buffer_.ResponseAvailable());

  EXPECT_EQ(OK, PushDataToBuffer("230 LastLine\r\n"));
  EXPECT_TRUE(buffer_.ResponseAvailable());

  FtpCtrlResponse response = buffer_.PopResponse();
  EXPECT_FALSE(buffer_.ResponseAvailable());
  EXPECT_EQ(230, response.status_code);
  ASSERT_EQ(3U, response.lines.size());
  EXPECT_EQ("FirstLineContinued", response.lines[0]);
  EXPECT_EQ("SecondLine215 Continued", response.lines[1]);
  EXPECT_EQ("LastLine", response.lines[2]);
}

TEST_F(FtpCtrlResponseBufferTest, MultilineContinuationZeroLength) {
  // For the corner case from bug 29322.
  EXPECT_EQ(OK, PushDataToBuffer("230-\r\n"));
  EXPECT_FALSE(buffer_.ResponseAvailable());

  EXPECT_EQ(OK, PushDataToBuffer("example.com\r\n"));
  EXPECT_FALSE(buffer_.ResponseAvailable());

  EXPECT_EQ(OK, PushDataToBuffer("230 LastLine\r\n"));
  EXPECT_TRUE(buffer_.ResponseAvailable());

  FtpCtrlResponse response = buffer_.PopResponse();
  EXPECT_FALSE(buffer_.ResponseAvailable());
  EXPECT_EQ(230, response.status_code);
  ASSERT_EQ(2U, response.lines.size());
  EXPECT_EQ("example.com", response.lines[0]);
  EXPECT_EQ("LastLine", response.lines[1]);
}

TEST_F(FtpCtrlResponseBufferTest, SimilarContinuation) {
  EXPECT_EQ(OK, PushDataToBuffer("230-FirstLine\r\n"));
  EXPECT_FALSE(buffer_.ResponseAvailable());

  // Notice the space at the start of the line. It should be recognized
  // as a continuation, and not the last line.
  EXPECT_EQ(OK, PushDataToBuffer(" 230 Continued\r\n"));
  EXPECT_FALSE(buffer_.ResponseAvailable());

  EXPECT_EQ(OK, PushDataToBuffer("230 TrueLastLine\r\n"));
  EXPECT_TRUE(buffer_.ResponseAvailable());

  FtpCtrlResponse response = buffer_.PopResponse();
  EXPECT_FALSE(buffer_.ResponseAvailable());
  EXPECT_EQ(230, response.status_code);
  ASSERT_EQ(2U, response.lines.size());
  EXPECT_EQ("FirstLine 230 Continued", response.lines[0]);
  EXPECT_EQ("TrueLastLine", response.lines[1]);
}

// The nesting of multi-line responses is not allowed.
TEST_F(FtpCtrlResponseBufferTest, NoNesting) {
  EXPECT_EQ(OK, PushDataToBuffer("230-FirstLine\r\n"));
  EXPECT_FALSE(buffer_.ResponseAvailable());

  EXPECT_EQ(OK, PushDataToBuffer("300-Continuation\r\n"));
  EXPECT_FALSE(buffer_.ResponseAvailable());

  EXPECT_EQ(OK, PushDataToBuffer("300 Still continuation\r\n"));
  EXPECT_FALSE(buffer_.ResponseAvailable());

  EXPECT_EQ(OK, PushDataToBuffer("230 Real End\r\n"));
  ASSERT_TRUE(buffer_.ResponseAvailable());

  FtpCtrlResponse response = buffer_.PopResponse();
  EXPECT_FALSE(buffer_.ResponseAvailable());
  EXPECT_EQ(230, response.status_code);
  ASSERT_EQ(2U, response.lines.size());
  EXPECT_EQ("FirstLine300-Continuation300 Still continuation",
            response.lines[0]);
  EXPECT_EQ("Real End", response.lines[1]);
}

TEST_F(FtpCtrlResponseBufferTest, NonNumericResponse) {
  EXPECT_EQ(ERR_INVALID_RESPONSE, PushDataToBuffer("Non-numeric\r\n"));
  EXPECT_FALSE(buffer_.ResponseAvailable());
}

TEST_F(FtpCtrlResponseBufferTest, OutOfRangeResponse) {
  EXPECT_EQ(ERR_INVALID_RESPONSE, PushDataToBuffer("777 OK?\r\n"));
  EXPECT_FALSE(buffer_.ResponseAvailable());
}

}  // namespace

}  // namespace net
