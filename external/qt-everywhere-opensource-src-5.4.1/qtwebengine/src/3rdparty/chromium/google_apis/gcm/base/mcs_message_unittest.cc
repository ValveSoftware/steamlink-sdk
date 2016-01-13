// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/base/mcs_message.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

const uint64 kAndroidId = 12345;
const uint64 kSecret = 54321;

class MCSMessageTest : public testing::Test {
 public:
  MCSMessageTest();
  virtual ~MCSMessageTest();
 private:
  base::MessageLoop message_loop_;
};

MCSMessageTest::MCSMessageTest() {
}

MCSMessageTest::~MCSMessageTest() {
}

TEST_F(MCSMessageTest, Invalid) {
  MCSMessage message;
  EXPECT_FALSE(message.IsValid());
}

TEST_F(MCSMessageTest, InitInferTag) {
  scoped_ptr<mcs_proto::LoginRequest> login_request(
      BuildLoginRequest(kAndroidId, kSecret, ""));
  scoped_ptr<google::protobuf::MessageLite> login_copy(
      new mcs_proto::LoginRequest(*login_request));
  MCSMessage message(*login_copy);
  login_copy.reset();
  ASSERT_TRUE(message.IsValid());
  EXPECT_EQ(kLoginRequestTag, message.tag());
  EXPECT_EQ(login_request->ByteSize(), message.size());
  EXPECT_EQ(login_request->SerializeAsString(), message.SerializeAsString());
  EXPECT_EQ(login_request->SerializeAsString(),
            message.GetProtobuf().SerializeAsString());
  login_copy = message.CloneProtobuf();
  EXPECT_EQ(login_request->SerializeAsString(),
            login_copy->SerializeAsString());
}

TEST_F(MCSMessageTest, InitWithTag) {
  scoped_ptr<mcs_proto::LoginRequest> login_request(
      BuildLoginRequest(kAndroidId, kSecret, ""));
  scoped_ptr<google::protobuf::MessageLite> login_copy(
      new mcs_proto::LoginRequest(*login_request));
  MCSMessage message(kLoginRequestTag, *login_copy);
  login_copy.reset();
  ASSERT_TRUE(message.IsValid());
  EXPECT_EQ(kLoginRequestTag, message.tag());
  EXPECT_EQ(login_request->ByteSize(), message.size());
  EXPECT_EQ(login_request->SerializeAsString(), message.SerializeAsString());
  EXPECT_EQ(login_request->SerializeAsString(),
            message.GetProtobuf().SerializeAsString());
  login_copy = message.CloneProtobuf();
  EXPECT_EQ(login_request->SerializeAsString(),
            login_copy->SerializeAsString());
}

TEST_F(MCSMessageTest, InitPassOwnership) {
  scoped_ptr<mcs_proto::LoginRequest> login_request(
      BuildLoginRequest(kAndroidId, kSecret, ""));
  scoped_ptr<google::protobuf::MessageLite> login_copy(
      new mcs_proto::LoginRequest(*login_request));
  MCSMessage message(kLoginRequestTag,
                     login_copy.PassAs<const google::protobuf::MessageLite>());
  EXPECT_FALSE(login_copy.get());
  ASSERT_TRUE(message.IsValid());
  EXPECT_EQ(kLoginRequestTag, message.tag());
  EXPECT_EQ(login_request->ByteSize(), message.size());
  EXPECT_EQ(login_request->SerializeAsString(), message.SerializeAsString());
  EXPECT_EQ(login_request->SerializeAsString(),
            message.GetProtobuf().SerializeAsString());
  login_copy = message.CloneProtobuf();
  EXPECT_EQ(login_request->SerializeAsString(),
            login_copy->SerializeAsString());
}

}  // namespace gcm
