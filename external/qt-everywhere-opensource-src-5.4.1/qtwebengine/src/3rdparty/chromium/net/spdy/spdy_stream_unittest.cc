// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "net/base/completion_callback.h"
#include "net/base/net_log_unittest.h"
#include "net/base/request_priority.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/buffered_spdy_framer.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_stream.h"
#include "net/spdy/spdy_stream_test_util.h"
#include "net/spdy/spdy_test_util_common.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(ukai): factor out common part with spdy_http_stream_unittest.cc
//
namespace net {

namespace test {

namespace {

const char kStreamUrl[] = "http://www.google.com/";
const char kPostBody[] = "\0hello!\xff";
const size_t kPostBodyLength = arraysize(kPostBody);
const base::StringPiece kPostBodyStringPiece(kPostBody, kPostBodyLength);

class SpdyStreamTest : public ::testing::Test,
                       public ::testing::WithParamInterface<NextProto> {
 protected:
  // A function that takes a SpdyStream and the number of bytes which
  // will unstall the next frame completely.
  typedef base::Callback<void(const base::WeakPtr<SpdyStream>&, int32)>
      UnstallFunction;

  SpdyStreamTest()
      : spdy_util_(GetParam()),
        session_deps_(GetParam()),
        offset_(0) {}

  base::WeakPtr<SpdySession> CreateDefaultSpdySession() {
    SpdySessionKey key(HostPortPair("www.google.com", 80),
                       ProxyServer::Direct(),
                       PRIVACY_MODE_DISABLED);
    return CreateInsecureSpdySession(session_, key, BoundNetLog());
  }

  virtual void TearDown() {
    base::MessageLoop::current()->RunUntilIdle();
  }

  void RunResumeAfterUnstallRequestResponseTest(
      const UnstallFunction& unstall_function);

  void RunResumeAfterUnstallBidirectionalTest(
      const UnstallFunction& unstall_function);

  // Add{Read,Write}() populates lists that are eventually passed to a
  // SocketData class. |frame| must live for the whole test.

  void AddRead(const SpdyFrame& frame) {
    reads_.push_back(CreateMockRead(frame, offset_++));
  }

  void AddWrite(const SpdyFrame& frame) {
    writes_.push_back(CreateMockWrite(frame, offset_++));
  }

  void AddReadEOF() {
    reads_.push_back(MockRead(ASYNC, 0, offset_++));
  }

  MockRead* GetReads() {
    return vector_as_array(&reads_);
  }

  size_t GetNumReads() const {
    return reads_.size();
  }

  MockWrite* GetWrites() {
    return vector_as_array(&writes_);
  }

  int GetNumWrites() const {
    return writes_.size();
  }

  SpdyTestUtil spdy_util_;
  SpdySessionDependencies session_deps_;
  scoped_refptr<HttpNetworkSession> session_;

 private:
  // Used by Add{Read,Write}() above.
  std::vector<MockWrite> writes_;
  std::vector<MockRead> reads_;
  int offset_;
};

INSTANTIATE_TEST_CASE_P(
    NextProto,
    SpdyStreamTest,
    testing::Values(kProtoDeprecatedSPDY2,
                    kProtoSPDY3, kProtoSPDY31, kProtoSPDY4));

TEST_P(SpdyStreamTest, SendDataAfterOpen) {
  GURL url(kStreamUrl);

  session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 1, kPostBodyLength, LOWEST, NULL, 0));
  AddWrite(*req);

  scoped_ptr<SpdyFrame> resp(
      spdy_util_.ConstructSpdyPostSynReply(NULL, 0));
  AddRead(*resp);

  scoped_ptr<SpdyFrame> msg(
      spdy_util_.ConstructSpdyBodyFrame(1, kPostBody, kPostBodyLength, false));
  AddWrite(*msg);

  scoped_ptr<SpdyFrame> echo(
      spdy_util_.ConstructSpdyBodyFrame(1, kPostBody, kPostBodyLength, false));
  AddRead(*echo);

  AddReadEOF();

  OrderedSocketData data(GetReads(), GetNumReads(),
                         GetWrites(), GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> session(CreateDefaultSpdySession());

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_BIDIRECTIONAL_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateSendImmediate delegate(stream, kPostBodyStringPiece);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrlFromHeaders());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kPostBodyLength));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream->GetUrlFromHeaders().spec());

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());

  EXPECT_TRUE(delegate.send_headers_completed());
  EXPECT_EQ("200", delegate.GetResponseHeaderValue(spdy_util_.GetStatusKey()));
  EXPECT_EQ(std::string(kPostBody, kPostBodyLength),
            delegate.TakeReceivedData());
  EXPECT_TRUE(data.at_write_eof());
}

TEST_P(SpdyStreamTest, PushedStream) {
  session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);

  AddReadEOF();

  OrderedSocketData data(GetReads(), GetNumReads(),
                         GetWrites(), GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> spdy_session(CreateDefaultSpdySession());

  // Conjure up a stream.
  SpdyStream stream(SPDY_PUSH_STREAM,
                    spdy_session,
                    GURL(),
                    DEFAULT_PRIORITY,
                    kSpdyStreamInitialWindowSize,
                    kSpdyStreamInitialWindowSize,
                    BoundNetLog());
  stream.set_stream_id(2);
  EXPECT_FALSE(stream.HasUrlFromHeaders());

  // Set required request headers.
  SpdyHeaderBlock request_headers;
  spdy_util_.AddUrlToHeaderBlock(kStreamUrl, &request_headers);
  stream.OnPushPromiseHeadersReceived(request_headers);

  // Send some basic response headers.
  SpdyHeaderBlock response;
  response[spdy_util_.GetStatusKey()] = "200";
  response[spdy_util_.GetVersionKey()] = "OK";
  stream.OnInitialResponseHeadersReceived(
      response, base::Time::Now(), base::TimeTicks::Now());

  // And some more headers.
  // TODO(baranovich): not valid for HTTP 2.
  SpdyHeaderBlock headers;
  headers["alpha"] = "beta";
  stream.OnAdditionalResponseHeadersReceived(headers);

  EXPECT_TRUE(stream.HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream.GetUrlFromHeaders().spec());

  StreamDelegateDoNothing delegate(stream.GetWeakPtr());
  stream.SetDelegate(&delegate);

  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ("200", delegate.GetResponseHeaderValue(spdy_util_.GetStatusKey()));
  EXPECT_EQ("beta", delegate.GetResponseHeaderValue("alpha"));

  EXPECT_TRUE(spdy_session == NULL);
}

TEST_P(SpdyStreamTest, StreamError) {
  GURL url(kStreamUrl);

  session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 1, kPostBodyLength, LOWEST, NULL, 0));
  AddWrite(*req);

  scoped_ptr<SpdyFrame> resp(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  AddRead(*resp);

  scoped_ptr<SpdyFrame> msg(
      spdy_util_.ConstructSpdyBodyFrame(1, kPostBody, kPostBodyLength, false));
  AddWrite(*msg);

  scoped_ptr<SpdyFrame> echo(
      spdy_util_.ConstructSpdyBodyFrame(1, kPostBody, kPostBodyLength, false));
  AddRead(*echo);

  AddReadEOF();

  CapturingBoundNetLog log;

  OrderedSocketData data(GetReads(), GetNumReads(),
                         GetWrites(), GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> session(CreateDefaultSpdySession());

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_BIDIRECTIONAL_STREAM, session, url, LOWEST, log.bound());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateSendImmediate delegate(stream, kPostBodyStringPiece);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrlFromHeaders());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kPostBodyLength));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream->GetUrlFromHeaders().spec());

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());

  const SpdyStreamId stream_id = delegate.stream_id();

  EXPECT_TRUE(delegate.send_headers_completed());
  EXPECT_EQ("200", delegate.GetResponseHeaderValue(spdy_util_.GetStatusKey()));
  EXPECT_EQ(std::string(kPostBody, kPostBodyLength),
            delegate.TakeReceivedData());
  EXPECT_TRUE(data.at_write_eof());

  // Check that the NetLog was filled reasonably.
  net::CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_LT(0u, entries.size());

  // Check that we logged SPDY_STREAM_ERROR correctly.
  int pos = net::ExpectLogContainsSomewhere(
      entries, 0,
      net::NetLog::TYPE_SPDY_STREAM_ERROR,
      net::NetLog::PHASE_NONE);

  int stream_id2;
  ASSERT_TRUE(entries[pos].GetIntegerValue("stream_id", &stream_id2));
  EXPECT_EQ(static_cast<int>(stream_id), stream_id2);
}

// Make sure that large blocks of data are properly split up into
// frame-sized chunks for a request/response (i.e., an HTTP-like)
// stream.
TEST_P(SpdyStreamTest, SendLargeDataAfterOpenRequestResponse) {
  GURL url(kStreamUrl);

  session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 1, kPostBodyLength, LOWEST, NULL, 0));
  AddWrite(*req);

  std::string chunk_data(kMaxSpdyFrameChunkSize, 'x');
  scoped_ptr<SpdyFrame> chunk(
      spdy_util_.ConstructSpdyBodyFrame(
          1, chunk_data.data(), chunk_data.length(), false));
  AddWrite(*chunk);
  AddWrite(*chunk);

  scoped_ptr<SpdyFrame> last_chunk(
      spdy_util_.ConstructSpdyBodyFrame(
          1, chunk_data.data(), chunk_data.length(), true));
  AddWrite(*last_chunk);

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyPostSynReply(NULL, 0));
  AddRead(*resp);

  AddReadEOF();

  OrderedSocketData data(GetReads(), GetNumReads(),
                         GetWrites(), GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> session(CreateDefaultSpdySession());

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_REQUEST_RESPONSE_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  std::string body_data(3 * kMaxSpdyFrameChunkSize, 'x');
  StreamDelegateWithBody delegate(stream, body_data);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrlFromHeaders());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kPostBodyLength));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream->GetUrlFromHeaders().spec());

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());

  EXPECT_TRUE(delegate.send_headers_completed());
  EXPECT_EQ("200", delegate.GetResponseHeaderValue(spdy_util_.GetStatusKey()));
  EXPECT_EQ(std::string(), delegate.TakeReceivedData());
  EXPECT_TRUE(data.at_write_eof());
}

// Make sure that large blocks of data are properly split up into
// frame-sized chunks for a bidirectional (i.e., non-HTTP-like)
// stream.
TEST_P(SpdyStreamTest, SendLargeDataAfterOpenBidirectional) {
  GURL url(kStreamUrl);

  session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 1, kPostBodyLength, LOWEST, NULL, 0));
  AddWrite(*req);

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyPostSynReply(NULL, 0));
  AddRead(*resp);

  std::string chunk_data(kMaxSpdyFrameChunkSize, 'x');
  scoped_ptr<SpdyFrame> chunk(
      spdy_util_.ConstructSpdyBodyFrame(
          1, chunk_data.data(), chunk_data.length(), false));
  AddWrite(*chunk);
  AddWrite(*chunk);
  AddWrite(*chunk);

  AddReadEOF();

  OrderedSocketData data(GetReads(), GetNumReads(),
                         GetWrites(), GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> session(CreateDefaultSpdySession());

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_BIDIRECTIONAL_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  std::string body_data(3 * kMaxSpdyFrameChunkSize, 'x');
  StreamDelegateSendImmediate delegate(stream, body_data);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrlFromHeaders());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kPostBodyLength));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream->GetUrlFromHeaders().spec());

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());

  EXPECT_TRUE(delegate.send_headers_completed());
  EXPECT_EQ("200", delegate.GetResponseHeaderValue(spdy_util_.GetStatusKey()));
  EXPECT_EQ(std::string(), delegate.TakeReceivedData());
  EXPECT_TRUE(data.at_write_eof());
}

// Receiving a header with uppercase ASCII should result in a protocol
// error.
TEST_P(SpdyStreamTest, UpperCaseHeaders) {
  GURL url(kStreamUrl);

  session_ =
      SpdySessionDependencies::SpdyCreateSessionDeterministic(&session_deps_);

  scoped_ptr<SpdyFrame> syn(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  AddWrite(*syn);

  const char* const kExtraHeaders[] = {"X-UpperCase", "yes"};
  scoped_ptr<SpdyFrame>
      reply(spdy_util_.ConstructSpdyGetSynReply(kExtraHeaders, 1, 1));
  AddRead(*reply);

  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_PROTOCOL_ERROR));
  AddWrite(*rst);

  AddReadEOF();

  DeterministicSocketData data(GetReads(), GetNumReads(),
                               GetWrites(), GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> session(CreateDefaultSpdySession());

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_REQUEST_RESPONSE_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateDoNothing delegate(stream);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrlFromHeaders());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructGetHeaderBlock(kStreamUrl));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), NO_MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream->GetUrlFromHeaders().spec());

  data.RunFor(4);

  EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, delegate.WaitForClose());
}

// Receiving a header with uppercase ASCII should result in a protocol
// error even for a push stream.
TEST_P(SpdyStreamTest, UpperCaseHeadersOnPush) {
  GURL url(kStreamUrl);

  session_ =
      SpdySessionDependencies::SpdyCreateSessionDeterministic(&session_deps_);

  scoped_ptr<SpdyFrame> syn(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  AddWrite(*syn);

  scoped_ptr<SpdyFrame>
      reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  AddRead(*reply);

  const char* const extra_headers[] = {"X-UpperCase", "yes"};
  scoped_ptr<SpdyFrame>
      push(spdy_util_.ConstructSpdyPush(extra_headers, 1, 2, 1, kStreamUrl));
  AddRead(*push);

  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(2, RST_STREAM_PROTOCOL_ERROR));
  AddWrite(*rst);

  AddReadEOF();

  DeterministicSocketData data(GetReads(), GetNumReads(),
                               GetWrites(), GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> session(CreateDefaultSpdySession());

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_REQUEST_RESPONSE_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateDoNothing delegate(stream);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrlFromHeaders());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructGetHeaderBlock(kStreamUrl));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), NO_MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream->GetUrlFromHeaders().spec());

  data.RunFor(4);

  base::WeakPtr<SpdyStream> push_stream;
  EXPECT_EQ(OK, session->GetPushStream(url, &push_stream, BoundNetLog()));
  EXPECT_FALSE(push_stream);

  data.RunFor(1);

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());
}

// Receiving a header with uppercase ASCII in a HEADERS frame should
// result in a protocol error.
TEST_P(SpdyStreamTest, UpperCaseHeadersInHeadersFrame) {
  GURL url(kStreamUrl);

  session_ =
      SpdySessionDependencies::SpdyCreateSessionDeterministic(&session_deps_);

  scoped_ptr<SpdyFrame> syn(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  AddWrite(*syn);

  scoped_ptr<SpdyFrame>
      reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  AddRead(*reply);

  scoped_ptr<SpdyFrame>
      push(spdy_util_.ConstructSpdyPush(NULL, 0, 2, 1, kStreamUrl));
  AddRead(*push);

  scoped_ptr<SpdyHeaderBlock> late_headers(new SpdyHeaderBlock());
  (*late_headers)["X-UpperCase"] = "yes";
  scoped_ptr<SpdyFrame> headers_frame(
      spdy_util_.ConstructSpdyControlFrame(late_headers.Pass(),
                                           false,
                                           2,
                                           LOWEST,
                                           HEADERS,
                                           CONTROL_FLAG_NONE,
                                           0));
  AddRead(*headers_frame);

  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(2, RST_STREAM_PROTOCOL_ERROR));
  AddWrite(*rst);

  AddReadEOF();

  DeterministicSocketData data(GetReads(), GetNumReads(),
                               GetWrites(), GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> session(CreateDefaultSpdySession());

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_REQUEST_RESPONSE_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateDoNothing delegate(stream);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrlFromHeaders());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructGetHeaderBlock(kStreamUrl));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), NO_MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream->GetUrlFromHeaders().spec());

  data.RunFor(3);

  base::WeakPtr<SpdyStream> push_stream;
  EXPECT_EQ(OK, session->GetPushStream(url, &push_stream, BoundNetLog()));
  EXPECT_TRUE(push_stream);

  data.RunFor(1);

  EXPECT_EQ(OK, session->GetPushStream(url, &push_stream, BoundNetLog()));
  EXPECT_FALSE(push_stream);

  data.RunFor(2);

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());
}

// Receiving a duplicate header in a HEADERS frame should result in a
// protocol error.
TEST_P(SpdyStreamTest, DuplicateHeaders) {
  GURL url(kStreamUrl);

  session_ =
      SpdySessionDependencies::SpdyCreateSessionDeterministic(&session_deps_);

  scoped_ptr<SpdyFrame> syn(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  AddWrite(*syn);

  scoped_ptr<SpdyFrame>
      reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  AddRead(*reply);

  scoped_ptr<SpdyFrame>
      push(spdy_util_.ConstructSpdyPush(NULL, 0, 2, 1, kStreamUrl));
  AddRead(*push);

  scoped_ptr<SpdyHeaderBlock> late_headers(new SpdyHeaderBlock());
  (*late_headers)[spdy_util_.GetStatusKey()] = "500 Server Error";
  scoped_ptr<SpdyFrame> headers_frame(
      spdy_util_.ConstructSpdyControlFrame(late_headers.Pass(),
                                           false,
                                           2,
                                           LOWEST,
                                           HEADERS,
                                           CONTROL_FLAG_NONE,
                                           0));
  AddRead(*headers_frame);

  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(2, RST_STREAM_PROTOCOL_ERROR));
  AddWrite(*rst);

  AddReadEOF();

  DeterministicSocketData data(GetReads(), GetNumReads(),
                               GetWrites(), GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> session(CreateDefaultSpdySession());

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_REQUEST_RESPONSE_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateDoNothing delegate(stream);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrlFromHeaders());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructGetHeaderBlock(kStreamUrl));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), NO_MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream->GetUrlFromHeaders().spec());

  data.RunFor(3);

  base::WeakPtr<SpdyStream> push_stream;
  EXPECT_EQ(OK, session->GetPushStream(url, &push_stream, BoundNetLog()));
  EXPECT_TRUE(push_stream);

  data.RunFor(1);

  EXPECT_EQ(OK, session->GetPushStream(url, &push_stream, BoundNetLog()));
  EXPECT_FALSE(push_stream);

  data.RunFor(2);

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());
}

// The tests below are only for SPDY/3 and above.

// Call IncreaseSendWindowSize on a stream with a large enough delta
// to overflow an int32. The SpdyStream should handle that case
// gracefully.
TEST_P(SpdyStreamTest, IncreaseSendWindowSizeOverflow) {
  if (spdy_util_.protocol() < kProtoSPDY3)
    return;

  session_ =
      SpdySessionDependencies::SpdyCreateSessionDeterministic(&session_deps_);

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 1, kPostBodyLength, LOWEST, NULL, 0));
  AddWrite(*req);

  // Triggered by the overflowing call to IncreaseSendWindowSize
  // below.
  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_FLOW_CONTROL_ERROR));
  AddWrite(*rst);

  AddReadEOF();

  CapturingBoundNetLog log;

  DeterministicSocketData data(GetReads(), GetNumReads(),
                               GetWrites(), GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> session(CreateDefaultSpdySession());
  GURL url(kStreamUrl);

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_BIDIRECTIONAL_STREAM, session, url, LOWEST, log.bound());
  ASSERT_TRUE(stream.get() != NULL);
  StreamDelegateSendImmediate delegate(stream, kPostBodyStringPiece);
  stream->SetDelegate(&delegate);

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kPostBodyLength));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream->GetUrlFromHeaders().spec());

  data.RunFor(1);

  int32 old_send_window_size = stream->send_window_size();
  ASSERT_GT(old_send_window_size, 0);
  int32 delta_window_size = kint32max - old_send_window_size + 1;
  stream->IncreaseSendWindowSize(delta_window_size);
  EXPECT_EQ(NULL, stream.get());

  data.RunFor(2);

  EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, delegate.WaitForClose());
}

// Functions used with
// RunResumeAfterUnstall{RequestResponse,Bidirectional}Test().

void StallStream(const base::WeakPtr<SpdyStream>& stream) {
  // Reduce the send window size to 0 to stall.
  while (stream->send_window_size() > 0) {
    stream->DecreaseSendWindowSize(
        std::min(kMaxSpdyFrameChunkSize, stream->send_window_size()));
  }
}

void IncreaseStreamSendWindowSize(const base::WeakPtr<SpdyStream>& stream,
                                  int32 delta_window_size) {
  EXPECT_TRUE(stream->send_stalled_by_flow_control());
  stream->IncreaseSendWindowSize(delta_window_size);
  EXPECT_FALSE(stream->send_stalled_by_flow_control());
}

void AdjustStreamSendWindowSize(const base::WeakPtr<SpdyStream>& stream,
                                int32 delta_window_size) {
  // Make sure that negative adjustments are handled properly.
  EXPECT_TRUE(stream->send_stalled_by_flow_control());
  stream->AdjustSendWindowSize(-delta_window_size);
  EXPECT_TRUE(stream->send_stalled_by_flow_control());
  stream->AdjustSendWindowSize(+delta_window_size);
  EXPECT_TRUE(stream->send_stalled_by_flow_control());
  stream->AdjustSendWindowSize(+delta_window_size);
  EXPECT_FALSE(stream->send_stalled_by_flow_control());
}

// Given an unstall function, runs a test to make sure that a
// request/response (i.e., an HTTP-like) stream resumes after a stall
// and unstall.
void SpdyStreamTest::RunResumeAfterUnstallRequestResponseTest(
    const UnstallFunction& unstall_function) {
  GURL url(kStreamUrl);

  session_ =
      SpdySessionDependencies::SpdyCreateSessionDeterministic(&session_deps_);

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 1, kPostBodyLength, LOWEST, NULL, 0));
  AddWrite(*req);

  scoped_ptr<SpdyFrame> body(
      spdy_util_.ConstructSpdyBodyFrame(1, kPostBody, kPostBodyLength, true));
  AddWrite(*body);

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  AddRead(*resp);

  AddReadEOF();

  DeterministicSocketData data(GetReads(), GetNumReads(),
                               GetWrites(), GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> session(CreateDefaultSpdySession());

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_REQUEST_RESPONSE_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateWithBody delegate(stream, kPostBodyStringPiece);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrlFromHeaders());
  EXPECT_FALSE(stream->send_stalled_by_flow_control());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kPostBodyLength));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream->GetUrlFromHeaders().spec());

  StallStream(stream);

  data.RunFor(1);

  EXPECT_TRUE(stream->send_stalled_by_flow_control());

  unstall_function.Run(stream, kPostBodyLength);

  EXPECT_FALSE(stream->send_stalled_by_flow_control());

  data.RunFor(3);

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());

  EXPECT_TRUE(delegate.send_headers_completed());
  EXPECT_EQ("200", delegate.GetResponseHeaderValue(":status"));
  EXPECT_EQ(std::string(), delegate.TakeReceivedData());
  EXPECT_TRUE(data.at_write_eof());
}

TEST_P(SpdyStreamTest, ResumeAfterSendWindowSizeIncreaseRequestResponse) {
  if (spdy_util_.protocol() < kProtoSPDY3)
    return;

  RunResumeAfterUnstallRequestResponseTest(
      base::Bind(&IncreaseStreamSendWindowSize));
}

TEST_P(SpdyStreamTest, ResumeAfterSendWindowSizeAdjustRequestResponse) {
  if (spdy_util_.protocol() < kProtoSPDY3)
    return;

  RunResumeAfterUnstallRequestResponseTest(
      base::Bind(&AdjustStreamSendWindowSize));
}

// Given an unstall function, runs a test to make sure that a
// bidirectional (i.e., non-HTTP-like) stream resumes after a stall
// and unstall.
void SpdyStreamTest::RunResumeAfterUnstallBidirectionalTest(
    const UnstallFunction& unstall_function) {
  GURL url(kStreamUrl);

  session_ =
      SpdySessionDependencies::SpdyCreateSessionDeterministic(&session_deps_);

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 1, kPostBodyLength, LOWEST, NULL, 0));
  AddWrite(*req);

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  AddRead(*resp);

  scoped_ptr<SpdyFrame> msg(
      spdy_util_.ConstructSpdyBodyFrame(1, kPostBody, kPostBodyLength, false));
  AddWrite(*msg);

  scoped_ptr<SpdyFrame> echo(
      spdy_util_.ConstructSpdyBodyFrame(1, kPostBody, kPostBodyLength, false));
  AddRead(*echo);

  AddReadEOF();

  DeterministicSocketData data(GetReads(), GetNumReads(),
                               GetWrites(), GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> session(CreateDefaultSpdySession());

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_BIDIRECTIONAL_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateSendImmediate delegate(stream, kPostBodyStringPiece);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrlFromHeaders());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kPostBodyLength));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream->GetUrlFromHeaders().spec());

  data.RunFor(1);

  EXPECT_FALSE(stream->send_stalled_by_flow_control());

  StallStream(stream);

  data.RunFor(1);

  EXPECT_TRUE(stream->send_stalled_by_flow_control());

  unstall_function.Run(stream, kPostBodyLength);

  EXPECT_FALSE(stream->send_stalled_by_flow_control());

  data.RunFor(3);

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());

  EXPECT_TRUE(delegate.send_headers_completed());
  EXPECT_EQ("200", delegate.GetResponseHeaderValue(":status"));
  EXPECT_EQ(std::string(kPostBody, kPostBodyLength),
            delegate.TakeReceivedData());
  EXPECT_TRUE(data.at_write_eof());
}

TEST_P(SpdyStreamTest, ResumeAfterSendWindowSizeIncreaseBidirectional) {
  if (spdy_util_.protocol() < kProtoSPDY3)
    return;

  RunResumeAfterUnstallBidirectionalTest(
      base::Bind(&IncreaseStreamSendWindowSize));
}

TEST_P(SpdyStreamTest, ResumeAfterSendWindowSizeAdjustBidirectional) {
  if (spdy_util_.protocol() < kProtoSPDY3)
    return;

  RunResumeAfterUnstallBidirectionalTest(
      base::Bind(&AdjustStreamSendWindowSize));
}

// Test calculation of amount of bytes received from network.
TEST_P(SpdyStreamTest, ReceivedBytes) {
  GURL url(kStreamUrl);

  session_ =
      SpdySessionDependencies::SpdyCreateSessionDeterministic(&session_deps_);

  scoped_ptr<SpdyFrame> syn(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  AddWrite(*syn);

  scoped_ptr<SpdyFrame>
      reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  AddRead(*reply);

  scoped_ptr<SpdyFrame> msg(
      spdy_util_.ConstructSpdyBodyFrame(1, kPostBody, kPostBodyLength, false));
  AddRead(*msg);

  AddReadEOF();

  DeterministicSocketData data(GetReads(), GetNumReads(),
                               GetWrites(), GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> session(CreateDefaultSpdySession());

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_REQUEST_RESPONSE_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateDoNothing delegate(stream);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrlFromHeaders());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructGetHeaderBlock(kStreamUrl));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), NO_MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream->GetUrlFromHeaders().spec());

  int64 reply_frame_len = reply->size();
  int64 data_header_len = spdy_util_.CreateFramer(false)
      ->GetDataFrameMinimumSize();
  int64 data_frame_len = data_header_len + kPostBodyLength;
  int64 response_len = reply_frame_len + data_frame_len;

  EXPECT_EQ(0, stream->raw_received_bytes());
  data.RunFor(1); // SYN
  EXPECT_EQ(0, stream->raw_received_bytes());
  data.RunFor(1); // REPLY
  EXPECT_EQ(reply_frame_len, stream->raw_received_bytes());
  data.RunFor(1); // DATA
  EXPECT_EQ(response_len, stream->raw_received_bytes());
  data.RunFor(1); // FIN

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());
}

}  // namespace

}  // namespace test

}  // namespace net
