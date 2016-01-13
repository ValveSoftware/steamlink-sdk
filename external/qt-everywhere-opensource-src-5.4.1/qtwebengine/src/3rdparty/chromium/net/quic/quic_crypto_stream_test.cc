// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_crypto_stream.h"

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/quic_flags.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;
using std::vector;

namespace net {
namespace test {
namespace {

class MockQuicCryptoStream : public QuicCryptoStream {
 public:
  explicit MockQuicCryptoStream(QuicSession* session)
      : QuicCryptoStream(session) {
  }

  virtual void OnHandshakeMessage(
      const CryptoHandshakeMessage& message) OVERRIDE {
    messages_.push_back(message);
  }

  vector<CryptoHandshakeMessage>* messages() {
    return &messages_;
  }

 private:
  vector<CryptoHandshakeMessage> messages_;

  DISALLOW_COPY_AND_ASSIGN(MockQuicCryptoStream);
};

class QuicCryptoStreamTest : public ::testing::Test {
 public:
  QuicCryptoStreamTest()
      : connection_(new MockConnection(false)),
        session_(connection_),
        stream_(&session_) {
    message_.set_tag(kSHLO);
    message_.SetStringPiece(1, "abc");
    message_.SetStringPiece(2, "def");
    ConstructHandshakeMessage();
  }

  void ConstructHandshakeMessage() {
    CryptoFramer framer;
    message_data_.reset(framer.ConstructHandshakeMessage(message_));
  }

 protected:
  MockConnection* connection_;
  MockSession session_;
  MockQuicCryptoStream stream_;
  CryptoHandshakeMessage message_;
  scoped_ptr<QuicData> message_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicCryptoStreamTest);
};

TEST_F(QuicCryptoStreamTest, NotInitiallyConected) {
  EXPECT_FALSE(stream_.encryption_established());
  EXPECT_FALSE(stream_.handshake_confirmed());
}

TEST_F(QuicCryptoStreamTest, ProcessRawData) {
  EXPECT_EQ(message_data_->length(),
            stream_.ProcessRawData(message_data_->data(),
                                   message_data_->length()));
  ASSERT_EQ(1u, stream_.messages()->size());
  const CryptoHandshakeMessage& message = (*stream_.messages())[0];
  EXPECT_EQ(kSHLO, message.tag());
  EXPECT_EQ(2u, message.tag_value_map().size());
  EXPECT_EQ("abc", CryptoTestUtils::GetValueForTag(message, 1));
  EXPECT_EQ("def", CryptoTestUtils::GetValueForTag(message, 2));
}

TEST_F(QuicCryptoStreamTest, ProcessBadData) {
  string bad(message_data_->data(), message_data_->length());
  const int kFirstTagIndex = sizeof(uint32) +  // message tag
                             sizeof(uint16) +  // number of tag-value pairs
                             sizeof(uint16);   // padding
  EXPECT_EQ(1, bad[kFirstTagIndex]);
  bad[kFirstTagIndex] = 0x7F;  // out of order tag

  EXPECT_CALL(*connection_,
              SendConnectionClose(QUIC_CRYPTO_TAGS_OUT_OF_ORDER));
  EXPECT_EQ(0u, stream_.ProcessRawData(bad.data(), bad.length()));
}

TEST_F(QuicCryptoStreamTest, NoFlowControl) {
  ValueRestore<bool> old_flag(&FLAGS_enable_quic_stream_flow_control_2, true);
  EXPECT_FALSE(stream_.flow_controller()->IsEnabled());
}

}  // namespace
}  // namespace test
}  // namespace net
