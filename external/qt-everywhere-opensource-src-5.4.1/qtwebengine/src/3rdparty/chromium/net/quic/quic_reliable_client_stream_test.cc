// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_reliable_client_stream.h"

#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/quic/quic_client_session.h"
#include "net/quic/quic_utils.h"
#include "net/quic/spdy_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::AnyNumber;
using testing::Return;
using testing::StrEq;
using testing::_;

namespace net {
namespace test {
namespace {

const QuicConnectionId kStreamId = 3;

class MockDelegate : public QuicReliableClientStream::Delegate {
 public:
  MockDelegate() {}

  MOCK_METHOD0(OnSendData, int());
  MOCK_METHOD2(OnSendDataComplete, int(int, bool*));
  MOCK_METHOD2(OnDataReceived, int(const char*, int));
  MOCK_METHOD1(OnClose, void(QuicErrorCode));
  MOCK_METHOD1(OnError, void(int));
  MOCK_METHOD0(HasSendHeadersComplete, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDelegate);
};

class QuicReliableClientStreamTest
    : public ::testing::TestWithParam<QuicVersion> {
 public:
  QuicReliableClientStreamTest()
      : session_(new MockConnection(false, SupportedVersions(GetParam()))) {
    stream_ = new QuicReliableClientStream(kStreamId, &session_, BoundNetLog());
    session_.ActivateStream(stream_);
    stream_->SetDelegate(&delegate_);
  }

  void InitializeHeaders() {
    headers_[":host"] = "www.google.com";
    headers_[":path"] = "/index.hml";
    headers_[":scheme"] = "https";
    headers_["cookie"] =
        "__utma=208381060.1228362404.1372200928.1372200928.1372200928.1; "
        "__utmc=160408618; "
        "GX=DQAAAOEAAACWJYdewdE9rIrW6qw3PtVi2-d729qaa-74KqOsM1NVQblK4VhX"
        "hoALMsy6HOdDad2Sz0flUByv7etmo3mLMidGrBoljqO9hSVA40SLqpG_iuKKSHX"
        "RW3Np4bq0F0SDGDNsW0DSmTS9ufMRrlpARJDS7qAI6M3bghqJp4eABKZiRqebHT"
        "pMU-RXvTI5D5oCF1vYxYofH_l1Kviuiy3oQ1kS1enqWgbhJ2t61_SNdv-1XJIS0"
        "O3YeHLmVCs62O6zp89QwakfAWK9d3IDQvVSJzCQsvxvNIvaZFa567MawWlXg0Rh"
        "1zFMi5vzcns38-8_Sns; "
        "GA=v*2%2Fmem*57968640*47239936%2Fmem*57968640*47114716%2Fno-nm-"
        "yj*15%2Fno-cc-yj*5%2Fpc-ch*133685%2Fpc-s-cr*133947%2Fpc-s-t*1339"
        "47%2Fno-nm-yj*4%2Fno-cc-yj*1%2Fceft-as*1%2Fceft-nqas*0%2Fad-ra-c"
        "v_p%2Fad-nr-cv_p-f*1%2Fad-v-cv_p*859%2Fad-ns-cv_p-f*1%2Ffn-v-ad%"
        "2Fpc-t*250%2Fpc-cm*461%2Fpc-s-cr*722%2Fpc-s-t*722%2Fau_p*4"
        "SICAID=AJKiYcHdKgxum7KMXG0ei2t1-W4OD1uW-ecNsCqC0wDuAXiDGIcT_HA2o1"
        "3Rs1UKCuBAF9g8rWNOFbxt8PSNSHFuIhOo2t6bJAVpCsMU5Laa6lewuTMYI8MzdQP"
        "ARHKyW-koxuhMZHUnGBJAM1gJODe0cATO_KGoX4pbbFxxJ5IicRxOrWK_5rU3cdy6"
        "edlR9FsEdH6iujMcHkbE5l18ehJDwTWmBKBzVD87naobhMMrF6VvnDGxQVGp9Ir_b"
        "Rgj3RWUoPumQVCxtSOBdX0GlJOEcDTNCzQIm9BSfetog_eP_TfYubKudt5eMsXmN6"
        "QnyXHeGeK2UINUzJ-D30AFcpqYgH9_1BvYSpi7fc7_ydBU8TaD8ZRxvtnzXqj0RfG"
        "tuHghmv3aD-uzSYJ75XDdzKdizZ86IG6Fbn1XFhYZM-fbHhm3mVEXnyRW4ZuNOLFk"
        "Fas6LMcVC6Q8QLlHYbXBpdNFuGbuZGUnav5C-2I_-46lL0NGg3GewxGKGHvHEfoyn"
        "EFFlEYHsBQ98rXImL8ySDycdLEFvBPdtctPmWCfTxwmoSMLHU2SCVDhbqMWU5b0yr"
        "JBCScs_ejbKaqBDoB7ZGxTvqlrB__2ZmnHHjCr8RgMRtKNtIeuZAo ";
  }

  testing::StrictMock<MockDelegate> delegate_;
  MockSession session_;
  QuicReliableClientStream* stream_;
  QuicCryptoClientConfig crypto_config_;
  SpdyHeaderBlock headers_;
};

INSTANTIATE_TEST_CASE_P(Version, QuicReliableClientStreamTest,
                        ::testing::ValuesIn(QuicSupportedVersions()));

TEST_P(QuicReliableClientStreamTest, OnFinRead) {
  InitializeHeaders();
  string uncompressed_headers =
      SpdyUtils::SerializeUncompressedHeaders(headers_);
  EXPECT_CALL(delegate_, OnDataReceived(StrEq(uncompressed_headers.data()),
                                        uncompressed_headers.size()));
  QuicStreamOffset offset = 0;
  stream_->OnStreamHeaders(uncompressed_headers);
  stream_->OnStreamHeadersComplete(false, uncompressed_headers.length());

  IOVector iov;
  QuicStreamFrame frame2(kStreamId, true, offset, iov);
  EXPECT_CALL(delegate_, OnClose(QUIC_NO_ERROR));
  stream_->OnStreamFrame(frame2);
}

TEST_P(QuicReliableClientStreamTest, ProcessData) {
  const char data[] = "hello world!";
  EXPECT_CALL(delegate_, OnDataReceived(StrEq(data), arraysize(data)));
  EXPECT_CALL(delegate_, OnClose(QUIC_NO_ERROR));

  EXPECT_EQ(arraysize(data), stream_->ProcessData(data, arraysize(data)));
}

TEST_P(QuicReliableClientStreamTest, ProcessDataWithError) {
  const char data[] = "hello world!";
  EXPECT_CALL(delegate_,
              OnDataReceived(StrEq(data),
                             arraysize(data))).WillOnce(Return(ERR_UNEXPECTED));
  EXPECT_CALL(delegate_, OnClose(QUIC_NO_ERROR));


  EXPECT_EQ(0u, stream_->ProcessData(data, arraysize(data)));
}

TEST_P(QuicReliableClientStreamTest, OnError) {
  EXPECT_CALL(delegate_, OnError(ERR_INTERNET_DISCONNECTED));

  stream_->OnError(ERR_INTERNET_DISCONNECTED);
  EXPECT_FALSE(stream_->GetDelegate());
}

TEST_P(QuicReliableClientStreamTest, WriteStreamData) {
  EXPECT_CALL(delegate_, OnClose(QUIC_NO_ERROR));

  const char kData1[] = "hello world";
  const size_t kDataLen = arraysize(kData1);

  // All data written.
  EXPECT_CALL(session_, WritevData(stream_->id(), _,  _, _, _, _)).WillOnce(
      Return(QuicConsumedData(kDataLen, true)));
  TestCompletionCallback callback;
  EXPECT_EQ(OK, stream_->WriteStreamData(base::StringPiece(kData1, kDataLen),
                                         true, callback.callback()));
}

TEST_P(QuicReliableClientStreamTest, WriteStreamDataAsync) {
  EXPECT_CALL(delegate_, HasSendHeadersComplete()).Times(AnyNumber());
  EXPECT_CALL(delegate_, OnClose(QUIC_NO_ERROR));

  const char kData1[] = "hello world";
  const size_t kDataLen = arraysize(kData1);

  // No data written.
  EXPECT_CALL(session_, WritevData(stream_->id(),  _, _, _, _, _)).WillOnce(
      Return(QuicConsumedData(0, false)));
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING,
            stream_->WriteStreamData(base::StringPiece(kData1, kDataLen),
                                     true, callback.callback()));
  ASSERT_FALSE(callback.have_result());

  // All data written.
  EXPECT_CALL(session_, WritevData(stream_->id(),  _, _, _, _, _)).WillOnce(
      Return(QuicConsumedData(kDataLen, true)));
  stream_->OnCanWrite();
  ASSERT_TRUE(callback.have_result());
  EXPECT_EQ(OK, callback.WaitForResult());
}

}  // namespace
}  // namespace test
}  // namespace net
