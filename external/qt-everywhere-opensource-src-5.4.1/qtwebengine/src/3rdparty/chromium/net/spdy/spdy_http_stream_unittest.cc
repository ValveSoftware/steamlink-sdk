// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_http_stream.h"

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "crypto/ec_private_key.h"
#include "crypto/ec_signature_creator.h"
#include "crypto/signature_creator.h"
#include "net/base/capturing_net_log.h"
#include "net/base/load_timing_info.h"
#include "net/base/load_timing_info_test_util.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_element_reader.h"
#include "net/cert/asn1_util.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/default_server_bound_cert_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Tests the load timing of a stream that's connected and is not the first
// request sent on a connection.
void TestLoadTimingReused(const HttpStream& stream) {
  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(stream.GetLoadTimingInfo(&load_timing_info));

  EXPECT_TRUE(load_timing_info.socket_reused);
  EXPECT_NE(NetLog::Source::kInvalidId, load_timing_info.socket_log_id);

  ExpectConnectTimingHasNoTimes(load_timing_info.connect_timing);
  ExpectLoadTimingHasOnlyConnectionTimes(load_timing_info);
}

// Tests the load timing of a stream that's connected and using a fresh
// connection.
void TestLoadTimingNotReused(const HttpStream& stream) {
  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(stream.GetLoadTimingInfo(&load_timing_info));

  EXPECT_FALSE(load_timing_info.socket_reused);
  EXPECT_NE(NetLog::Source::kInvalidId, load_timing_info.socket_log_id);

  ExpectConnectTimingHasTimes(load_timing_info.connect_timing,
                              CONNECT_TIMING_HAS_DNS_TIMES);
  ExpectLoadTimingHasOnlyConnectionTimes(load_timing_info);
}

}  // namespace

class SpdyHttpStreamTest : public testing::Test,
                           public testing::WithParamInterface<NextProto> {
 public:
  SpdyHttpStreamTest()
      : spdy_util_(GetParam()),
        session_deps_(GetParam()) {
    session_deps_.net_log = &net_log_;
  }

  DeterministicSocketData* deterministic_data() {
    return deterministic_data_.get();
  }

  OrderedSocketData* data() { return data_.get(); }

 protected:
  virtual void TearDown() OVERRIDE {
    crypto::ECSignatureCreator::SetFactoryForTesting(NULL);
    base::MessageLoop::current()->RunUntilIdle();
  }

  // Initializes the session using DeterministicSocketData.  It's advisable
  // to use this function rather than the OrderedSocketData, since the
  // DeterministicSocketData behaves in a reasonable manner.
  void InitSessionDeterministic(MockRead* reads, size_t reads_count,
                                MockWrite* writes, size_t writes_count,
                                const SpdySessionKey& key) {
    deterministic_data_.reset(
        new DeterministicSocketData(reads, reads_count, writes, writes_count));
    session_deps_.deterministic_socket_factory->AddSocketDataProvider(
        deterministic_data_.get());
    http_session_ =
        SpdySessionDependencies::SpdyCreateSessionDeterministic(&session_deps_);
    session_ = CreateInsecureSpdySession(http_session_, key, BoundNetLog());
  }

  // Initializes the session using the finicky OrderedSocketData class.
  void InitSession(MockRead* reads, size_t reads_count,
                   MockWrite* writes, size_t writes_count,
                   const SpdySessionKey& key) {
    data_.reset(new OrderedSocketData(reads, reads_count,
                                      writes, writes_count));
    session_deps_.socket_factory->AddSocketDataProvider(data_.get());
    http_session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);
    session_ = CreateInsecureSpdySession(http_session_, key, BoundNetLog());
  }

  void TestSendCredentials(
    ServerBoundCertService* server_bound_cert_service,
    const std::string& cert,
    const std::string& proof);

  SpdyTestUtil spdy_util_;
  CapturingNetLog net_log_;
  SpdySessionDependencies session_deps_;
  scoped_ptr<OrderedSocketData> data_;
  scoped_ptr<DeterministicSocketData> deterministic_data_;
  scoped_refptr<HttpNetworkSession> http_session_;
  base::WeakPtr<SpdySession> session_;

 private:
  MockECSignatureCreatorFactory ec_signature_creator_factory_;
};

INSTANTIATE_TEST_CASE_P(
    NextProto,
    SpdyHttpStreamTest,
    testing::Values(kProtoDeprecatedSPDY2,
                    kProtoSPDY3, kProtoSPDY31, kProtoSPDY4));

// SpdyHttpStream::GetUploadProgress() should still work even before the
// stream is initialized.
TEST_P(SpdyHttpStreamTest, GetUploadProgressBeforeInitialization) {
  MockRead reads[] = {
    MockRead(ASYNC, 0, 0)  // EOF
  };

  HostPortPair host_port_pair("www.google.com", 80);
  SpdySessionKey key(host_port_pair, ProxyServer::Direct(),
                     PRIVACY_MODE_DISABLED);
  InitSession(reads, arraysize(reads), NULL, 0, key);

  SpdyHttpStream stream(session_, false);
  UploadProgress progress = stream.GetUploadProgress();
  EXPECT_EQ(0u, progress.size());
  EXPECT_EQ(0u, progress.position());

  // Pump the event loop so |reads| is consumed before the function returns.
  base::RunLoop().RunUntilIdle();
}

TEST_P(SpdyHttpStreamTest, SendRequest) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  MockWrite writes[] = {
    CreateMockWrite(*req.get(), 1),
  };
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp, 2),
    MockRead(SYNCHRONOUS, 0, 3)  // EOF
  };

  HostPortPair host_port_pair("www.google.com", 80);
  SpdySessionKey key(host_port_pair, ProxyServer::Direct(),
                     PRIVACY_MODE_DISABLED);
  InitSession(reads, arraysize(reads), writes, arraysize(writes), key);

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  TestCompletionCallback callback;
  HttpResponseInfo response;
  HttpRequestHeaders headers;
  BoundNetLog net_log;
  scoped_ptr<SpdyHttpStream> http_stream(new SpdyHttpStream(session_, true));
  // Make sure getting load timing information the stream early does not crash.
  LoadTimingInfo load_timing_info;
  EXPECT_FALSE(http_stream->GetLoadTimingInfo(&load_timing_info));

  ASSERT_EQ(
      OK,
      http_stream->InitializeStream(&request, DEFAULT_PRIORITY,
                                    net_log, CompletionCallback()));
  EXPECT_FALSE(http_stream->GetLoadTimingInfo(&load_timing_info));

  EXPECT_EQ(ERR_IO_PENDING, http_stream->SendRequest(headers, &response,
                                                     callback.callback()));
  EXPECT_TRUE(HasSpdySession(http_session_->spdy_session_pool(), key));
  EXPECT_FALSE(http_stream->GetLoadTimingInfo(&load_timing_info));

  // This triggers the MockWrite and read 2
  callback.WaitForResult();

  // Can get timing information once the stream connects.
  TestLoadTimingNotReused(*http_stream);

  // This triggers read 3. The empty read causes the session to shut down.
  data()->CompleteRead();

  // Because we abandoned the stream, we don't expect to find a session in the
  // pool anymore.
  EXPECT_FALSE(HasSpdySession(http_session_->spdy_session_pool(), key));
  EXPECT_TRUE(data()->at_read_eof());
  EXPECT_TRUE(data()->at_write_eof());

  TestLoadTimingNotReused(*http_stream);
  http_stream->Close(true);
  // Test that there's no crash when trying to get the load timing after the
  // stream has been closed.
  TestLoadTimingNotReused(*http_stream);
}

TEST_P(SpdyHttpStreamTest, LoadTimingTwoRequests) {
  scoped_ptr<SpdyFrame> req1(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> req2(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 3, LOWEST, true));
  MockWrite writes[] = {
    CreateMockWrite(*req1, 0),
    CreateMockWrite(*req2, 1),
  };
  scoped_ptr<SpdyFrame> resp1(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body1(
      spdy_util_.ConstructSpdyBodyFrame(1, "", 0, true));
  scoped_ptr<SpdyFrame> resp2(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<SpdyFrame> body2(
      spdy_util_.ConstructSpdyBodyFrame(3, "", 0, true));
  MockRead reads[] = {
    CreateMockRead(*resp1, 2),
    CreateMockRead(*body1, 3),
    CreateMockRead(*resp2, 4),
    CreateMockRead(*body2, 5),
    MockRead(ASYNC, 0, 6)  // EOF
  };

  HostPortPair host_port_pair("www.google.com", 80);
  SpdySessionKey key(host_port_pair, ProxyServer::Direct(),
                     PRIVACY_MODE_DISABLED);
  InitSessionDeterministic(reads, arraysize(reads),
                           writes, arraysize(writes),
                           key);

  HttpRequestInfo request1;
  request1.method = "GET";
  request1.url = GURL("http://www.google.com/");
  TestCompletionCallback callback1;
  HttpResponseInfo response1;
  HttpRequestHeaders headers1;
  scoped_ptr<SpdyHttpStream> http_stream1(new SpdyHttpStream(session_, true));

  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL("http://www.google.com/");
  TestCompletionCallback callback2;
  HttpResponseInfo response2;
  HttpRequestHeaders headers2;
  scoped_ptr<SpdyHttpStream> http_stream2(new SpdyHttpStream(session_, true));

  // First write.
  ASSERT_EQ(OK,
            http_stream1->InitializeStream(&request1, DEFAULT_PRIORITY,
                                           BoundNetLog(),
                                           CompletionCallback()));
  EXPECT_EQ(ERR_IO_PENDING, http_stream1->SendRequest(headers1, &response1,
                                                      callback1.callback()));
  EXPECT_TRUE(HasSpdySession(http_session_->spdy_session_pool(), key));

  deterministic_data()->RunFor(1);
  EXPECT_LE(0, callback1.WaitForResult());

  TestLoadTimingNotReused(*http_stream1);
  LoadTimingInfo load_timing_info1;
  LoadTimingInfo load_timing_info2;
  EXPECT_TRUE(http_stream1->GetLoadTimingInfo(&load_timing_info1));
  EXPECT_FALSE(http_stream2->GetLoadTimingInfo(&load_timing_info2));

  // Second write.
  ASSERT_EQ(OK,
            http_stream2->InitializeStream(&request2, DEFAULT_PRIORITY,
                                           BoundNetLog(),
                                           CompletionCallback()));
  EXPECT_EQ(ERR_IO_PENDING, http_stream2->SendRequest(headers2, &response2,
                                                      callback2.callback()));
  EXPECT_TRUE(HasSpdySession(http_session_->spdy_session_pool(), key));

  deterministic_data()->RunFor(1);
  EXPECT_LE(0, callback2.WaitForResult());
  TestLoadTimingReused(*http_stream2);
  EXPECT_TRUE(http_stream2->GetLoadTimingInfo(&load_timing_info2));
  EXPECT_EQ(load_timing_info1.socket_log_id, load_timing_info2.socket_log_id);

  // All the reads.
  deterministic_data()->RunFor(6);

  // Read stream 1 to completion, before making sure we can still read load
  // timing from both streams.
  scoped_refptr<IOBuffer> buf1(new IOBuffer(1));
  ASSERT_EQ(
      0, http_stream1->ReadResponseBody(buf1.get(), 1, callback1.callback()));

  // Stream 1 has been read to completion.
  TestLoadTimingNotReused(*http_stream1);
  // Stream 2 still has queued body data.
  TestLoadTimingReused(*http_stream2);
}

TEST_P(SpdyHttpStreamTest, SendChunkedPost) {
  BufferedSpdyFramer framer(spdy_util_.spdy_version(), false);

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructChunkedSpdyPost(NULL, 0));
  scoped_ptr<SpdyFrame> body(
      framer.CreateDataFrame(1, kUploadData, kUploadDataSize, DATA_FLAG_FIN));
  std::vector<MockWrite> writes;
  int seq = 0;
  writes.push_back(CreateMockWrite(*req, seq++));
  writes.push_back(CreateMockWrite(*body, seq++));  // POST upload frame

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyPostSynReply(NULL, 0));
  std::vector<MockRead> reads;
  reads.push_back(CreateMockRead(*resp, seq++));
  reads.push_back(CreateMockRead(*body, seq++));
  reads.push_back(MockRead(SYNCHRONOUS, 0, seq++));  // EOF

  HostPortPair host_port_pair("www.google.com", 80);
  SpdySessionKey key(host_port_pair, ProxyServer::Direct(),
                     PRIVACY_MODE_DISABLED);
  InitSession(vector_as_array(&reads), reads.size(),
              vector_as_array(&writes), writes.size(),
              key);
  EXPECT_EQ(spdy_util_.spdy_version(), session_->GetProtocolVersion());

  UploadDataStream upload_stream(UploadDataStream::CHUNKED, 0);
  const int kFirstChunkSize = kUploadDataSize/2;
  upload_stream.AppendChunk(kUploadData, kFirstChunkSize, false);
  upload_stream.AppendChunk(kUploadData + kFirstChunkSize,
                            kUploadDataSize - kFirstChunkSize, true);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  request.upload_data_stream = &upload_stream;

  ASSERT_EQ(OK, upload_stream.Init(CompletionCallback()));

  TestCompletionCallback callback;
  HttpResponseInfo response;
  HttpRequestHeaders headers;
  BoundNetLog net_log;
  SpdyHttpStream http_stream(session_, true);
  ASSERT_EQ(
      OK,
      http_stream.InitializeStream(&request, DEFAULT_PRIORITY,
                                   net_log, CompletionCallback()));

  EXPECT_EQ(ERR_IO_PENDING, http_stream.SendRequest(
      headers, &response, callback.callback()));
  EXPECT_TRUE(HasSpdySession(http_session_->spdy_session_pool(), key));

  // This results in writing the post body and reading the response headers.
  callback.WaitForResult();

  // This triggers reading the body and the EOF, causing the session to shut
  // down.
  data()->CompleteRead();
  base::MessageLoop::current()->RunUntilIdle();

  // Because we abandoned the stream, we don't expect to find a session in the
  // pool anymore.
  EXPECT_FALSE(HasSpdySession(http_session_->spdy_session_pool(), key));
  EXPECT_TRUE(data()->at_read_eof());
  EXPECT_TRUE(data()->at_write_eof());
}

// Test to ensure the SpdyStream state machine does not get confused when a
// chunk becomes available while a write is pending.
TEST_P(SpdyHttpStreamTest, DelayedSendChunkedPost) {
  const char kUploadData1[] = "12345678";
  const int kUploadData1Size = arraysize(kUploadData1)-1;
  scoped_ptr<SpdyFrame> req(spdy_util_.ConstructChunkedSpdyPost(NULL, 0));
  scoped_ptr<SpdyFrame> chunk1(spdy_util_.ConstructSpdyBodyFrame(1, false));
  scoped_ptr<SpdyFrame> chunk2(
      spdy_util_.ConstructSpdyBodyFrame(
          1, kUploadData1, kUploadData1Size, false));
  scoped_ptr<SpdyFrame> chunk3(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*req.get(), 0),
    CreateMockWrite(*chunk1, 1),  // POST upload frames
    CreateMockWrite(*chunk2, 2),
    CreateMockWrite(*chunk3, 3),
  };
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyPostSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*resp, 4),
    CreateMockRead(*chunk1, 5),
    CreateMockRead(*chunk2, 6),
    CreateMockRead(*chunk3, 7),
    MockRead(ASYNC, 0, 8)  // EOF
  };

  HostPortPair host_port_pair("www.google.com", 80);
  SpdySessionKey key(host_port_pair, ProxyServer::Direct(),
                     PRIVACY_MODE_DISABLED);
  InitSessionDeterministic(reads, arraysize(reads),
                           writes, arraysize(writes),
                           key);

  UploadDataStream upload_stream(UploadDataStream::CHUNKED, 0);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  request.upload_data_stream = &upload_stream;

  ASSERT_EQ(OK, upload_stream.Init(CompletionCallback()));
  upload_stream.AppendChunk(kUploadData, kUploadDataSize, false);

  BoundNetLog net_log;
  scoped_ptr<SpdyHttpStream> http_stream(new SpdyHttpStream(session_, true));
  ASSERT_EQ(OK, http_stream->InitializeStream(&request, DEFAULT_PRIORITY,
                                              net_log, CompletionCallback()));

  TestCompletionCallback callback;
  HttpRequestHeaders headers;
  HttpResponseInfo response;
  // This will attempt to Write() the initial request and headers, which will
  // complete asynchronously.
  EXPECT_EQ(ERR_IO_PENDING, http_stream->SendRequest(headers, &response,
                                                     callback.callback()));
  EXPECT_TRUE(HasSpdySession(http_session_->spdy_session_pool(), key));

  // Complete the initial request write and the first chunk.
  deterministic_data()->RunFor(2);
  ASSERT_TRUE(callback.have_result());
  EXPECT_EQ(OK, callback.WaitForResult());

  // Now append the final two chunks which will enqueue two more writes.
  upload_stream.AppendChunk(kUploadData1, kUploadData1Size, false);
  upload_stream.AppendChunk(kUploadData, kUploadDataSize, true);

  // Finish writing all the chunks.
  deterministic_data()->RunFor(2);

  // Read response headers.
  deterministic_data()->RunFor(1);
  ASSERT_EQ(OK, http_stream->ReadResponseHeaders(callback.callback()));

  // Read and check |chunk1| response.
  deterministic_data()->RunFor(1);
  scoped_refptr<IOBuffer> buf1(new IOBuffer(kUploadDataSize));
  ASSERT_EQ(kUploadDataSize,
            http_stream->ReadResponseBody(
                buf1.get(), kUploadDataSize, callback.callback()));
  EXPECT_EQ(kUploadData, std::string(buf1->data(), kUploadDataSize));

  // Read and check |chunk2| response.
  deterministic_data()->RunFor(1);
  scoped_refptr<IOBuffer> buf2(new IOBuffer(kUploadData1Size));
  ASSERT_EQ(kUploadData1Size,
            http_stream->ReadResponseBody(
                buf2.get(), kUploadData1Size, callback.callback()));
  EXPECT_EQ(kUploadData1, std::string(buf2->data(), kUploadData1Size));

  // Read and check |chunk3| response.
  deterministic_data()->RunFor(1);
  scoped_refptr<IOBuffer> buf3(new IOBuffer(kUploadDataSize));
  ASSERT_EQ(kUploadDataSize,
            http_stream->ReadResponseBody(
                buf3.get(), kUploadDataSize, callback.callback()));
  EXPECT_EQ(kUploadData, std::string(buf3->data(), kUploadDataSize));

  // Finish reading the |EOF|.
  deterministic_data()->RunFor(1);
  ASSERT_TRUE(response.headers.get());
  ASSERT_EQ(200, response.headers->response_code());
  EXPECT_TRUE(deterministic_data()->at_read_eof());
  EXPECT_TRUE(deterministic_data()->at_write_eof());
}

// Test case for bug: http://code.google.com/p/chromium/issues/detail?id=50058
TEST_P(SpdyHttpStreamTest, SpdyURLTest) {
  const char * const full_url = "http://www.google.com/foo?query=what#anchor";
  const char * const base_url = "http://www.google.com/foo?query=what";
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet(base_url, false, 1, LOWEST));
  MockWrite writes[] = {
    CreateMockWrite(*req.get(), 1),
  };
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp, 2),
    MockRead(SYNCHRONOUS, 0, 3)  // EOF
  };

  HostPortPair host_port_pair("www.google.com", 80);
  SpdySessionKey key(host_port_pair, ProxyServer::Direct(),
                     PRIVACY_MODE_DISABLED);
  InitSession(reads, arraysize(reads), writes, arraysize(writes), key);

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL(full_url);
  TestCompletionCallback callback;
  HttpResponseInfo response;
  HttpRequestHeaders headers;
  BoundNetLog net_log;
  scoped_ptr<SpdyHttpStream> http_stream(new SpdyHttpStream(session_, true));
  ASSERT_EQ(OK,
            http_stream->InitializeStream(
                &request, DEFAULT_PRIORITY, net_log, CompletionCallback()));

  EXPECT_EQ(ERR_IO_PENDING, http_stream->SendRequest(headers, &response,
                                                     callback.callback()));

  EXPECT_EQ(base_url, http_stream->stream()->GetUrlFromHeaders().spec());

  // This triggers the MockWrite and read 2
  callback.WaitForResult();

  // This triggers read 3. The empty read causes the session to shut down.
  data()->CompleteRead();

  // Because we abandoned the stream, we don't expect to find a session in the
  // pool anymore.
  EXPECT_FALSE(HasSpdySession(http_session_->spdy_session_pool(), key));
  EXPECT_TRUE(data()->at_read_eof());
  EXPECT_TRUE(data()->at_write_eof());
}

// The tests below are only for SPDY/3 and above.

// Test the receipt of a WINDOW_UPDATE frame while waiting for a chunk to be
// made available is handled correctly.
TEST_P(SpdyHttpStreamTest, DelayedSendChunkedPostWithWindowUpdate) {
  if (GetParam() < kProtoSPDY3)
    return;

  scoped_ptr<SpdyFrame> req(spdy_util_.ConstructChunkedSpdyPost(NULL, 0));
  scoped_ptr<SpdyFrame> chunk1(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*req.get(), 0),
    CreateMockWrite(*chunk1, 1),
  };
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyPostSynReply(NULL, 0));
  scoped_ptr<SpdyFrame> window_update(
      spdy_util_.ConstructSpdyWindowUpdate(1, kUploadDataSize));
  MockRead reads[] = {
    CreateMockRead(*window_update, 2),
    CreateMockRead(*resp, 3),
    CreateMockRead(*chunk1, 4),
    MockRead(ASYNC, 0, 5)  // EOF
  };

  HostPortPair host_port_pair("www.google.com", 80);
  SpdySessionKey key(host_port_pair, ProxyServer::Direct(),
                     PRIVACY_MODE_DISABLED);

  InitSessionDeterministic(reads, arraysize(reads),
                           writes, arraysize(writes),
                           key);

  UploadDataStream upload_stream(UploadDataStream::CHUNKED, 0);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  request.upload_data_stream = &upload_stream;

  ASSERT_EQ(OK, upload_stream.Init(CompletionCallback()));
  upload_stream.AppendChunk(kUploadData, kUploadDataSize, true);

  BoundNetLog net_log;
  scoped_ptr<SpdyHttpStream> http_stream(new SpdyHttpStream(session_, true));
  ASSERT_EQ(OK, http_stream->InitializeStream(&request, DEFAULT_PRIORITY,
                                              net_log, CompletionCallback()));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  // This will attempt to Write() the initial request and headers, which will
  // complete asynchronously.
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING, http_stream->SendRequest(headers, &response,
                                                     callback.callback()));
  EXPECT_TRUE(HasSpdySession(http_session_->spdy_session_pool(), key));

  // Complete the initial request write and first chunk.
  deterministic_data_->RunFor(2);
  ASSERT_TRUE(callback.have_result());
  EXPECT_EQ(OK, callback.WaitForResult());

  // Verify that the window size has decreased.
  ASSERT_TRUE(http_stream->stream() != NULL);
  EXPECT_NE(static_cast<int>(kSpdyStreamInitialWindowSize),
            http_stream->stream()->send_window_size());

  // Read window update.
  deterministic_data_->RunFor(1);

  // Verify the window update.
  ASSERT_TRUE(http_stream->stream() != NULL);
  EXPECT_EQ(static_cast<int>(kSpdyStreamInitialWindowSize),
            http_stream->stream()->send_window_size());

  // Read response headers.
  deterministic_data_->RunFor(1);
  ASSERT_EQ(OK, http_stream->ReadResponseHeaders(callback.callback()));

  // Read and check |chunk1| response.
  deterministic_data_->RunFor(1);
  scoped_refptr<IOBuffer> buf1(new IOBuffer(kUploadDataSize));
  ASSERT_EQ(kUploadDataSize,
            http_stream->ReadResponseBody(
                buf1.get(), kUploadDataSize, callback.callback()));
  EXPECT_EQ(kUploadData, std::string(buf1->data(), kUploadDataSize));

  // Finish reading the |EOF|.
  deterministic_data_->RunFor(1);
  ASSERT_TRUE(response.headers.get());
  ASSERT_EQ(200, response.headers->response_code());
  EXPECT_TRUE(deterministic_data_->at_read_eof());
  EXPECT_TRUE(deterministic_data_->at_write_eof());
}

// TODO(willchan): Write a longer test for SpdyStream that exercises all
// methods.

}  // namespace net
