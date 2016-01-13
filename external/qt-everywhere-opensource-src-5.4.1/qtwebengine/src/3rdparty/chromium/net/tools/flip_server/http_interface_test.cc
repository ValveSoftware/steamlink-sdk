// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/flip_server/http_interface.h"

#include <list>

#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "net/tools/balsa/balsa_enums.h"
#include "net/tools/balsa/balsa_frame.h"
#include "net/tools/balsa/balsa_headers.h"
#include "net/tools/flip_server/flip_config.h"
#include "net/tools/flip_server/flip_test_utils.h"
#include "net/tools/flip_server/mem_cache.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

using ::base::StringPiece;
using ::testing::_;
using ::testing::InSequence;

namespace {

class MockSMConnection : public SMConnection {
 public:
  MockSMConnection(EpollServer* epoll_server,
                   SSLState* ssl_state,
                   MemoryCache* memory_cache,
                   FlipAcceptor* acceptor,
                   std::string log_prefix)
      : SMConnection(epoll_server,
                     ssl_state,
                     memory_cache,
                     acceptor,
                     log_prefix) {}

  MOCK_METHOD0(Cleanup, void());
  MOCK_METHOD8(InitSMConnection,
               void(SMConnectionPoolInterface*,
                    SMInterface*,
                    EpollServer*,
                    int,
                    std::string,
                    std::string,
                    std::string,
                    bool));
};

class FlipHttpSMTest : public ::testing::Test {
 public:
  explicit FlipHttpSMTest(FlipHandlerType type = FLIP_HANDLER_PROXY) {
    SSLState* ssl_state = NULL;
    mock_another_interface_.reset(new MockSMInterface);
    memory_cache_.reset(new MemoryCache);
    acceptor_.reset(new FlipAcceptor(type,
                                     "127.0.0.1",
                                     "8941",
                                     "ssl_cert_filename",
                                     "ssl_key_filename",
                                     "127.0.0.1",
                                     "8942",
                                     "127.0.0.1",
                                     "8943",
                                     1,
                                     0,
                                     true,
                                     1,
                                     false,
                                     true,
                                     NULL));
    epoll_server_.reset(new EpollServer);
    connection_.reset(new MockSMConnection(epoll_server_.get(),
                                           ssl_state,
                                           memory_cache_.get(),
                                           acceptor_.get(),
                                           "log_prefix"));

    interface_.reset(new HttpSM(connection_.get(),
                                mock_another_interface_.get(),
                                memory_cache_.get(),
                                acceptor_.get()));
  }

  virtual void TearDown() OVERRIDE {
    if (acceptor_->listen_fd_ >= 0) {
      epoll_server_->UnregisterFD(acceptor_->listen_fd_);
      close(acceptor_->listen_fd_);
      acceptor_->listen_fd_ = -1;
    }
    STLDeleteElements(connection_->output_list());
  }

  bool HasStream(uint32 stream_id) {
    return interface_->output_ordering().ExistsInPriorityMaps(stream_id);
  }

 protected:
  scoped_ptr<MockSMInterface> mock_another_interface_;
  scoped_ptr<MemoryCache> memory_cache_;
  scoped_ptr<FlipAcceptor> acceptor_;
  scoped_ptr<EpollServer> epoll_server_;
  scoped_ptr<MockSMConnection> connection_;
  scoped_ptr<HttpSM> interface_;
};

class FlipHttpSMProxyTest : public FlipHttpSMTest {
 public:
  FlipHttpSMProxyTest() : FlipHttpSMTest(FLIP_HANDLER_PROXY) {}
  virtual ~FlipHttpSMProxyTest() {}
};

class FlipHttpSMHttpTest : public FlipHttpSMTest {
 public:
  FlipHttpSMHttpTest() : FlipHttpSMTest(FLIP_HANDLER_HTTP_SERVER) {}
  virtual ~FlipHttpSMHttpTest() {}
};

class FlipHttpSMSpdyTest : public FlipHttpSMTest {
 public:
  FlipHttpSMSpdyTest() : FlipHttpSMTest(FLIP_HANDLER_SPDY_SERVER) {}
  virtual ~FlipHttpSMSpdyTest() {}
};

TEST_F(FlipHttpSMTest, Construct) {
  ASSERT_FALSE(interface_->spdy_framer()->is_request());
}

TEST_F(FlipHttpSMTest, AddToOutputOrder) {
  uint32 stream_id = 13;
  MemCacheIter mci;
  mci.stream_id = stream_id;

  {
    BalsaHeaders headers;
    std::string filename = "foobar";
    memory_cache_->InsertFile(&headers, filename, "");
    mci.file_data = memory_cache_->GetFileData(filename);
  }

  interface_->AddToOutputOrder(mci);
  ASSERT_TRUE(HasStream(stream_id));
}

TEST_F(FlipHttpSMTest, InitSMInterface) {
  scoped_ptr<MockSMInterface> mock(new MockSMInterface);
  {
    InSequence s;
    EXPECT_CALL(*mock_another_interface_, SendEOF(_));
    EXPECT_CALL(*mock_another_interface_, ResetForNewInterface(_));
    EXPECT_CALL(*mock, SendEOF(_));
    EXPECT_CALL(*mock, ResetForNewInterface(_));
  }

  interface_->ResetForNewConnection();
  interface_->InitSMInterface(mock.get(), 0);
  interface_->ResetForNewConnection();
}

TEST_F(FlipHttpSMTest, InitSMConnection) {
  EXPECT_CALL(*connection_, InitSMConnection(_, _, _, _, _, _, _, _));

  interface_->InitSMConnection(NULL, NULL, NULL, 0, "", "", "", false);
}

TEST_F(FlipHttpSMTest, ProcessReadInput) {
  std::string data =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 14\r\n\r\n"
      "hello, world\r\n";
  testing::MockFunction<void(int)> checkpoint;  // NOLINT
  {
    InSequence s;
    EXPECT_CALL(*mock_another_interface_, SendSynReply(_, _));
    EXPECT_CALL(checkpoint, Call(0));
    EXPECT_CALL(*mock_another_interface_, SendDataFrame(_, _, _, _, _));
    EXPECT_CALL(*mock_another_interface_, SendEOF(_));
  }

  ASSERT_EQ(BalsaFrameEnums::READING_HEADER_AND_FIRSTLINE,
            interface_->spdy_framer()->ParseState());

  size_t read = interface_->ProcessReadInput(data.data(), data.size());
  ASSERT_EQ(39u, read);
  checkpoint.Call(0);
  read += interface_->ProcessReadInput(&data.data()[read], data.size() - read);
  ASSERT_EQ(data.size(), read);
  ASSERT_EQ(BalsaFrameEnums::MESSAGE_FULLY_READ,
            interface_->spdy_framer()->ParseState());
  ASSERT_TRUE(interface_->MessageFullyRead());
}

TEST_F(FlipHttpSMTest, ProcessWriteInput) {
  std::string data = "hello, world";
  interface_->ProcessWriteInput(data.data(), data.size());

  ASSERT_EQ(1u, connection_->output_list()->size());
  std::list<DataFrame*>::const_iterator i = connection_->output_list()->begin();
  DataFrame* df = *i++;
  ASSERT_EQ(data, StringPiece(df->data, df->size));
  ASSERT_EQ(connection_->output_list()->end(), i);
}

TEST_F(FlipHttpSMTest, Reset) {
  std::string data = "HTTP/1.1 200 OK\r\n\r\n";
  testing::MockFunction<void(int)> checkpoint;  // NOLINT
  {
    InSequence s;
    EXPECT_CALL(*mock_another_interface_, SendSynReply(_, _));
    EXPECT_CALL(checkpoint, Call(0));
  }

  ASSERT_EQ(BalsaFrameEnums::READING_HEADER_AND_FIRSTLINE,
            interface_->spdy_framer()->ParseState());

  interface_->ProcessReadInput(data.data(), data.size());
  checkpoint.Call(0);
  ASSERT_FALSE(interface_->MessageFullyRead());
  ASSERT_EQ(BalsaFrameEnums::READING_UNTIL_CLOSE,
            interface_->spdy_framer()->ParseState());

  interface_->Reset();
  ASSERT_EQ(BalsaFrameEnums::READING_HEADER_AND_FIRSTLINE,
            interface_->spdy_framer()->ParseState());
}

TEST_F(FlipHttpSMTest, ResetForNewConnection) {
  std::string data = "HTTP/1.1 200 OK\r\n\r\n";
  testing::MockFunction<void(int)> checkpoint;  // NOLINT
  {
    InSequence s;
    EXPECT_CALL(*mock_another_interface_, SendSynReply(_, _));
    EXPECT_CALL(checkpoint, Call(0));
    EXPECT_CALL(*mock_another_interface_, SendEOF(_));
    EXPECT_CALL(*mock_another_interface_, ResetForNewInterface(_));
  }

  ASSERT_EQ(BalsaFrameEnums::READING_HEADER_AND_FIRSTLINE,
            interface_->spdy_framer()->ParseState());

  interface_->ProcessReadInput(data.data(), data.size());
  checkpoint.Call(0);
  ASSERT_FALSE(interface_->MessageFullyRead());
  ASSERT_EQ(BalsaFrameEnums::READING_UNTIL_CLOSE,
            interface_->spdy_framer()->ParseState());

  interface_->ResetForNewConnection();
  ASSERT_EQ(BalsaFrameEnums::READING_HEADER_AND_FIRSTLINE,
            interface_->spdy_framer()->ParseState());
}

TEST_F(FlipHttpSMTest, NewStream) {
  uint32 stream_id = 4;
  {
    BalsaHeaders headers;
    std::string filename = "foobar";
    memory_cache_->InsertFile(&headers, filename, "");
  }

  interface_->NewStream(stream_id, 1, "foobar");
  ASSERT_TRUE(HasStream(stream_id));
}

TEST_F(FlipHttpSMTest, NewStreamError) {
  std::string syn_reply =
      "HTTP/1.1 404 Not Found\r\n"
      "transfer-encoding: chunked\r\n\r\n";
  std::string body = "e\r\npage not found\r\n";
  uint32 stream_id = 4;

  ASSERT_FALSE(HasStream(stream_id));
  interface_->NewStream(stream_id, 1, "foobar");

  ASSERT_EQ(3u, connection_->output_list()->size());
  std::list<DataFrame*>::const_iterator i = connection_->output_list()->begin();
  DataFrame* df = *i++;
  ASSERT_EQ(syn_reply, StringPiece(df->data, df->size));
  df = *i++;
  ASSERT_EQ(body, StringPiece(df->data, df->size));
  df = *i++;
  ASSERT_EQ("0\r\n\r\n", StringPiece(df->data, df->size));
  ASSERT_FALSE(HasStream(stream_id));
}

TEST_F(FlipHttpSMTest, SendErrorNotFound) {
  std::string syn_reply =
      "HTTP/1.1 404 Not Found\r\n"
      "transfer-encoding: chunked\r\n\r\n";
  std::string body = "e\r\npage not found\r\n";
  uint32 stream_id = 13;
  MemCacheIter mci;
  mci.stream_id = stream_id;

  {
    BalsaHeaders headers;
    std::string filename = "foobar";
    memory_cache_->InsertFile(&headers, filename, "");
    mci.file_data = memory_cache_->GetFileData(filename);
  }

  interface_->AddToOutputOrder(mci);
  ASSERT_TRUE(HasStream(stream_id));
  interface_->SendErrorNotFound(stream_id);

  ASSERT_EQ(3u, connection_->output_list()->size());
  std::list<DataFrame*>::const_iterator i = connection_->output_list()->begin();
  DataFrame* df = *i++;
  ASSERT_EQ(syn_reply, StringPiece(df->data, df->size));
  df = *i++;
  ASSERT_EQ(body, StringPiece(df->data, df->size));
  df = *i++;
  ASSERT_EQ("0\r\n\r\n", StringPiece(df->data, df->size));
  ASSERT_FALSE(HasStream(stream_id));
}

TEST_F(FlipHttpSMTest, SendSynStream) {
  std::string expected =
      "GET / HTTP/1.0\r\n"
      "key1: value1\r\n\r\n";
  BalsaHeaders headers;
  headers.SetResponseFirstlineFromStringPieces("GET", "/path", "HTTP/1.0");
  headers.AppendHeader("key1", "value1");
  interface_->SendSynStream(18, headers);

  // TODO(yhirano): Is this behavior correct?
  ASSERT_EQ(0u, connection_->output_list()->size());
}

TEST_F(FlipHttpSMTest, SendSynReply) {
  std::string expected =
      "HTTP/1.1 200 OK\r\n"
      "key1: value1\r\n\r\n";
  BalsaHeaders headers;
  headers.SetResponseFirstlineFromStringPieces("HTTP/1.1", "200", "OK");
  headers.AppendHeader("key1", "value1");
  interface_->SendSynReply(18, headers);

  ASSERT_EQ(1u, connection_->output_list()->size());
  DataFrame* df = connection_->output_list()->front();
  ASSERT_EQ(expected, StringPiece(df->data, df->size));
}

TEST_F(FlipHttpSMTest, SendDataFrame) {
  std::string data = "foo bar baz";
  interface_->SendDataFrame(12, data.data(), data.size(), 0, false);

  ASSERT_EQ(1u, connection_->output_list()->size());
  DataFrame* df = connection_->output_list()->front();
  ASSERT_EQ("b\r\nfoo bar baz\r\n", StringPiece(df->data, df->size));
}

TEST_F(FlipHttpSMProxyTest, ProcessBodyData) {
  BalsaVisitorInterface* visitor = interface_.get();
  std::string data = "hello, world";
  {
    InSequence s;
    EXPECT_CALL(*mock_another_interface_,
                SendDataFrame(0, data.data(), data.size(), 0, false));
  }
  visitor->ProcessBodyData(data.data(), data.size());
}

// --
// FlipHttpSMProxyTest

TEST_F(FlipHttpSMProxyTest, ProcessHeaders) {
  BalsaVisitorInterface* visitor = interface_.get();
  {
    InSequence s;
    EXPECT_CALL(*mock_another_interface_, SendSynReply(0, _));
  }
  BalsaHeaders headers;
  visitor->ProcessHeaders(headers);
}

TEST_F(FlipHttpSMProxyTest, MessageDone) {
  BalsaVisitorInterface* visitor = interface_.get();
  {
    InSequence s;
    EXPECT_CALL(*mock_another_interface_, SendEOF(0));
  }
  visitor->MessageDone();
}

TEST_F(FlipHttpSMProxyTest, Cleanup) {
  EXPECT_CALL(*connection_, Cleanup()).Times(0);
  interface_->Cleanup();
}

TEST_F(FlipHttpSMProxyTest, SendEOF) {
  {
    InSequence s;
    EXPECT_CALL(*mock_another_interface_, ResetForNewInterface(_));
  }
  interface_->SendEOF(32);
  ASSERT_EQ(1u, connection_->output_list()->size());
  DataFrame* df = connection_->output_list()->front();
  ASSERT_EQ("0\r\n\r\n", StringPiece(df->data, df->size));
}

// --
// FlipHttpSMHttpTest

TEST_F(FlipHttpSMHttpTest, ProcessHeaders) {
  BalsaVisitorInterface* visitor = interface_.get();
  {
    BalsaHeaders headers;
    std::string filename = "GET_/path/file";
    memory_cache_->InsertFile(&headers, filename, "");
  }

  BalsaHeaders headers;
  headers.AppendHeader("Host", "example.com");
  headers.SetRequestFirstlineFromStringPieces("GET", "/path/file", "HTTP/1.0");
  uint32 stream_id = 133;
  interface_->SetStreamID(stream_id);
  ASSERT_FALSE(HasStream(stream_id));
  visitor->ProcessHeaders(headers);
  ASSERT_TRUE(HasStream(stream_id));
}

TEST_F(FlipHttpSMHttpTest, MessageDone) {
  BalsaVisitorInterface* visitor = interface_.get();
  {
    InSequence s;
    EXPECT_CALL(*mock_another_interface_, SendEOF(0)).Times(0);
  }
  visitor->MessageDone();
}

TEST_F(FlipHttpSMHttpTest, Cleanup) {
  EXPECT_CALL(*connection_, Cleanup()).Times(0);
  interface_->Cleanup();
}

TEST_F(FlipHttpSMHttpTest, SendEOF) {
  {
    InSequence s;
    EXPECT_CALL(*mock_another_interface_, ResetForNewInterface(_)).Times(0);
  }
  interface_->SendEOF(32);
  ASSERT_EQ(1u, connection_->output_list()->size());
  DataFrame* df = connection_->output_list()->front();
  ASSERT_EQ("0\r\n\r\n", StringPiece(df->data, df->size));
}

// --
// FlipHttpSMSpdyTest

TEST_F(FlipHttpSMSpdyTest, ProcessHeaders) {
  BalsaVisitorInterface* visitor = interface_.get();
  {
    InSequence s;
    EXPECT_CALL(*mock_another_interface_, SendSynReply(0, _));
  }
  BalsaHeaders headers;
  visitor->ProcessHeaders(headers);
}

TEST_F(FlipHttpSMSpdyTest, MessageDone) {
  BalsaVisitorInterface* visitor = interface_.get();
  {
    InSequence s;
    EXPECT_CALL(*mock_another_interface_, SendEOF(0)).Times(0);
  }
  visitor->MessageDone();
}

TEST_F(FlipHttpSMSpdyTest, Cleanup) {
  EXPECT_CALL(*connection_, Cleanup()).Times(0);
  interface_->Cleanup();
}

TEST_F(FlipHttpSMSpdyTest, SendEOF) {
  {
    InSequence s;
    EXPECT_CALL(*mock_another_interface_, ResetForNewInterface(_)).Times(0);
  }
  interface_->SendEOF(32);
  ASSERT_EQ(1u, connection_->output_list()->size());
  DataFrame* df = connection_->output_list()->front();
  ASSERT_EQ("0\r\n\r\n", StringPiece(df->data, df->size));
}

}  // namespace

}  // namespace net
