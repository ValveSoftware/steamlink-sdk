// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_parser.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_file_element_reader.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

const size_t kOutputSize = 1024;  // Just large enough for this test.
// The number of bytes that can fit in a buffer of kOutputSize.
const size_t kMaxPayloadSize =
    kOutputSize - HttpStreamParser::kChunkHeaderFooterSize;

// The empty payload is how the last chunk is encoded.
TEST(HttpStreamParser, EncodeChunk_EmptyPayload) {
  char output[kOutputSize];

  const base::StringPiece kPayload = "";
  const base::StringPiece kExpected = "0\r\n\r\n";
  const int num_bytes_written =
      HttpStreamParser::EncodeChunk(kPayload, output, sizeof(output));
  ASSERT_EQ(kExpected.size(), static_cast<size_t>(num_bytes_written));
  EXPECT_EQ(kExpected, base::StringPiece(output, num_bytes_written));
}

TEST(HttpStreamParser, EncodeChunk_ShortPayload) {
  char output[kOutputSize];

  const std::string kPayload("foo\x00\x11\x22", 6);
  // 11 = payload size + sizeof("6") + CRLF x 2.
  const std::string kExpected("6\r\nfoo\x00\x11\x22\r\n", 11);
  const int num_bytes_written =
      HttpStreamParser::EncodeChunk(kPayload, output, sizeof(output));
  ASSERT_EQ(kExpected.size(), static_cast<size_t>(num_bytes_written));
  EXPECT_EQ(kExpected, base::StringPiece(output, num_bytes_written));
}

TEST(HttpStreamParser, EncodeChunk_LargePayload) {
  char output[kOutputSize];

  const std::string kPayload(1000, '\xff');  // '\xff' x 1000.
  // 3E8 = 1000 in hex.
  const std::string kExpected = "3E8\r\n" + kPayload + "\r\n";
  const int num_bytes_written =
      HttpStreamParser::EncodeChunk(kPayload, output, sizeof(output));
  ASSERT_EQ(kExpected.size(), static_cast<size_t>(num_bytes_written));
  EXPECT_EQ(kExpected, base::StringPiece(output, num_bytes_written));
}

TEST(HttpStreamParser, EncodeChunk_FullPayload) {
  char output[kOutputSize];

  const std::string kPayload(kMaxPayloadSize, '\xff');
  // 3F4 = 1012 in hex.
  const std::string kExpected = "3F4\r\n" + kPayload + "\r\n";
  const int num_bytes_written =
      HttpStreamParser::EncodeChunk(kPayload, output, sizeof(output));
  ASSERT_EQ(kExpected.size(), static_cast<size_t>(num_bytes_written));
  EXPECT_EQ(kExpected, base::StringPiece(output, num_bytes_written));
}

TEST(HttpStreamParser, EncodeChunk_TooLargePayload) {
  char output[kOutputSize];

  // The payload is one byte larger the output buffer size.
  const std::string kPayload(kMaxPayloadSize + 1, '\xff');
  const int num_bytes_written =
      HttpStreamParser::EncodeChunk(kPayload, output, sizeof(output));
  ASSERT_EQ(ERR_INVALID_ARGUMENT, num_bytes_written);
}

TEST(HttpStreamParser, ShouldMergeRequestHeadersAndBody_NoBody) {
  // Shouldn't be merged if upload data is non-existent.
  ASSERT_FALSE(HttpStreamParser::ShouldMergeRequestHeadersAndBody(
      "some header", NULL));
}

TEST(HttpStreamParser, ShouldMergeRequestHeadersAndBody_EmptyBody) {
  ScopedVector<UploadElementReader> element_readers;
  scoped_ptr<UploadDataStream> body(
      new UploadDataStream(element_readers.Pass(), 0));
  ASSERT_EQ(OK, body->Init(CompletionCallback()));
  // Shouldn't be merged if upload data is empty.
  ASSERT_FALSE(HttpStreamParser::ShouldMergeRequestHeadersAndBody(
      "some header", body.get()));
}

TEST(HttpStreamParser, ShouldMergeRequestHeadersAndBody_ChunkedBody) {
  const std::string payload = "123";
  scoped_ptr<UploadDataStream> body(
      new UploadDataStream(UploadDataStream::CHUNKED, 0));
  body->AppendChunk(payload.data(), payload.size(), true);
  ASSERT_EQ(OK, body->Init(CompletionCallback()));
  // Shouldn't be merged if upload data carries chunked data.
  ASSERT_FALSE(HttpStreamParser::ShouldMergeRequestHeadersAndBody(
      "some header", body.get()));
}

TEST(HttpStreamParser, ShouldMergeRequestHeadersAndBody_FileBody) {
  {
    ScopedVector<UploadElementReader> element_readers;

    // Create an empty temporary file.
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath temp_file_path;
    ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.path(),
                                               &temp_file_path));

    element_readers.push_back(
        new UploadFileElementReader(base::MessageLoopProxy::current().get(),
                                    temp_file_path,
                                    0,
                                    0,
                                    base::Time()));

    scoped_ptr<UploadDataStream> body(
        new UploadDataStream(element_readers.Pass(), 0));
    TestCompletionCallback callback;
    ASSERT_EQ(ERR_IO_PENDING, body->Init(callback.callback()));
    ASSERT_EQ(OK, callback.WaitForResult());
    // Shouldn't be merged if upload data carries a file, as it's not in-memory.
    ASSERT_FALSE(HttpStreamParser::ShouldMergeRequestHeadersAndBody(
        "some header", body.get()));
  }
  // UploadFileElementReaders may post clean-up tasks on destruction.
  base::RunLoop().RunUntilIdle();
}

TEST(HttpStreamParser, ShouldMergeRequestHeadersAndBody_SmallBodyInMemory) {
  ScopedVector<UploadElementReader> element_readers;
  const std::string payload = "123";
  element_readers.push_back(new UploadBytesElementReader(
      payload.data(), payload.size()));

  scoped_ptr<UploadDataStream> body(
      new UploadDataStream(element_readers.Pass(), 0));
  ASSERT_EQ(OK, body->Init(CompletionCallback()));
  // Yes, should be merged if the in-memory body is small here.
  ASSERT_TRUE(HttpStreamParser::ShouldMergeRequestHeadersAndBody(
      "some header", body.get()));
}

TEST(HttpStreamParser, ShouldMergeRequestHeadersAndBody_LargeBodyInMemory) {
  ScopedVector<UploadElementReader> element_readers;
  const std::string payload(10000, 'a');  // 'a' x 10000.
  element_readers.push_back(new UploadBytesElementReader(
      payload.data(), payload.size()));

  scoped_ptr<UploadDataStream> body(
      new UploadDataStream(element_readers.Pass(), 0));
  ASSERT_EQ(OK, body->Init(CompletionCallback()));
  // Shouldn't be merged if the in-memory body is large here.
  ASSERT_FALSE(HttpStreamParser::ShouldMergeRequestHeadersAndBody(
      "some header", body.get()));
}

// Test to ensure the HttpStreamParser state machine does not get confused
// when sending a request with a chunked body, where chunks become available
// asynchronously, over a socket where writes may also complete
// asynchronously.
// This is a regression test for http://crbug.com/132243
TEST(HttpStreamParser, AsyncChunkAndAsyncSocket) {
  // The chunks that will be written in the request, as reflected in the
  // MockWrites below.
  static const char kChunk1[] = "Chunk 1";
  static const char kChunk2[] = "Chunky 2";
  static const char kChunk3[] = "Test 3";

  MockWrite writes[] = {
    MockWrite(ASYNC, 0,
              "GET /one.html HTTP/1.1\r\n"
              "Host: localhost\r\n"
              "Transfer-Encoding: chunked\r\n"
              "Connection: keep-alive\r\n\r\n"),
    MockWrite(ASYNC, 1, "7\r\nChunk 1\r\n"),
    MockWrite(ASYNC, 2, "8\r\nChunky 2\r\n"),
    MockWrite(ASYNC, 3, "6\r\nTest 3\r\n"),
    MockWrite(ASYNC, 4, "0\r\n\r\n"),
  };

  // The size of the response body, as reflected in the Content-Length of the
  // MockRead below.
  static const int kBodySize = 8;

  MockRead reads[] = {
    MockRead(ASYNC, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 6, "Content-Length: 8\r\n\r\n"),
    MockRead(ASYNC, 7, "one.html"),
    MockRead(SYNCHRONOUS, 0, 8),  // EOF
  };

  UploadDataStream upload_stream(UploadDataStream::CHUNKED, 0);
  upload_stream.AppendChunk(kChunk1, arraysize(kChunk1) - 1, false);
  ASSERT_EQ(OK, upload_stream.Init(CompletionCallback()));

  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));

  scoped_ptr<DeterministicMockTCPClientSocket> transport(
      new DeterministicMockTCPClientSocket(NULL, &data));
  data.set_delegate(transport->AsWeakPtr());

  TestCompletionCallback callback;
  int rv = transport->Connect(callback.callback());
  rv = callback.GetResult(rv);
  ASSERT_EQ(OK, rv);

  scoped_ptr<ClientSocketHandle> socket_handle(new ClientSocketHandle);
  socket_handle->SetSocket(transport.PassAs<StreamSocket>());

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://localhost");
  request_info.load_flags = LOAD_NORMAL;
  request_info.upload_data_stream = &upload_stream;

  scoped_refptr<GrowableIOBuffer> read_buffer(new GrowableIOBuffer);
  HttpStreamParser parser(
      socket_handle.get(), &request_info, read_buffer.get(), BoundNetLog());

  HttpRequestHeaders request_headers;
  request_headers.SetHeader("Host", "localhost");
  request_headers.SetHeader("Transfer-Encoding", "chunked");
  request_headers.SetHeader("Connection", "keep-alive");

  HttpResponseInfo response_info;
  // This will attempt to Write() the initial request and headers, which will
  // complete asynchronously.
  rv = parser.SendRequest("GET /one.html HTTP/1.1\r\n", request_headers,
                          &response_info, callback.callback());
  ASSERT_EQ(ERR_IO_PENDING, rv);

  // Complete the initial request write. Additionally, this should enqueue the
  // first chunk.
  data.RunFor(1);
  ASSERT_FALSE(callback.have_result());

  // Now append another chunk (while the first write is still pending), which
  // should not confuse the state machine.
  upload_stream.AppendChunk(kChunk2, arraysize(kChunk2) - 1, false);
  ASSERT_FALSE(callback.have_result());

  // Complete writing the first chunk, which should then enqueue the second
  // chunk for writing and return, because it is set to complete
  // asynchronously.
  data.RunFor(1);
  ASSERT_FALSE(callback.have_result());

  // Complete writing the second chunk. However, because no chunks are
  // available yet, no further writes should be called until a new chunk is
  // added.
  data.RunFor(1);
  ASSERT_FALSE(callback.have_result());

  // Add the final chunk. This will enqueue another write, but it will not
  // complete due to the async nature.
  upload_stream.AppendChunk(kChunk3, arraysize(kChunk3) - 1, true);
  ASSERT_FALSE(callback.have_result());

  // Finalize writing the last chunk, which will enqueue the trailer.
  data.RunFor(1);
  ASSERT_FALSE(callback.have_result());

  // Finalize writing the trailer.
  data.RunFor(1);
  ASSERT_TRUE(callback.have_result());

  // Warning: This will hang if the callback doesn't already have a result,
  // due to the deterministic socket provider. Do not remove the above
  // ASSERT_TRUE, which will avoid this hang.
  rv = callback.WaitForResult();
  ASSERT_EQ(OK, rv);

  // Attempt to read the response status and the response headers.
  rv = parser.ReadResponseHeaders(callback.callback());
  ASSERT_EQ(ERR_IO_PENDING, rv);
  data.RunFor(2);

  ASSERT_TRUE(callback.have_result());
  rv = callback.WaitForResult();
  ASSERT_GT(rv, 0);

  // Finally, attempt to read the response body.
  scoped_refptr<IOBuffer> body_buffer(new IOBuffer(kBodySize));
  rv = parser.ReadResponseBody(
      body_buffer.get(), kBodySize, callback.callback());
  ASSERT_EQ(ERR_IO_PENDING, rv);
  data.RunFor(1);

  ASSERT_TRUE(callback.have_result());
  rv = callback.WaitForResult();
  ASSERT_EQ(kBodySize, rv);
}

TEST(HttpStreamParser, TruncatedHeaders) {
  MockRead truncated_status_reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 20"),
    MockRead(SYNCHRONOUS, 0, 2),  // EOF
  };

  MockRead truncated_after_status_reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 Ok\r\n"),
    MockRead(SYNCHRONOUS, 0, 2),  // EOF
  };

  MockRead truncated_in_header_reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 Ok\r\nHead"),
    MockRead(SYNCHRONOUS, 0, 2),  // EOF
  };

  MockRead truncated_after_header_reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 Ok\r\nHeader: foo\r\n"),
    MockRead(SYNCHRONOUS, 0, 2),  // EOF
  };

  MockRead truncated_after_final_newline_reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 Ok\r\nHeader: foo\r\n\r"),
    MockRead(SYNCHRONOUS, 0, 2),  // EOF
  };

  MockRead not_truncated_reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 200 Ok\r\nHeader: foo\r\n\r\n"),
    MockRead(SYNCHRONOUS, 0, 2),  // EOF
  };

  MockRead* reads[] = {
    truncated_status_reads,
    truncated_after_status_reads,
    truncated_in_header_reads,
    truncated_after_header_reads,
    truncated_after_final_newline_reads,
    not_truncated_reads,
  };

  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n\r\n"),
  };

  enum {
    HTTP = 0,
    HTTPS,
    NUM_PROTOCOLS,
  };

  for (size_t protocol = 0; protocol < NUM_PROTOCOLS; protocol++) {
    SCOPED_TRACE(protocol);

    for (size_t i = 0; i < arraysize(reads); i++) {
      SCOPED_TRACE(i);
      DeterministicSocketData data(reads[i], 2, writes, arraysize(writes));
      data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
      data.SetStop(3);

      scoped_ptr<DeterministicMockTCPClientSocket> transport(
          new DeterministicMockTCPClientSocket(NULL, &data));
      data.set_delegate(transport->AsWeakPtr());

      TestCompletionCallback callback;
      int rv = transport->Connect(callback.callback());
      rv = callback.GetResult(rv);
      ASSERT_EQ(OK, rv);

      scoped_ptr<ClientSocketHandle> socket_handle(new ClientSocketHandle);
      socket_handle->SetSocket(transport.PassAs<StreamSocket>());

      HttpRequestInfo request_info;
      request_info.method = "GET";
      if (protocol == HTTP) {
        request_info.url = GURL("http://localhost");
      } else {
        request_info.url = GURL("https://localhost");
      }
      request_info.load_flags = LOAD_NORMAL;

      scoped_refptr<GrowableIOBuffer> read_buffer(new GrowableIOBuffer);
      HttpStreamParser parser(
          socket_handle.get(), &request_info, read_buffer.get(), BoundNetLog());

      HttpRequestHeaders request_headers;
      HttpResponseInfo response_info;
      rv = parser.SendRequest("GET / HTTP/1.1\r\n", request_headers,
                              &response_info, callback.callback());
      ASSERT_EQ(OK, rv);

      rv = parser.ReadResponseHeaders(callback.callback());
      if (i == arraysize(reads) - 1) {
        EXPECT_EQ(OK, rv);
        EXPECT_TRUE(response_info.headers.get());
      } else {
        if (protocol == HTTP) {
          EXPECT_EQ(ERR_CONNECTION_CLOSED, rv);
          EXPECT_TRUE(response_info.headers.get());
        } else {
          EXPECT_EQ(ERR_RESPONSE_HEADERS_TRUNCATED, rv);
          EXPECT_FALSE(response_info.headers.get());
        }
      }
    }
  }
}

// Confirm that on 101 response, the headers are parsed but the data that
// follows remains in the buffer.
TEST(HttpStreamParser, Websocket101Response) {
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1,
             "HTTP/1.1 101 Switching Protocols\r\n"
             "Upgrade: websocket\r\n"
             "Connection: Upgrade\r\n"
             "\r\n"
             "a fake websocket frame"),
  };

  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET / HTTP/1.1\r\n\r\n"),
  };

  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  data.SetStop(2);

  scoped_ptr<DeterministicMockTCPClientSocket> transport(
      new DeterministicMockTCPClientSocket(NULL, &data));
  data.set_delegate(transport->AsWeakPtr());

  TestCompletionCallback callback;
  int rv = transport->Connect(callback.callback());
  rv = callback.GetResult(rv);
  ASSERT_EQ(OK, rv);

  scoped_ptr<ClientSocketHandle> socket_handle(new ClientSocketHandle);
  socket_handle->SetSocket(transport.PassAs<StreamSocket>());

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://localhost");
  request_info.load_flags = LOAD_NORMAL;

  scoped_refptr<GrowableIOBuffer> read_buffer(new GrowableIOBuffer);
  HttpStreamParser parser(
      socket_handle.get(), &request_info, read_buffer.get(), BoundNetLog());

  HttpRequestHeaders request_headers;
  HttpResponseInfo response_info;
  rv = parser.SendRequest("GET / HTTP/1.1\r\n", request_headers,
                          &response_info, callback.callback());
  ASSERT_EQ(OK, rv);

  rv = parser.ReadResponseHeaders(callback.callback());
  EXPECT_EQ(OK, rv);
  ASSERT_TRUE(response_info.headers.get());
  EXPECT_EQ(101, response_info.headers->response_code());
  EXPECT_TRUE(response_info.headers->HasHeaderValue("Connection", "Upgrade"));
  EXPECT_TRUE(response_info.headers->HasHeaderValue("Upgrade", "websocket"));
  EXPECT_EQ(read_buffer->capacity(), read_buffer->offset());
  EXPECT_EQ("a fake websocket frame",
            base::StringPiece(read_buffer->StartOfBuffer(),
                              read_buffer->capacity()));
}

// Helper class for constructing HttpStreamParser and running GET requests.
class SimpleGetRunner {
 public:
  SimpleGetRunner() : read_buffer_(new GrowableIOBuffer), sequence_number_(0) {
    writes_.push_back(MockWrite(
        SYNCHRONOUS, sequence_number_++, "GET / HTTP/1.1\r\n\r\n"));
  }

  HttpStreamParser* parser() { return parser_.get(); }
  GrowableIOBuffer* read_buffer() { return read_buffer_.get(); }
  HttpResponseInfo* response_info() { return &response_info_; }

  void AddInitialData(const std::string& data) {
    int offset = read_buffer_->offset();
    int size = data.size();
    read_buffer_->SetCapacity(offset + size);
    memcpy(read_buffer_->StartOfBuffer() + offset, data.data(), size);
    read_buffer_->set_offset(offset + size);
  }

  void AddRead(const std::string& data) {
    reads_.push_back(MockRead(SYNCHRONOUS, sequence_number_++, data.data()));
  }

  void SetupParserAndSendRequest() {
    reads_.push_back(MockRead(SYNCHRONOUS, 0, sequence_number_++));  // EOF

    socket_handle_.reset(new ClientSocketHandle);
    data_.reset(new DeterministicSocketData(
        &reads_.front(), reads_.size(), &writes_.front(), writes_.size()));
    data_->set_connect_data(MockConnect(SYNCHRONOUS, OK));
    data_->SetStop(reads_.size() + writes_.size());

    transport_.reset(new DeterministicMockTCPClientSocket(NULL, data_.get()));
    data_->set_delegate(transport_->AsWeakPtr());

    TestCompletionCallback callback;
    int rv = transport_->Connect(callback.callback());
    rv = callback.GetResult(rv);
    ASSERT_EQ(OK, rv);

    socket_handle_->SetSocket(transport_.PassAs<StreamSocket>());

    request_info_.method = "GET";
    request_info_.url = GURL("http://localhost");
    request_info_.load_flags = LOAD_NORMAL;

    parser_.reset(new HttpStreamParser(
        socket_handle_.get(), &request_info_, read_buffer(), BoundNetLog()));

    rv = parser_->SendRequest("GET / HTTP/1.1\r\n", request_headers_,
                              &response_info_, callback.callback());
    ASSERT_EQ(OK, rv);
  }

  void ReadHeaders() {
    TestCompletionCallback callback;
    EXPECT_EQ(OK, parser_->ReadResponseHeaders(callback.callback()));
  }

  void ReadBody(int user_buf_len, int* read_lengths) {
    TestCompletionCallback callback;
    scoped_refptr<IOBuffer> buffer = new IOBuffer(user_buf_len);
    int rv;
    int i = 0;
    while (true) {
      rv = parser_->ReadResponseBody(buffer, user_buf_len, callback.callback());
      EXPECT_EQ(read_lengths[i], rv);
      i++;
      if (rv <= 0)
        return;
    }
  }

 private:
  HttpRequestHeaders request_headers_;
  HttpResponseInfo response_info_;
  HttpRequestInfo request_info_;
  scoped_refptr<GrowableIOBuffer> read_buffer_;
  std::vector<MockRead> reads_;
  std::vector<MockWrite> writes_;
  scoped_ptr<ClientSocketHandle> socket_handle_;
  scoped_ptr<DeterministicSocketData> data_;
  scoped_ptr<DeterministicMockTCPClientSocket> transport_;
  scoped_ptr<HttpStreamParser> parser_;
  int sequence_number_;
};

// Test that HTTP/0.9 response size is correctly calculated.
TEST(HttpStreamParser, ReceivedBytesNoHeaders) {
  std::string response = "hello\r\nworld\r\n";

  SimpleGetRunner get_runner;
  get_runner.AddRead(response);
  get_runner.SetupParserAndSendRequest();
  get_runner.ReadHeaders();
  EXPECT_EQ(0, get_runner.parser()->received_bytes());
  int response_size = response.size();
  int read_lengths[] = {response_size, 0};
  get_runner.ReadBody(response_size, read_lengths);
  EXPECT_EQ(response_size, get_runner.parser()->received_bytes());
}

// Test basic case where there is no keep-alive or extra data from the socket,
// and the entire response is received in a single read.
TEST(HttpStreamParser, ReceivedBytesNormal) {
  std::string headers = "HTTP/1.1 200 OK\r\n"
      "Content-Length: 7\r\n\r\n";
  std::string body = "content";
  std::string response = headers + body;

  SimpleGetRunner get_runner;
  get_runner.AddRead(response);
  get_runner.SetupParserAndSendRequest();
  get_runner.ReadHeaders();
  int64 headers_size = headers.size();
  EXPECT_EQ(headers_size, get_runner.parser()->received_bytes());
  int body_size = body.size();
  int read_lengths[] = {body_size, 0};
  get_runner.ReadBody(body_size, read_lengths);
  int64 response_size = response.size();
  EXPECT_EQ(response_size, get_runner.parser()->received_bytes());
}

// Test that bytes that represent "next" response are not counted
// as current response "received_bytes".
TEST(HttpStreamParser, ReceivedBytesExcludesNextResponse) {
  std::string headers = "HTTP/1.1 200 OK\r\n"
      "Content-Length:  8\r\n\r\n";
  std::string body = "content8";
  std::string response = headers + body;
  std::string next_response = "HTTP/1.1 200 OK\r\n\r\nFOO";
  std::string data = response + next_response;

  SimpleGetRunner get_runner;
  get_runner.AddRead(data);
  get_runner.SetupParserAndSendRequest();
  get_runner.ReadHeaders();
  EXPECT_EQ(39, get_runner.parser()->received_bytes());
  int64 headers_size = headers.size();
  EXPECT_EQ(headers_size, get_runner.parser()->received_bytes());
  int body_size = body.size();
  int read_lengths[] = {body_size, 0};
  get_runner.ReadBody(body_size, read_lengths);
  int64 response_size = response.size();
  EXPECT_EQ(response_size, get_runner.parser()->received_bytes());
  int64 next_response_size = next_response.size();
  EXPECT_EQ(next_response_size, get_runner.read_buffer()->offset());
}

// Test that "received_bytes" calculation works fine when last read
// contains more data than requested by user.
// We send data in two reads:
// 1) Headers + beginning of response
// 2) remaining part of response + next response start
// We setup user read buffer so it fully accepts the beginnig of response
// body, but it is larger that remaining part of body.
TEST(HttpStreamParser, ReceivedBytesMultiReadExcludesNextResponse) {
  std::string headers = "HTTP/1.1 200 OK\r\n"
      "Content-Length: 36\r\n\r\n";
  int64 user_buf_len = 32;
  std::string body_start = std::string(user_buf_len, '#');
  int body_start_size = body_start.size();
  EXPECT_EQ(user_buf_len, body_start_size);
  std::string response_start = headers + body_start;
  std::string body_end = "abcd";
  std::string next_response = "HTTP/1.1 200 OK\r\n\r\nFOO";
  std::string response_end = body_end + next_response;

  SimpleGetRunner get_runner;
  get_runner.AddRead(response_start);
  get_runner.AddRead(response_end);
  get_runner.SetupParserAndSendRequest();
  get_runner.ReadHeaders();
  int64 headers_size = headers.size();
  EXPECT_EQ(headers_size, get_runner.parser()->received_bytes());
  int body_end_size = body_end.size();
  int read_lengths[] = {body_start_size, body_end_size, 0};
  get_runner.ReadBody(body_start_size, read_lengths);
  int64 response_size = response_start.size() + body_end_size;
  EXPECT_EQ(response_size, get_runner.parser()->received_bytes());
  int64 next_response_size = next_response.size();
  EXPECT_EQ(next_response_size, get_runner.read_buffer()->offset());
}

// Test that "received_bytes" calculation works fine when there is no
// network activity at all; that is when all data is read from read buffer.
// In this case read buffer contains two responses. We expect that only
// bytes that correspond to the first one are taken into account.
TEST(HttpStreamParser, ReceivedBytesFromReadBufExcludesNextResponse) {
  std::string headers = "HTTP/1.1 200 OK\r\n"
      "Content-Length: 7\r\n\r\n";
  std::string body = "content";
  std::string response = headers + body;
  std::string next_response = "HTTP/1.1 200 OK\r\n\r\nFOO";
  std::string data = response + next_response;

  SimpleGetRunner get_runner;
  get_runner.AddInitialData(data);
  get_runner.SetupParserAndSendRequest();
  get_runner.ReadHeaders();
  int64 headers_size = headers.size();
  EXPECT_EQ(headers_size, get_runner.parser()->received_bytes());
  int body_size = body.size();
  int read_lengths[] = {body_size, 0};
  get_runner.ReadBody(body_size, read_lengths);
  int64 response_size = response.size();
  EXPECT_EQ(response_size, get_runner.parser()->received_bytes());
  int64 next_response_size = next_response.size();
  EXPECT_EQ(next_response_size, get_runner.read_buffer()->offset());
}

// Test calculating "received_bytes" when part of request has been already
// loaded and placed to read buffer by previous stream parser.
TEST(HttpStreamParser, ReceivedBytesUseReadBuf) {
  std::string buffer = "HTTP/1.1 200 OK\r\n";
  std::string remaining_headers = "Content-Length: 7\r\n\r\n";
  int64 headers_size = buffer.size() + remaining_headers.size();
  std::string body = "content";
  std::string response = remaining_headers + body;

  SimpleGetRunner get_runner;
  get_runner.AddInitialData(buffer);
  get_runner.AddRead(response);
  get_runner.SetupParserAndSendRequest();
  get_runner.ReadHeaders();
  EXPECT_EQ(headers_size, get_runner.parser()->received_bytes());
  int body_size = body.size();
  int read_lengths[] = {body_size, 0};
  get_runner.ReadBody(body_size, read_lengths);
  EXPECT_EQ(headers_size + body_size, get_runner.parser()->received_bytes());
  EXPECT_EQ(0, get_runner.read_buffer()->offset());
}

// Test the case when the resulting read_buf contains both unused bytes and
// bytes ejected by chunked-encoding filter.
TEST(HttpStreamParser, ReceivedBytesChunkedTransferExcludesNextResponse) {
  std::string response = "HTTP/1.1 200 OK\r\n"
      "Transfer-Encoding: chunked\r\n\r\n"
      "7\r\nChunk 1\r\n"
      "8\r\nChunky 2\r\n"
      "6\r\nTest 3\r\n"
      "0\r\n\r\n";
  std::string next_response = "foo bar\r\n";
  std::string data = response + next_response;

  SimpleGetRunner get_runner;
  get_runner.AddInitialData(data);
  get_runner.SetupParserAndSendRequest();
  get_runner.ReadHeaders();
  int read_lengths[] = {4, 3, 6, 2, 6, 0};
  get_runner.ReadBody(7, read_lengths);
  int64 response_size = response.size();
  EXPECT_EQ(response_size, get_runner.parser()->received_bytes());
  int64 next_response_size = next_response.size();
  EXPECT_EQ(next_response_size, get_runner.read_buffer()->offset());
}

// Test that data transfered in multiple reads is correctly processed.
// We feed data into 4-bytes reads. Also we set length of read
// buffer to 5-bytes to test all possible buffer misaligments.
TEST(HttpStreamParser, ReceivedBytesMultipleReads) {
  std::string headers = "HTTP/1.1 200 OK\r\n"
      "Content-Length: 33\r\n\r\n";
  std::string body = "foo bar baz\r\n"
      "sputnik mir babushka";
  std::string response = headers + body;

  size_t receive_length = 4;
  std::vector<std::string> blocks;
  for (size_t i = 0; i < response.size(); i += receive_length) {
    size_t length = std::min(receive_length, response.size() - i);
    blocks.push_back(response.substr(i, length));
  }

  SimpleGetRunner get_runner;
  for (std::vector<std::string>::size_type i = 0; i < blocks.size(); ++i)
    get_runner.AddRead(blocks[i]);
  get_runner.SetupParserAndSendRequest();
  get_runner.ReadHeaders();
  int64 headers_size = headers.size();
  EXPECT_EQ(headers_size, get_runner.parser()->received_bytes());
  int read_lengths[] = {1, 4, 4, 4, 4, 4, 4, 4, 4, 0};
  get_runner.ReadBody(receive_length + 1, read_lengths);
  int64 response_size = response.size();
  EXPECT_EQ(response_size, get_runner.parser()->received_bytes());
}

// Test that "continue" HTTP header is counted as "received_bytes".
TEST(HttpStreamParser, ReceivedBytesIncludesContinueHeader) {
  std::string status100 = "HTTP/1.1 100 OK\r\n\r\n";
  std::string headers = "HTTP/1.1 200 OK\r\n"
      "Content-Length: 7\r\n\r\n";
  int64 headers_size = status100.size() + headers.size();
  std::string body = "content";
  std::string response = headers + body;

  SimpleGetRunner get_runner;
  get_runner.AddRead(status100);
  get_runner.AddRead(response);
  get_runner.SetupParserAndSendRequest();
  get_runner.ReadHeaders();
  EXPECT_EQ(100, get_runner.response_info()->headers->response_code());
  int64 status100_size = status100.size();
  EXPECT_EQ(status100_size, get_runner.parser()->received_bytes());
  get_runner.ReadHeaders();
  EXPECT_EQ(200, get_runner.response_info()->headers->response_code());
  EXPECT_EQ(headers_size, get_runner.parser()->received_bytes());
  int64 response_size = headers_size + body.size();
  int body_size = body.size();
  int read_lengths[] = {body_size, 0};
  get_runner.ReadBody(body_size, read_lengths);
  EXPECT_EQ(response_size, get_runner.parser()->received_bytes());
}

// Test that an HttpStreamParser can be read from after it's received headers
// and data structures owned by its owner have been deleted.  This happens
// when a ResponseBodyDrainer is used.
TEST(HttpStreamParser, ReadAfterUnownedObjectsDestroyed) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0,
              "GET /foo.html HTTP/1.1\r\n\r\n"),
    MockWrite(SYNCHRONOUS, 1, "1"),
  };

  const int kBodySize = 1;
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(SYNCHRONOUS, 6, "Content-Length: 1\r\n\r\n"),
    MockRead(SYNCHRONOUS, 6, "Connection: Keep-Alive\r\n\r\n"),
    MockRead(SYNCHRONOUS, 7, "1"),
    MockRead(SYNCHRONOUS, 0, 8),  // EOF
  };

  StaticSocketDataProvider data(reads, arraysize(reads), writes,
                                arraysize(writes));
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));

  scoped_ptr<MockTCPClientSocket> transport(
      new MockTCPClientSocket(AddressList(), NULL, &data));

  TestCompletionCallback callback;
  ASSERT_EQ(OK, transport->Connect(callback.callback()));

  scoped_ptr<ClientSocketHandle> socket_handle(new ClientSocketHandle);
  socket_handle->SetSocket(transport.PassAs<StreamSocket>());

  scoped_ptr<HttpRequestInfo> request_info(new HttpRequestInfo());
  request_info->method = "GET";
  request_info->url = GURL("http://somewhere/foo.html");

  scoped_refptr<GrowableIOBuffer> read_buffer(new GrowableIOBuffer);
  HttpStreamParser parser(socket_handle.get(), request_info.get(),
                          read_buffer.get(), BoundNetLog());

  scoped_ptr<HttpRequestHeaders> request_headers(new HttpRequestHeaders());
  scoped_ptr<HttpResponseInfo> response_info(new HttpResponseInfo());
  ASSERT_EQ(OK, parser.SendRequest("GET /foo.html HTTP/1.1\r\n",
            *request_headers, response_info.get(), callback.callback()));
  ASSERT_EQ(OK, parser.ReadResponseHeaders(callback.callback()));

  // If the object that owns the HttpStreamParser is deleted, it takes the
  // objects passed to the HttpStreamParser with it.
  request_info.reset();
  request_headers.reset();
  response_info.reset();

  scoped_refptr<IOBuffer> body_buffer(new IOBuffer(kBodySize));
  ASSERT_EQ(kBodySize, parser.ReadResponseBody(
      body_buffer.get(), kBodySize, callback.callback()));
}

}  // namespace

}  // namespace net
