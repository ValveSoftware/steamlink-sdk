// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_session_pool.h"

#include <cstddef>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/dns/host_cache.h"
#include "net/http/http_network_session.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_stream_test_util.h"
#include "net/spdy/spdy_test_util_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class SpdySessionPoolTest : public ::testing::Test,
                            public ::testing::WithParamInterface<NextProto> {
 protected:
  // Used by RunIPPoolingTest().
  enum SpdyPoolCloseSessionsType {
    SPDY_POOL_CLOSE_SESSIONS_MANUALLY,
    SPDY_POOL_CLOSE_CURRENT_SESSIONS,
    SPDY_POOL_CLOSE_IDLE_SESSIONS,
  };

  SpdySessionPoolTest()
      : session_deps_(GetParam()),
        spdy_session_pool_(NULL) {}

  void CreateNetworkSession() {
    http_session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);
    spdy_session_pool_ = http_session_->spdy_session_pool();
  }

  void RunIPPoolingTest(SpdyPoolCloseSessionsType close_sessions_type);

  SpdySessionDependencies session_deps_;
  scoped_refptr<HttpNetworkSession> http_session_;
  SpdySessionPool* spdy_session_pool_;
};

INSTANTIATE_TEST_CASE_P(
    NextProto,
    SpdySessionPoolTest,
    testing::Values(kProtoDeprecatedSPDY2,
                    kProtoSPDY3, kProtoSPDY31, kProtoSPDY4));

// A delegate that opens a new session when it is closed.
class SessionOpeningDelegate : public SpdyStream::Delegate {
 public:
  SessionOpeningDelegate(SpdySessionPool* spdy_session_pool,
                         const SpdySessionKey& key)
      : spdy_session_pool_(spdy_session_pool),
        key_(key) {}

  virtual ~SessionOpeningDelegate() {}

  virtual void OnRequestHeadersSent() OVERRIDE {}

  virtual SpdyResponseHeadersStatus OnResponseHeadersUpdated(
      const SpdyHeaderBlock& response_headers) OVERRIDE {
    return RESPONSE_HEADERS_ARE_COMPLETE;
  }

  virtual void OnDataReceived(scoped_ptr<SpdyBuffer> buffer) OVERRIDE {}

  virtual void OnDataSent() OVERRIDE {}

  virtual void OnClose(int status) OVERRIDE {
    ignore_result(CreateFakeSpdySession(spdy_session_pool_, key_));
  }

 private:
  SpdySessionPool* const spdy_session_pool_;
  const SpdySessionKey key_;
};

// Set up a SpdyStream to create a new session when it is closed.
// CloseCurrentSessions should not close the newly-created session.
TEST_P(SpdySessionPoolTest, CloseCurrentSessions) {
  const char kTestHost[] = "www.foo.com";
  const int kTestPort = 80;

  session_deps_.host_resolver->set_synchronous_mode(true);

  HostPortPair test_host_port_pair(kTestHost, kTestPort);
  SpdySessionKey test_key =
      SpdySessionKey(
          test_host_port_pair, ProxyServer::Direct(),
          PRIVACY_MODE_DISABLED);

  MockConnect connect_data(SYNCHRONOUS, OK);
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };

  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateNetworkSession();

  // Setup the first session to the first host.
  base::WeakPtr<SpdySession> session =
      CreateInsecureSpdySession(http_session_, test_key, BoundNetLog());

  // Flush the SpdySession::OnReadComplete() task.
  base::MessageLoop::current()->RunUntilIdle();

  // Verify that we have sessions for everything.
  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, test_key));

  // Set the stream to create a new session when it is closed.
  base::WeakPtr<SpdyStream> spdy_stream =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                session, GURL("http://www.foo.com"),
                                MEDIUM, BoundNetLog());
  SessionOpeningDelegate delegate(spdy_session_pool_, test_key);
  spdy_stream->SetDelegate(&delegate);

  // Close the current session.
  spdy_session_pool_->CloseCurrentSessions(net::ERR_ABORTED);

  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, test_key));
}

TEST_P(SpdySessionPoolTest, CloseCurrentIdleSessions) {
  MockConnect connect_data(SYNCHRONOUS, OK);
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };

  session_deps_.host_resolver->set_synchronous_mode(true);

  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateNetworkSession();

  // Set up session 1
  const std::string kTestHost1("http://www.a.com");
  HostPortPair test_host_port_pair1(kTestHost1, 80);
  SpdySessionKey key1(test_host_port_pair1, ProxyServer::Direct(),
                      PRIVACY_MODE_DISABLED);
  base::WeakPtr<SpdySession> session1 =
      CreateInsecureSpdySession(http_session_, key1, BoundNetLog());
  GURL url1(kTestHost1);
  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                session1, url1, MEDIUM, BoundNetLog());
  ASSERT_TRUE(spdy_stream1.get() != NULL);

  // Set up session 2
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  const std::string kTestHost2("http://www.b.com");
  HostPortPair test_host_port_pair2(kTestHost2, 80);
  SpdySessionKey key2(test_host_port_pair2, ProxyServer::Direct(),
                      PRIVACY_MODE_DISABLED);
  base::WeakPtr<SpdySession> session2 =
      CreateInsecureSpdySession(http_session_, key2, BoundNetLog());
  GURL url2(kTestHost2);
  base::WeakPtr<SpdyStream> spdy_stream2 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                session2, url2, MEDIUM, BoundNetLog());
  ASSERT_TRUE(spdy_stream2.get() != NULL);

  // Set up session 3
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  const std::string kTestHost3("http://www.c.com");
  HostPortPair test_host_port_pair3(kTestHost3, 80);
  SpdySessionKey key3(test_host_port_pair3, ProxyServer::Direct(),
                      PRIVACY_MODE_DISABLED);
  base::WeakPtr<SpdySession> session3 =
      CreateInsecureSpdySession(http_session_, key3, BoundNetLog());
  GURL url3(kTestHost3);
  base::WeakPtr<SpdyStream> spdy_stream3 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                session3, url3, MEDIUM, BoundNetLog());
  ASSERT_TRUE(spdy_stream3.get() != NULL);

  // All sessions are active and not closed
  EXPECT_TRUE(session1->is_active());
  EXPECT_TRUE(session1->IsAvailable());
  EXPECT_TRUE(session2->is_active());
  EXPECT_TRUE(session2->IsAvailable());
  EXPECT_TRUE(session3->is_active());
  EXPECT_TRUE(session3->IsAvailable());

  // Should not do anything, all are active
  spdy_session_pool_->CloseCurrentIdleSessions();
  EXPECT_TRUE(session1->is_active());
  EXPECT_TRUE(session1->IsAvailable());
  EXPECT_TRUE(session2->is_active());
  EXPECT_TRUE(session2->IsAvailable());
  EXPECT_TRUE(session3->is_active());
  EXPECT_TRUE(session3->IsAvailable());

  // Make sessions 1 and 3 inactive, but keep them open.
  // Session 2 still open and active
  session1->CloseCreatedStream(spdy_stream1, OK);
  EXPECT_EQ(NULL, spdy_stream1.get());
  session3->CloseCreatedStream(spdy_stream3, OK);
  EXPECT_EQ(NULL, spdy_stream3.get());
  EXPECT_FALSE(session1->is_active());
  EXPECT_TRUE(session1->IsAvailable());
  EXPECT_TRUE(session2->is_active());
  EXPECT_TRUE(session2->IsAvailable());
  EXPECT_FALSE(session3->is_active());
  EXPECT_TRUE(session3->IsAvailable());

  // Should close session 1 and 3, 2 should be left open
  spdy_session_pool_->CloseCurrentIdleSessions();
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_TRUE(session1 == NULL);
  EXPECT_TRUE(session2->is_active());
  EXPECT_TRUE(session2->IsAvailable());
  EXPECT_TRUE(session3 == NULL);

  // Should not do anything
  spdy_session_pool_->CloseCurrentIdleSessions();
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_TRUE(session2->is_active());
  EXPECT_TRUE(session2->IsAvailable());

  // Make 2 not active
  session2->CloseCreatedStream(spdy_stream2, OK);
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(NULL, spdy_stream2.get());
  EXPECT_FALSE(session2->is_active());
  EXPECT_TRUE(session2->IsAvailable());

  // This should close session 2
  spdy_session_pool_->CloseCurrentIdleSessions();
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_TRUE(session2 == NULL);
}

// Set up a SpdyStream to create a new session when it is closed.
// CloseAllSessions should close the newly-created session.
TEST_P(SpdySessionPoolTest, CloseAllSessions) {
  const char kTestHost[] = "www.foo.com";
  const int kTestPort = 80;

  session_deps_.host_resolver->set_synchronous_mode(true);

  HostPortPair test_host_port_pair(kTestHost, kTestPort);
  SpdySessionKey test_key =
      SpdySessionKey(
          test_host_port_pair, ProxyServer::Direct(),
          PRIVACY_MODE_DISABLED);

  MockConnect connect_data(SYNCHRONOUS, OK);
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };

  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateNetworkSession();

  // Setup the first session to the first host.
  base::WeakPtr<SpdySession> session =
      CreateInsecureSpdySession(http_session_, test_key, BoundNetLog());

  // Flush the SpdySession::OnReadComplete() task.
  base::MessageLoop::current()->RunUntilIdle();

  // Verify that we have sessions for everything.
  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, test_key));

  // Set the stream to create a new session when it is closed.
  base::WeakPtr<SpdyStream> spdy_stream =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                session, GURL("http://www.foo.com"),
                                MEDIUM, BoundNetLog());
  SessionOpeningDelegate delegate(spdy_session_pool_, test_key);
  spdy_stream->SetDelegate(&delegate);

  // Close the current session.
  spdy_session_pool_->CloseAllSessions();

  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, test_key));
}

// This test has three variants, one for each style of closing the connection.
// If |clean_via_close_current_sessions| is SPDY_POOL_CLOSE_SESSIONS_MANUALLY,
// the sessions are closed manually, calling SpdySessionPool::Remove() directly.
// If |clean_via_close_current_sessions| is SPDY_POOL_CLOSE_CURRENT_SESSIONS,
// sessions are closed with SpdySessionPool::CloseCurrentSessions().
// If |clean_via_close_current_sessions| is SPDY_POOL_CLOSE_IDLE_SESSIONS,
// sessions are closed with SpdySessionPool::CloseIdleSessions().
void SpdySessionPoolTest::RunIPPoolingTest(
    SpdyPoolCloseSessionsType close_sessions_type) {
  const int kTestPort = 80;
  struct TestHosts {
    std::string url;
    std::string name;
    std::string iplist;
    SpdySessionKey key;
    AddressList addresses;
  } test_hosts[] = {
    { "http:://www.foo.com",
      "www.foo.com",
      "192.0.2.33,192.168.0.1,192.168.0.5"
    },
    { "http://js.foo.com",
      "js.foo.com",
      "192.168.0.2,192.168.0.3,192.168.0.5,192.0.2.33"
    },
    { "http://images.foo.com",
      "images.foo.com",
      "192.168.0.4,192.168.0.3"
    },
  };

  session_deps_.host_resolver->set_synchronous_mode(true);
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_hosts); i++) {
    session_deps_.host_resolver->rules()->AddIPLiteralRule(
        test_hosts[i].name, test_hosts[i].iplist, std::string());

    // This test requires that the HostResolver cache be populated.  Normal
    // code would have done this already, but we do it manually.
    HostResolver::RequestInfo info(HostPortPair(test_hosts[i].name, kTestPort));
    session_deps_.host_resolver->Resolve(info,
                                         DEFAULT_PRIORITY,
                                         &test_hosts[i].addresses,
                                         CompletionCallback(),
                                         NULL,
                                         BoundNetLog());

    // Setup a SpdySessionKey
    test_hosts[i].key = SpdySessionKey(
        HostPortPair(test_hosts[i].name, kTestPort), ProxyServer::Direct(),
        PRIVACY_MODE_DISABLED);
  }

  MockConnect connect_data(SYNCHRONOUS, OK);
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };

  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateNetworkSession();

  // Setup the first session to the first host.
  base::WeakPtr<SpdySession> session =
      CreateInsecureSpdySession(
          http_session_, test_hosts[0].key, BoundNetLog());

  // Flush the SpdySession::OnReadComplete() task.
  base::MessageLoop::current()->RunUntilIdle();

  // The third host has no overlap with the first, so it can't pool IPs.
  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, test_hosts[2].key));

  // The second host overlaps with the first, and should IP pool.
  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, test_hosts[1].key));

  // Verify that the second host, through a proxy, won't share the IP.
  SpdySessionKey proxy_key(test_hosts[1].key.host_port_pair(),
      ProxyServer::FromPacString("HTTP http://proxy.foo.com/"),
      PRIVACY_MODE_DISABLED);
  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, proxy_key));

  // Overlap between 2 and 3 does is not transitive to 1.
  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, test_hosts[2].key));

  // Create a new session to host 2.
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  base::WeakPtr<SpdySession> session2 =
      CreateInsecureSpdySession(
          http_session_, test_hosts[2].key, BoundNetLog());

  // Verify that we have sessions for everything.
  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, test_hosts[0].key));
  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, test_hosts[1].key));
  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, test_hosts[2].key));

  // Grab the session to host 1 and verify that it is the same session
  // we got with host 0, and that is a different from host 2's session.
  base::WeakPtr<SpdySession> session1 =
      spdy_session_pool_->FindAvailableSession(
          test_hosts[1].key, BoundNetLog());
  EXPECT_EQ(session.get(), session1.get());
  EXPECT_NE(session2.get(), session1.get());

  // Remove the aliases and observe that we still have a session for host1.
  SpdySessionPoolPeer pool_peer(spdy_session_pool_);
  pool_peer.RemoveAliases(test_hosts[0].key);
  pool_peer.RemoveAliases(test_hosts[1].key);
  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, test_hosts[1].key));

  // Expire the host cache
  session_deps_.host_resolver->GetHostCache()->clear();
  EXPECT_TRUE(HasSpdySession(spdy_session_pool_, test_hosts[1].key));

  // Cleanup the sessions.
  switch (close_sessions_type) {
    case SPDY_POOL_CLOSE_SESSIONS_MANUALLY:
      session->CloseSessionOnError(ERR_ABORTED, std::string());
      session2->CloseSessionOnError(ERR_ABORTED, std::string());
      base::MessageLoop::current()->RunUntilIdle();
      EXPECT_TRUE(session == NULL);
      EXPECT_TRUE(session2 == NULL);
      break;
    case SPDY_POOL_CLOSE_CURRENT_SESSIONS:
      spdy_session_pool_->CloseCurrentSessions(ERR_ABORTED);
      break;
    case SPDY_POOL_CLOSE_IDLE_SESSIONS:
      GURL url(test_hosts[0].url);
      base::WeakPtr<SpdyStream> spdy_stream =
          CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                    session, url, MEDIUM, BoundNetLog());
      GURL url1(test_hosts[1].url);
      base::WeakPtr<SpdyStream> spdy_stream1 =
          CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                    session1, url1, MEDIUM, BoundNetLog());
      GURL url2(test_hosts[2].url);
      base::WeakPtr<SpdyStream> spdy_stream2 =
          CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                    session2, url2, MEDIUM, BoundNetLog());

      // Close streams to make spdy_session and spdy_session1 inactive.
      session->CloseCreatedStream(spdy_stream, OK);
      EXPECT_EQ(NULL, spdy_stream.get());
      session1->CloseCreatedStream(spdy_stream1, OK);
      EXPECT_EQ(NULL, spdy_stream1.get());

      // Check spdy_session and spdy_session1 are not closed.
      EXPECT_FALSE(session->is_active());
      EXPECT_TRUE(session->IsAvailable());
      EXPECT_FALSE(session1->is_active());
      EXPECT_TRUE(session1->IsAvailable());
      EXPECT_TRUE(session2->is_active());
      EXPECT_TRUE(session2->IsAvailable());

      // Test that calling CloseIdleSessions, does not cause a crash.
      // http://crbug.com/181400
      spdy_session_pool_->CloseCurrentIdleSessions();
      base::MessageLoop::current()->RunUntilIdle();

      // Verify spdy_session and spdy_session1 are closed.
      EXPECT_TRUE(session == NULL);
      EXPECT_TRUE(session1 == NULL);
      EXPECT_TRUE(session2->is_active());
      EXPECT_TRUE(session2->IsAvailable());

      spdy_stream2->Cancel();
      EXPECT_EQ(NULL, spdy_stream.get());
      EXPECT_EQ(NULL, spdy_stream1.get());
      EXPECT_EQ(NULL, spdy_stream2.get());

      session2->CloseSessionOnError(ERR_ABORTED, std::string());
      base::MessageLoop::current()->RunUntilIdle();
      EXPECT_TRUE(session2 == NULL);
      break;
  }

  // Verify that the map is all cleaned up.
  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, test_hosts[0].key));
  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, test_hosts[1].key));
  EXPECT_FALSE(HasSpdySession(spdy_session_pool_, test_hosts[2].key));
}

TEST_P(SpdySessionPoolTest, IPPooling) {
  RunIPPoolingTest(SPDY_POOL_CLOSE_SESSIONS_MANUALLY);
}

TEST_P(SpdySessionPoolTest, IPPoolingCloseCurrentSessions) {
  RunIPPoolingTest(SPDY_POOL_CLOSE_CURRENT_SESSIONS);
}

TEST_P(SpdySessionPoolTest, IPPoolingCloseIdleSessions) {
  RunIPPoolingTest(SPDY_POOL_CLOSE_IDLE_SESSIONS);
}

// Construct a Pool with SpdySessions in various availability states. Simulate
// an IP address change. Ensure sessions gracefully shut down. Regression test
// for crbug.com/379469.
TEST_P(SpdySessionPoolTest, IPAddressChanged) {
  MockConnect connect_data(SYNCHRONOUS, OK);
  session_deps_.host_resolver->set_synchronous_mode(true);
  SpdyTestUtil spdy_util(GetParam());

  MockRead reads[] = {
      MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };
  scoped_ptr<SpdyFrame> req(
      spdy_util.ConstructSpdyGet("http://www.a.com", false, 1, MEDIUM));
  MockWrite writes[] = {CreateMockWrite(*req, 1)};

  DelayedSocketData data(1, reads, arraysize(reads), writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateNetworkSession();

  // Set up session A: Going away, but with an active stream.
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  const std::string kTestHostA("http://www.a.com");
  HostPortPair test_host_port_pairA(kTestHostA, 80);
  SpdySessionKey keyA(
      test_host_port_pairA, ProxyServer::Direct(), PRIVACY_MODE_DISABLED);
  base::WeakPtr<SpdySession> sessionA =
      CreateInsecureSpdySession(http_session_, keyA, BoundNetLog());

  GURL urlA(kTestHostA);
  base::WeakPtr<SpdyStream> spdy_streamA = CreateStreamSynchronously(
      SPDY_BIDIRECTIONAL_STREAM, sessionA, urlA, MEDIUM, BoundNetLog());
  test::StreamDelegateDoNothing delegateA(spdy_streamA);
  spdy_streamA->SetDelegate(&delegateA);

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util.ConstructGetHeaderBlock(urlA.spec()));
  spdy_streamA->SendRequestHeaders(headers.Pass(), NO_MORE_DATA_TO_SEND);
  EXPECT_TRUE(spdy_streamA->HasUrlFromHeaders());

  base::MessageLoop::current()->RunUntilIdle();  // Allow headers to write.
  EXPECT_TRUE(delegateA.send_headers_completed());

  sessionA->MakeUnavailable();
  EXPECT_TRUE(sessionA->IsGoingAway());
  EXPECT_FALSE(delegateA.StreamIsClosed());

  // Set up session B: Available, with a created stream.
  const std::string kTestHostB("http://www.b.com");
  HostPortPair test_host_port_pairB(kTestHostB, 80);
  SpdySessionKey keyB(
      test_host_port_pairB, ProxyServer::Direct(), PRIVACY_MODE_DISABLED);
  base::WeakPtr<SpdySession> sessionB =
      CreateInsecureSpdySession(http_session_, keyB, BoundNetLog());
  EXPECT_TRUE(sessionB->IsAvailable());

  GURL urlB(kTestHostB);
  base::WeakPtr<SpdyStream> spdy_streamB = CreateStreamSynchronously(
      SPDY_BIDIRECTIONAL_STREAM, sessionB, urlB, MEDIUM, BoundNetLog());
  test::StreamDelegateDoNothing delegateB(spdy_streamB);
  spdy_streamB->SetDelegate(&delegateB);

  // Set up session C: Draining.
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  const std::string kTestHostC("http://www.c.com");
  HostPortPair test_host_port_pairC(kTestHostC, 80);
  SpdySessionKey keyC(
      test_host_port_pairC, ProxyServer::Direct(), PRIVACY_MODE_DISABLED);
  base::WeakPtr<SpdySession> sessionC =
      CreateInsecureSpdySession(http_session_, keyC, BoundNetLog());

  sessionC->CloseSessionOnError(ERR_SPDY_PROTOCOL_ERROR, "Error!");
  EXPECT_TRUE(sessionC->IsDraining());

  spdy_session_pool_->OnIPAddressChanged();

#if defined(OS_ANDROID) || defined(OS_WIN) || defined(OS_IOS)
  EXPECT_TRUE(sessionA->IsGoingAway());
  EXPECT_TRUE(sessionB->IsDraining());
  EXPECT_TRUE(sessionC->IsDraining());

  EXPECT_EQ(1u,
            sessionA->num_active_streams());  // Active stream is still active.
  EXPECT_FALSE(delegateA.StreamIsClosed());

  EXPECT_TRUE(delegateB.StreamIsClosed());  // Created stream was closed.
  EXPECT_EQ(ERR_NETWORK_CHANGED, delegateB.WaitForClose());

  sessionA->CloseSessionOnError(ERR_ABORTED, "Closing");
  sessionB->CloseSessionOnError(ERR_ABORTED, "Closing");

  EXPECT_TRUE(delegateA.StreamIsClosed());
  EXPECT_EQ(ERR_ABORTED, delegateA.WaitForClose());
#else
  EXPECT_TRUE(sessionA->IsDraining());
  EXPECT_TRUE(sessionB->IsDraining());
  EXPECT_TRUE(sessionC->IsDraining());

  // Both streams were closed with an error.
  EXPECT_TRUE(delegateA.StreamIsClosed());
  EXPECT_EQ(ERR_NETWORK_CHANGED, delegateA.WaitForClose());
  EXPECT_TRUE(delegateB.StreamIsClosed());
  EXPECT_EQ(ERR_NETWORK_CHANGED, delegateB.WaitForClose());
#endif  // defined(OS_ANDROID) || defined(OS_WIN) || defined(OS_IOS)
}

}  // namespace

}  // namespace net
