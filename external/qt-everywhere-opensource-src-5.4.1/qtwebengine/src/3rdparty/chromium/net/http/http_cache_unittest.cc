// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_cache.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/cache_type.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/base/load_timing_info_test_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_log_unittest.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/cert/cert_status_flags.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_transaction.h"
#include "net/http/http_transaction_test_util.h"
#include "net/http/http_util.h"
#include "net/http/mock_http_cache.h"
#include "net/socket/client_socket_handle.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/websockets/websocket_handshake_stream_base.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;

namespace {

// Tests the load timing values of a request that goes through a
// MockNetworkTransaction.
void TestLoadTimingNetworkRequest(const net::LoadTimingInfo& load_timing_info) {
  EXPECT_FALSE(load_timing_info.socket_reused);
  EXPECT_NE(net::NetLog::Source::kInvalidId, load_timing_info.socket_log_id);

  EXPECT_TRUE(load_timing_info.proxy_resolve_start.is_null());
  EXPECT_TRUE(load_timing_info.proxy_resolve_end.is_null());

  net::ExpectConnectTimingHasTimes(load_timing_info.connect_timing,
                                   net::CONNECT_TIMING_HAS_CONNECT_TIMES_ONLY);
  EXPECT_LE(load_timing_info.connect_timing.connect_end,
            load_timing_info.send_start);

  EXPECT_LE(load_timing_info.send_start, load_timing_info.send_end);

  // Set by URLRequest / URLRequestHttpJob, at a higher level.
  EXPECT_TRUE(load_timing_info.request_start_time.is_null());
  EXPECT_TRUE(load_timing_info.request_start.is_null());
  EXPECT_TRUE(load_timing_info.receive_headers_end.is_null());
}

// Tests the load timing values of a request that receives a cached response.
void TestLoadTimingCachedResponse(const net::LoadTimingInfo& load_timing_info) {
  EXPECT_FALSE(load_timing_info.socket_reused);
  EXPECT_EQ(net::NetLog::Source::kInvalidId, load_timing_info.socket_log_id);

  EXPECT_TRUE(load_timing_info.proxy_resolve_start.is_null());
  EXPECT_TRUE(load_timing_info.proxy_resolve_end.is_null());

  net::ExpectConnectTimingHasNoTimes(load_timing_info.connect_timing);

  // Only the send start / end times should be sent, and they should have the
  // same value.
  EXPECT_FALSE(load_timing_info.send_start.is_null());
  EXPECT_EQ(load_timing_info.send_start, load_timing_info.send_end);

  // Set by URLRequest / URLRequestHttpJob, at a higher level.
  EXPECT_TRUE(load_timing_info.request_start_time.is_null());
  EXPECT_TRUE(load_timing_info.request_start.is_null());
  EXPECT_TRUE(load_timing_info.receive_headers_end.is_null());
}

class DeleteCacheCompletionCallback : public net::TestCompletionCallbackBase {
 public:
  explicit DeleteCacheCompletionCallback(MockHttpCache* cache)
      : cache_(cache),
        callback_(base::Bind(&DeleteCacheCompletionCallback::OnComplete,
                             base::Unretained(this))) {
  }

  const net::CompletionCallback& callback() const { return callback_; }

 private:
  void OnComplete(int result) {
    delete cache_;
    SetResult(result);
  }

  MockHttpCache* cache_;
  net::CompletionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(DeleteCacheCompletionCallback);
};

//-----------------------------------------------------------------------------
// helpers

void ReadAndVerifyTransaction(net::HttpTransaction* trans,
                              const MockTransaction& trans_info) {
  std::string content;
  int rv = ReadTransaction(trans, &content);

  EXPECT_EQ(net::OK, rv);
  std::string expected(trans_info.data);
  EXPECT_EQ(expected, content);
}

void RunTransactionTestBase(net::HttpCache* cache,
                            const MockTransaction& trans_info,
                            const MockHttpRequest& request,
                            net::HttpResponseInfo* response_info,
                            const net::BoundNetLog& net_log,
                            net::LoadTimingInfo* load_timing_info,
                            int64* received_bytes) {
  net::TestCompletionCallback callback;

  // write to the cache

  scoped_ptr<net::HttpTransaction> trans;
  int rv = cache->CreateTransaction(net::DEFAULT_PRIORITY, &trans);
  EXPECT_EQ(net::OK, rv);
  ASSERT_TRUE(trans.get());

  rv = trans->Start(&request, callback.callback(), net_log);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(trans_info.return_code, rv);

  if (net::OK != rv)
    return;

  const net::HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response);

  if (response_info)
    *response_info = *response;

  if (load_timing_info) {
    // If a fake network connection is used, need a NetLog to get a fake socket
    // ID.
    EXPECT_TRUE(net_log.net_log());
    *load_timing_info = net::LoadTimingInfo();
    trans->GetLoadTimingInfo(load_timing_info);
  }

  ReadAndVerifyTransaction(trans.get(), trans_info);

  if (received_bytes)
    *received_bytes = trans->GetTotalReceivedBytes();
}

void RunTransactionTestWithRequest(net::HttpCache* cache,
                                   const MockTransaction& trans_info,
                                   const MockHttpRequest& request,
                                   net::HttpResponseInfo* response_info) {
  RunTransactionTestBase(cache, trans_info, request, response_info,
                         net::BoundNetLog(), NULL, NULL);
}

void RunTransactionTestAndGetTiming(net::HttpCache* cache,
                                    const MockTransaction& trans_info,
                                    const net::BoundNetLog& log,
                                    net::LoadTimingInfo* load_timing_info) {
  RunTransactionTestBase(cache, trans_info, MockHttpRequest(trans_info),
                         NULL, log, load_timing_info, NULL);
}

void RunTransactionTest(net::HttpCache* cache,
                        const MockTransaction& trans_info) {
  RunTransactionTestAndGetTiming(cache, trans_info, net::BoundNetLog(), NULL);
}

void RunTransactionTestWithResponseInfo(net::HttpCache* cache,
                                        const MockTransaction& trans_info,
                                        net::HttpResponseInfo* response) {
  RunTransactionTestWithRequest(cache, trans_info, MockHttpRequest(trans_info),
                                response);
}

void RunTransactionTestWithResponseInfoAndGetTiming(
    net::HttpCache* cache,
    const MockTransaction& trans_info,
    net::HttpResponseInfo* response,
    const net::BoundNetLog& log,
    net::LoadTimingInfo* load_timing_info) {
  RunTransactionTestBase(cache, trans_info, MockHttpRequest(trans_info),
                         response, log, load_timing_info, NULL);
}

void RunTransactionTestWithResponse(net::HttpCache* cache,
                                    const MockTransaction& trans_info,
                                    std::string* response_headers) {
  net::HttpResponseInfo response;
  RunTransactionTestWithResponseInfo(cache, trans_info, &response);
  response.headers->GetNormalizedHeaders(response_headers);
}

void RunTransactionTestWithResponseAndGetTiming(
    net::HttpCache* cache,
    const MockTransaction& trans_info,
    std::string* response_headers,
    const net::BoundNetLog& log,
    net::LoadTimingInfo* load_timing_info) {
  net::HttpResponseInfo response;
  RunTransactionTestBase(cache, trans_info, MockHttpRequest(trans_info),
                         &response, log, load_timing_info, NULL);
  response.headers->GetNormalizedHeaders(response_headers);
}

// This class provides a handler for kFastNoStoreGET_Transaction so that the
// no-store header can be included on demand.
class FastTransactionServer {
 public:
  FastTransactionServer() {
    no_store = false;
  }
  ~FastTransactionServer() {}

  void set_no_store(bool value) { no_store = value; }

  static void FastNoStoreHandler(const net::HttpRequestInfo* request,
                                 std::string* response_status,
                                 std::string* response_headers,
                                 std::string* response_data) {
    if (no_store)
      *response_headers = "Cache-Control: no-store\n";
  }

 private:
  static bool no_store;
  DISALLOW_COPY_AND_ASSIGN(FastTransactionServer);
};
bool FastTransactionServer::no_store;

const MockTransaction kFastNoStoreGET_Transaction = {
  "http://www.google.com/nostore",
  "GET",
  base::Time(),
  "",
  net::LOAD_VALIDATE_CACHE,
  "HTTP/1.1 200 OK",
  "Cache-Control: max-age=10000\n",
  base::Time(),
  "<html><body>Google Blah Blah</body></html>",
  TEST_MODE_SYNC_NET_START,
  &FastTransactionServer::FastNoStoreHandler,
  0,
  net::OK
};

// This class provides a handler for kRangeGET_TransactionOK so that the range
// request can be served on demand.
class RangeTransactionServer {
 public:
  RangeTransactionServer() {
    not_modified_ = false;
    modified_ = false;
    bad_200_ = false;
  }
  ~RangeTransactionServer() {
    not_modified_ = false;
    modified_ = false;
    bad_200_ = false;
  }

  // Returns only 416 or 304 when set.
  void set_not_modified(bool value) { not_modified_ = value; }

  // Returns 206 when revalidating a range (instead of 304).
  void set_modified(bool value) { modified_ = value; }

  // Returns 200 instead of 206 (a malformed response overall).
  void set_bad_200(bool value) { bad_200_ = value; }

  static void RangeHandler(const net::HttpRequestInfo* request,
                           std::string* response_status,
                           std::string* response_headers,
                           std::string* response_data);

 private:
  static bool not_modified_;
  static bool modified_;
  static bool bad_200_;
  DISALLOW_COPY_AND_ASSIGN(RangeTransactionServer);
};
bool RangeTransactionServer::not_modified_ = false;
bool RangeTransactionServer::modified_ = false;
bool RangeTransactionServer::bad_200_ = false;

// A dummy extra header that must be preserved on a given request.

// EXTRA_HEADER_LINE doesn't include a line terminator because it
// will be passed to AddHeaderFromString() which doesn't accept them.
#define EXTRA_HEADER_LINE "Extra: header"

// EXTRA_HEADER contains a line terminator, as expected by
// AddHeadersFromString() (_not_ AddHeaderFromString()).
#define EXTRA_HEADER EXTRA_HEADER_LINE "\r\n"

static const char kExtraHeaderKey[] = "Extra";

// Static.
void RangeTransactionServer::RangeHandler(const net::HttpRequestInfo* request,
                                          std::string* response_status,
                                          std::string* response_headers,
                                          std::string* response_data) {
  if (request->extra_headers.IsEmpty()) {
    response_status->assign("HTTP/1.1 416 Requested Range Not Satisfiable");
    response_data->clear();
    return;
  }

  // We want to make sure we don't delete extra headers.
  EXPECT_TRUE(request->extra_headers.HasHeader(kExtraHeaderKey));

  if (request->extra_headers.HasHeader("X-Require-Mock-Auth") &&
      !request->extra_headers.HasHeader("Authorization")) {
    response_status->assign("HTTP/1.1 401 Unauthorized");
    response_data->assign("WWW-Authenticate: Foo\n");
    return;
  }

  if (not_modified_) {
    response_status->assign("HTTP/1.1 304 Not Modified");
    response_data->clear();
    return;
  }

  std::vector<net::HttpByteRange> ranges;
  std::string range_header;
  if (!request->extra_headers.GetHeader(
          net::HttpRequestHeaders::kRange, &range_header) ||
      !net::HttpUtil::ParseRangeHeader(range_header, &ranges) || bad_200_ ||
      ranges.size() != 1) {
    // This is not a byte range request. We return 200.
    response_status->assign("HTTP/1.1 200 OK");
    response_headers->assign("Date: Wed, 28 Nov 2007 09:40:09 GMT");
    response_data->assign("Not a range");
    return;
  }

  // We can handle this range request.
  net::HttpByteRange byte_range = ranges[0];
  if (byte_range.first_byte_position() > 79) {
    response_status->assign("HTTP/1.1 416 Requested Range Not Satisfiable");
    response_data->clear();
    return;
  }

  EXPECT_TRUE(byte_range.ComputeBounds(80));
  int start = static_cast<int>(byte_range.first_byte_position());
  int end = static_cast<int>(byte_range.last_byte_position());

  EXPECT_LT(end, 80);

  std::string content_range = base::StringPrintf(
      "Content-Range: bytes %d-%d/80\n", start, end);
  response_headers->append(content_range);

  if (!request->extra_headers.HasHeader("If-None-Match") || modified_) {
    std::string data;
    if (end == start) {
      EXPECT_EQ(0, end % 10);
      data = "r";
    } else {
      EXPECT_EQ(9, (end - start) % 10);
      for (int block_start = start; block_start < end; block_start += 10) {
        base::StringAppendF(&data, "rg: %02d-%02d ",
                            block_start, block_start + 9);
      }
    }
    *response_data = data;

    if (end - start != 9) {
      // We also have to fix content-length.
      int len = end - start + 1;
      std::string content_length = base::StringPrintf("Content-Length: %d\n",
                                                      len);
      response_headers->replace(response_headers->find("Content-Length:"),
                                content_length.size(), content_length);
    }
  } else {
    response_status->assign("HTTP/1.1 304 Not Modified");
    response_data->clear();
  }
}

const MockTransaction kRangeGET_TransactionOK = {
  "http://www.google.com/range",
  "GET",
  base::Time(),
  "Range: bytes = 40-49\r\n"
  EXTRA_HEADER,
  net::LOAD_NORMAL,
  "HTTP/1.1 206 Partial Content",
  "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
  "ETag: \"foo\"\n"
  "Accept-Ranges: bytes\n"
  "Content-Length: 10\n",
  base::Time(),
  "rg: 40-49 ",
  TEST_MODE_NORMAL,
  &RangeTransactionServer::RangeHandler,
  0,
  net::OK
};

// Verifies the response headers (|response|) match a partial content
// response for the range starting at |start| and ending at |end|.
void Verify206Response(std::string response, int start, int end) {
  std::string raw_headers(net::HttpUtil::AssembleRawHeaders(response.data(),
                                                            response.size()));
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(raw_headers));

  ASSERT_EQ(206, headers->response_code());

  int64 range_start, range_end, object_size;
  ASSERT_TRUE(
      headers->GetContentRange(&range_start, &range_end, &object_size));
  int64 content_length = headers->GetContentLength();

  int length = end - start + 1;
  ASSERT_EQ(length, content_length);
  ASSERT_EQ(start, range_start);
  ASSERT_EQ(end, range_end);
}

// Creates a truncated entry that can be resumed using byte ranges.
void CreateTruncatedEntry(std::string raw_headers, MockHttpCache* cache) {
  // Create a disk cache entry that stores an incomplete resource.
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache->CreateBackendEntry(kRangeGET_TransactionOK.url, &entry,
                                        NULL));

  raw_headers = net::HttpUtil::AssembleRawHeaders(raw_headers.data(),
                                                  raw_headers.size());

  net::HttpResponseInfo response;
  response.response_time = base::Time::Now();
  response.request_time = base::Time::Now();
  response.headers = new net::HttpResponseHeaders(raw_headers);
  // Set the last argument for this to be an incomplete request.
  EXPECT_TRUE(MockHttpCache::WriteResponseInfo(entry, &response, true, true));

  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(100));
  int len = static_cast<int>(base::strlcpy(buf->data(),
                                           "rg: 00-09 rg: 10-19 ", 100));
  net::TestCompletionCallback cb;
  int rv = entry->WriteData(1, 0, buf.get(), len, cb.callback(), true);
  EXPECT_EQ(len, cb.GetResult(rv));
  entry->Close();
}

// Helper to represent a network HTTP response.
struct Response {
  // Set this response into |trans|.
  void AssignTo(MockTransaction* trans) const {
    trans->status = status;
    trans->response_headers = headers;
    trans->data = body;
  }

  std::string status_and_headers() const {
    return std::string(status) + "\n" + std::string(headers);
  }

  const char* status;
  const char* headers;
  const char* body;
};

struct Context {
  Context() : result(net::ERR_IO_PENDING) {}

  int result;
  net::TestCompletionCallback callback;
  scoped_ptr<net::HttpTransaction> trans;
};

class FakeWebSocketHandshakeStreamCreateHelper
    : public net::WebSocketHandshakeStreamBase::CreateHelper {
 public:
  virtual ~FakeWebSocketHandshakeStreamCreateHelper() {}
  virtual net::WebSocketHandshakeStreamBase* CreateBasicStream(
      scoped_ptr<net::ClientSocketHandle> connect, bool using_proxy) OVERRIDE {
    return NULL;
  }
  virtual net::WebSocketHandshakeStreamBase* CreateSpdyStream(
      const base::WeakPtr<net::SpdySession>& session,
      bool use_relative_url) OVERRIDE {
    return NULL;
  }
};

// Returns true if |entry| is not one of the log types paid attention to in this
// test. Note that TYPE_HTTP_CACHE_WRITE_INFO and TYPE_HTTP_CACHE_*_DATA are
// ignored.
bool ShouldIgnoreLogEntry(const net::CapturingNetLog::CapturedEntry& entry) {
  switch (entry.type) {
    case net::NetLog::TYPE_HTTP_CACHE_GET_BACKEND:
    case net::NetLog::TYPE_HTTP_CACHE_OPEN_ENTRY:
    case net::NetLog::TYPE_HTTP_CACHE_CREATE_ENTRY:
    case net::NetLog::TYPE_HTTP_CACHE_ADD_TO_ENTRY:
    case net::NetLog::TYPE_HTTP_CACHE_DOOM_ENTRY:
    case net::NetLog::TYPE_HTTP_CACHE_READ_INFO:
      return false;
    default:
      return true;
  }
}

// Modifies |entries| to only include log entries created by the cache layer and
// asserted on in these tests.
void FilterLogEntries(net::CapturingNetLog::CapturedEntryList* entries) {
  entries->erase(std::remove_if(entries->begin(), entries->end(),
                                &ShouldIgnoreLogEntry),
                 entries->end());
}

}  // namespace


//-----------------------------------------------------------------------------
// Tests.

TEST(HttpCache, CreateThenDestroy) {
  MockHttpCache cache;

  scoped_ptr<net::HttpTransaction> trans;
  EXPECT_EQ(net::OK, cache.CreateTransaction(&trans));
  ASSERT_TRUE(trans.get());
}

TEST(HttpCache, GetBackend) {
  MockHttpCache cache(net::HttpCache::DefaultBackend::InMemory(0));

  disk_cache::Backend* backend;
  net::TestCompletionCallback cb;
  // This will lazily initialize the backend.
  int rv = cache.http_cache()->GetBackend(&backend, cb.callback());
  EXPECT_EQ(net::OK, cb.GetResult(rv));
}

TEST(HttpCache, SimpleGET) {
  MockHttpCache cache;
  net::CapturingBoundNetLog log;
  net::LoadTimingInfo load_timing_info;

  // Write to the cache.
  RunTransactionTestAndGetTiming(cache.http_cache(), kSimpleGET_Transaction,
                                 log.bound(), &load_timing_info);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);
}

TEST(HttpCache, SimpleGETNoDiskCache) {
  MockHttpCache cache;

  cache.disk_cache()->set_fail_requests();

  net::CapturingBoundNetLog log;
  net::LoadTimingInfo load_timing_info;

  // Read from the network, and don't use the cache.
  RunTransactionTestAndGetTiming(cache.http_cache(), kSimpleGET_Transaction,
                                 log.bound(), &load_timing_info);

  // Check that the NetLog was filled as expected.
  // (We attempted to both Open and Create entries, but both failed).
  net::CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  FilterLogEntries(&entries);

  EXPECT_EQ(6u, entries.size());
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 0, net::NetLog::TYPE_HTTP_CACHE_GET_BACKEND));
  EXPECT_TRUE(net::LogContainsEndEvent(
      entries, 1, net::NetLog::TYPE_HTTP_CACHE_GET_BACKEND));
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 2, net::NetLog::TYPE_HTTP_CACHE_OPEN_ENTRY));
  EXPECT_TRUE(net::LogContainsEndEvent(
      entries, 3, net::NetLog::TYPE_HTTP_CACHE_OPEN_ENTRY));
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 4, net::NetLog::TYPE_HTTP_CACHE_CREATE_ENTRY));
  EXPECT_TRUE(net::LogContainsEndEvent(
      entries, 5, net::NetLog::TYPE_HTTP_CACHE_CREATE_ENTRY));

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);
}

TEST(HttpCache, SimpleGETNoDiskCache2) {
  // This will initialize a cache object with NULL backend.
  MockBlockingBackendFactory* factory = new MockBlockingBackendFactory();
  factory->set_fail(true);
  factory->FinishCreation();  // We'll complete synchronously.
  MockHttpCache cache(factory);

  // Read from the network, and don't use the cache.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_FALSE(cache.http_cache()->GetCurrentBackend());
}

// Tests that IOBuffers are not referenced after IO completes.
TEST(HttpCache, ReleaseBuffer) {
  MockHttpCache cache;

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  MockHttpRequest request(kSimpleGET_Transaction);
  scoped_ptr<net::HttpTransaction> trans;
  ASSERT_EQ(net::OK, cache.CreateTransaction(&trans));

  const int kBufferSize = 10;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kBufferSize));
  net::ReleaseBufferCompletionCallback cb(buffer.get());

  int rv = trans->Start(&request, cb.callback(), net::BoundNetLog());
  EXPECT_EQ(net::OK, cb.GetResult(rv));

  rv = trans->Read(buffer.get(), kBufferSize, cb.callback());
  EXPECT_EQ(kBufferSize, cb.GetResult(rv));
}

TEST(HttpCache, SimpleGETWithDiskFailures) {
  MockHttpCache cache;

  cache.disk_cache()->set_soft_failures(true);

  // Read from the network, and fail to write to the cache.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // This one should see an empty cache again.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that disk failures after the transaction has started don't cause the
// request to fail.
TEST(HttpCache, SimpleGETWithDiskFailures2) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  scoped_ptr<Context> c(new Context());
  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_EQ(net::OK, rv);

  rv = c->trans->Start(&request, c->callback.callback(), net::BoundNetLog());
  EXPECT_EQ(net::ERR_IO_PENDING, rv);
  rv = c->callback.WaitForResult();

  // Start failing request now.
  cache.disk_cache()->set_soft_failures(true);

  // We have to open the entry again to propagate the failure flag.
  disk_cache::Entry* en;
  ASSERT_TRUE(cache.OpenBackendEntry(kSimpleGET_Transaction.url, &en));
  en->Close();

  ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
  c.reset();

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // This one should see an empty cache again.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we handle failures to read from the cache.
TEST(HttpCache, SimpleGETWithDiskFailures3) {
  MockHttpCache cache;

  // Read from the network, and write to the cache.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  cache.disk_cache()->set_soft_failures(true);

  // Now fail to read from the cache.
  scoped_ptr<Context> c(new Context());
  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_EQ(net::OK, rv);

  MockHttpRequest request(kSimpleGET_Transaction);
  rv = c->trans->Start(&request, c->callback.callback(), net::BoundNetLog());
  EXPECT_EQ(net::OK, c->callback.GetResult(rv));

  // Now verify that the entry was removed from the cache.
  cache.disk_cache()->set_soft_failures(false);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(3, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadOnlyFromCache_Hit) {
  MockHttpCache cache;

  net::CapturingBoundNetLog log;
  net::LoadTimingInfo load_timing_info;

  // Write to the cache.
  RunTransactionTestAndGetTiming(cache.http_cache(), kSimpleGET_Transaction,
                                 log.bound(), &load_timing_info);

  // Check that the NetLog was filled as expected.
  net::CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  FilterLogEntries(&entries);

  EXPECT_EQ(8u, entries.size());
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 0, net::NetLog::TYPE_HTTP_CACHE_GET_BACKEND));
  EXPECT_TRUE(net::LogContainsEndEvent(
      entries, 1, net::NetLog::TYPE_HTTP_CACHE_GET_BACKEND));
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 2, net::NetLog::TYPE_HTTP_CACHE_OPEN_ENTRY));
  EXPECT_TRUE(net::LogContainsEndEvent(
      entries, 3, net::NetLog::TYPE_HTTP_CACHE_OPEN_ENTRY));
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 4, net::NetLog::TYPE_HTTP_CACHE_CREATE_ENTRY));
  EXPECT_TRUE(net::LogContainsEndEvent(
      entries, 5, net::NetLog::TYPE_HTTP_CACHE_CREATE_ENTRY));
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 6, net::NetLog::TYPE_HTTP_CACHE_ADD_TO_ENTRY));
  EXPECT_TRUE(net::LogContainsEndEvent(
      entries, 7, net::NetLog::TYPE_HTTP_CACHE_ADD_TO_ENTRY));

  TestLoadTimingNetworkRequest(load_timing_info);

  // Force this transaction to read from the cache.
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_ONLY_FROM_CACHE;

  log.Clear();

  RunTransactionTestAndGetTiming(cache.http_cache(), transaction, log.bound(),
                                 &load_timing_info);

  // Check that the NetLog was filled as expected.
  log.GetEntries(&entries);
  FilterLogEntries(&entries);

  EXPECT_EQ(8u, entries.size());
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 0, net::NetLog::TYPE_HTTP_CACHE_GET_BACKEND));
  EXPECT_TRUE(net::LogContainsEndEvent(
      entries, 1, net::NetLog::TYPE_HTTP_CACHE_GET_BACKEND));
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 2, net::NetLog::TYPE_HTTP_CACHE_OPEN_ENTRY));
  EXPECT_TRUE(net::LogContainsEndEvent(
      entries, 3, net::NetLog::TYPE_HTTP_CACHE_OPEN_ENTRY));
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 4, net::NetLog::TYPE_HTTP_CACHE_ADD_TO_ENTRY));
  EXPECT_TRUE(net::LogContainsEndEvent(
      entries, 5, net::NetLog::TYPE_HTTP_CACHE_ADD_TO_ENTRY));
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 6, net::NetLog::TYPE_HTTP_CACHE_READ_INFO));
  EXPECT_TRUE(net::LogContainsEndEvent(
      entries, 7, net::NetLog::TYPE_HTTP_CACHE_READ_INFO));

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingCachedResponse(load_timing_info);
}

TEST(HttpCache, SimpleGET_LoadOnlyFromCache_Miss) {
  MockHttpCache cache;

  // force this transaction to read from the cache
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_ONLY_FROM_CACHE;

  MockHttpRequest request(transaction);
  net::TestCompletionCallback callback;

  scoped_ptr<net::HttpTransaction> trans;
  ASSERT_EQ(net::OK, cache.CreateTransaction(&trans));

  int rv = trans->Start(&request, callback.callback(), net::BoundNetLog());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(net::ERR_CACHE_MISS, rv);

  trans.reset();

  EXPECT_EQ(0, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadPreferringCache_Hit) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to read from the cache if valid
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_PREFERRING_CACHE;

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadPreferringCache_Miss) {
  MockHttpCache cache;

  // force this transaction to read from the cache if valid
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_PREFERRING_CACHE;

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests LOAD_PREFERRING_CACHE in the presence of vary headers.
TEST(HttpCache, SimpleGET_LoadPreferringCache_VaryMatch) {
  MockHttpCache cache;

  // Write to the cache.
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "Foo: bar\r\n";
  transaction.response_headers = "Cache-Control: max-age=10000\n"
                                 "Vary: Foo\n";
  AddMockTransaction(&transaction);
  RunTransactionTest(cache.http_cache(), transaction);

  // Read from the cache.
  transaction.load_flags |= net::LOAD_PREFERRING_CACHE;
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  RemoveMockTransaction(&transaction);
}

// Tests LOAD_PREFERRING_CACHE in the presence of vary headers.
TEST(HttpCache, SimpleGET_LoadPreferringCache_VaryMismatch) {
  MockHttpCache cache;

  // Write to the cache.
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "Foo: bar\r\n";
  transaction.response_headers = "Cache-Control: max-age=10000\n"
                                 "Vary: Foo\n";
  AddMockTransaction(&transaction);
  RunTransactionTest(cache.http_cache(), transaction);

  // Attempt to read from the cache... this is a vary mismatch that must reach
  // the network again.
  transaction.load_flags |= net::LOAD_PREFERRING_CACHE;
  transaction.request_headers = "Foo: none\r\n";
  net::CapturingBoundNetLog log;
  net::LoadTimingInfo load_timing_info;
  RunTransactionTestAndGetTiming(cache.http_cache(), transaction, log.bound(),
                                 &load_timing_info);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);
  RemoveMockTransaction(&transaction);
}

// Tests that LOAD_FROM_CACHE_IF_OFFLINE returns proper response on
// network success
TEST(HttpCache, SimpleGET_CacheOverride_Network) {
  MockHttpCache cache;

  // Prime cache.
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_FROM_CACHE_IF_OFFLINE;
  transaction.response_headers = "Cache-Control: no-cache\n";

  AddMockTransaction(&transaction);
  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  RemoveMockTransaction(&transaction);

  // Re-run transaction; make sure the result came from the network,
  // not the cache.
  transaction.data = "Changed data.";
  AddMockTransaction(&transaction);
  net::HttpResponseInfo response_info;
  RunTransactionTestWithResponseInfo(cache.http_cache(), transaction,
                                     &response_info);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_FALSE(response_info.server_data_unavailable);
  EXPECT_TRUE(response_info.network_accessed);

  RemoveMockTransaction(&transaction);
}

// Tests that LOAD_FROM_CACHE_IF_OFFLINE returns proper response on
// offline failure
TEST(HttpCache, SimpleGET_CacheOverride_Offline) {
  MockHttpCache cache;

  // Prime cache.
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_FROM_CACHE_IF_OFFLINE;
  transaction.response_headers = "Cache-Control: no-cache\n";

  AddMockTransaction(&transaction);
  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  RemoveMockTransaction(&transaction);

  // Network failure with offline error; should return cache entry above +
  // flag signalling stale data.
  transaction.return_code = net::ERR_NAME_NOT_RESOLVED;
  AddMockTransaction(&transaction);

  MockHttpRequest request(transaction);
  net::TestCompletionCallback callback;
  scoped_ptr<net::HttpTransaction> trans;
  ASSERT_EQ(net::OK, cache.CreateTransaction(&trans));
  int rv = trans->Start(&request, callback.callback(), net::BoundNetLog());
  EXPECT_EQ(net::OK, callback.GetResult(rv));

  const net::HttpResponseInfo* response_info = trans->GetResponseInfo();
  ASSERT_TRUE(response_info);
  EXPECT_TRUE(response_info->server_data_unavailable);
  EXPECT_TRUE(response_info->was_cached);
  EXPECT_FALSE(response_info->network_accessed);
  ReadAndVerifyTransaction(trans.get(), transaction);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());

  RemoveMockTransaction(&transaction);
}

// Tests that LOAD_FROM_CACHE_IF_OFFLINE returns proper response on
// non-offline failure.
TEST(HttpCache, SimpleGET_CacheOverride_NonOffline) {
  MockHttpCache cache;

  // Prime cache.
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_FROM_CACHE_IF_OFFLINE;
  transaction.response_headers = "Cache-Control: no-cache\n";

  AddMockTransaction(&transaction);
  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  RemoveMockTransaction(&transaction);

  // Network failure with non-offline error; should fail with that error.
  transaction.return_code = net::ERR_PROXY_CONNECTION_FAILED;
  AddMockTransaction(&transaction);

  net::HttpResponseInfo response_info2;
  RunTransactionTestWithResponseInfo(cache.http_cache(), transaction,
                                     &response_info2);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_FALSE(response_info2.server_data_unavailable);

  RemoveMockTransaction(&transaction);
}

// Tests that was_cached was set properly on a failure, even if the cached
// response wasn't returned.
TEST(HttpCache, SimpleGET_CacheSignal_Failure) {
  MockHttpCache cache;

  // Prime cache.
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers = "Cache-Control: no-cache\n";

  AddMockTransaction(&transaction);
  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  RemoveMockTransaction(&transaction);

  // Network failure with error; should fail but have was_cached set.
  transaction.return_code = net::ERR_FAILED;
  AddMockTransaction(&transaction);

  MockHttpRequest request(transaction);
  net::TestCompletionCallback callback;
  scoped_ptr<net::HttpTransaction> trans;
  int rv = cache.http_cache()->CreateTransaction(net::DEFAULT_PRIORITY, &trans);
  EXPECT_EQ(net::OK, rv);
  ASSERT_TRUE(trans.get());
  rv = trans->Start(&request, callback.callback(), net::BoundNetLog());
  EXPECT_EQ(net::ERR_FAILED, callback.GetResult(rv));

  const net::HttpResponseInfo* response_info = trans->GetResponseInfo();
  ASSERT_TRUE(response_info);
  EXPECT_TRUE(response_info->was_cached);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());

  RemoveMockTransaction(&transaction);
}

// Confirm if we have an empty cache, a read is marked as network verified.
TEST(HttpCache, SimpleGET_NetworkAccessed_Network) {
  MockHttpCache cache;

  // write to the cache
  net::HttpResponseInfo response_info;
  RunTransactionTestWithResponseInfo(cache.http_cache(), kSimpleGET_Transaction,
                                     &response_info);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  EXPECT_TRUE(response_info.network_accessed);
}

// Confirm if we have a fresh entry in cache, it isn't marked as
// network verified.
TEST(HttpCache, SimpleGET_NetworkAccessed_Cache) {
  MockHttpCache cache;

  // Prime cache.
  MockTransaction transaction(kSimpleGET_Transaction);

  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Re-run transaction; make sure we don't mark the network as accessed.
  net::HttpResponseInfo response_info;
  RunTransactionTestWithResponseInfo(cache.http_cache(), transaction,
                                     &response_info);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_FALSE(response_info.server_data_unavailable);
  EXPECT_FALSE(response_info.network_accessed);
}

TEST(HttpCache, SimpleGET_LoadBypassCache) {
  MockHttpCache cache;

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // Force this transaction to write to the cache again.
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_BYPASS_CACHE;

  net::CapturingBoundNetLog log;
  net::LoadTimingInfo load_timing_info;

  // Write to the cache.
  RunTransactionTestAndGetTiming(cache.http_cache(), transaction, log.bound(),
                                 &load_timing_info);

  // Check that the NetLog was filled as expected.
  net::CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  FilterLogEntries(&entries);

  EXPECT_EQ(8u, entries.size());
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 0, net::NetLog::TYPE_HTTP_CACHE_GET_BACKEND));
  EXPECT_TRUE(net::LogContainsEndEvent(
      entries, 1, net::NetLog::TYPE_HTTP_CACHE_GET_BACKEND));
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 2, net::NetLog::TYPE_HTTP_CACHE_DOOM_ENTRY));
  EXPECT_TRUE(net::LogContainsEndEvent(
      entries, 3, net::NetLog::TYPE_HTTP_CACHE_DOOM_ENTRY));
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 4, net::NetLog::TYPE_HTTP_CACHE_CREATE_ENTRY));
  EXPECT_TRUE(net::LogContainsEndEvent(
      entries, 5, net::NetLog::TYPE_HTTP_CACHE_CREATE_ENTRY));
  EXPECT_TRUE(net::LogContainsBeginEvent(
      entries, 6, net::NetLog::TYPE_HTTP_CACHE_ADD_TO_ENTRY));
  EXPECT_TRUE(net::LogContainsEndEvent(
      entries, 7, net::NetLog::TYPE_HTTP_CACHE_ADD_TO_ENTRY));

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);
}

TEST(HttpCache, SimpleGET_LoadBypassCache_Implicit) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to write to the cache again
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "pragma: no-cache\r\n";

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadBypassCache_Implicit2) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to write to the cache again
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "cache-control: no-cache\r\n";

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadValidateCache) {
  MockHttpCache cache;

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // Read from the cache.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // Force this transaction to validate the cache.
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_VALIDATE_CACHE;

  net::HttpResponseInfo response_info;
  net::CapturingBoundNetLog log;
  net::LoadTimingInfo load_timing_info;
  RunTransactionTestWithResponseInfoAndGetTiming(
      cache.http_cache(), transaction, &response_info, log.bound(),
      &load_timing_info);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  EXPECT_TRUE(response_info.network_accessed);
  TestLoadTimingNetworkRequest(load_timing_info);
}

TEST(HttpCache, SimpleGET_LoadValidateCache_Implicit) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // read from the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to validate the cache
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "cache-control: max-age=0\r\n";

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

static void PreserveRequestHeaders_Handler(
    const net::HttpRequestInfo* request,
    std::string* response_status,
    std::string* response_headers,
    std::string* response_data) {
  EXPECT_TRUE(request->extra_headers.HasHeader(kExtraHeaderKey));
}

// Tests that we don't remove extra headers for simple requests.
TEST(HttpCache, SimpleGET_PreserveRequestHeaders) {
  MockHttpCache cache;

  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.handler = PreserveRequestHeaders_Handler;
  transaction.request_headers = EXTRA_HEADER;
  transaction.response_headers = "Cache-Control: max-age=0\n";
  AddMockTransaction(&transaction);

  // Write, then revalidate the entry.
  RunTransactionTest(cache.http_cache(), transaction);
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  RemoveMockTransaction(&transaction);
}

// Tests that we don't remove extra headers for conditionalized requests.
TEST(HttpCache, ConditionalizedGET_PreserveRequestHeaders) {
  MockHttpCache cache;

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), kETagGET_Transaction);

  MockTransaction transaction(kETagGET_Transaction);
  transaction.handler = PreserveRequestHeaders_Handler;
  transaction.request_headers = "If-None-Match: \"foopy\"\r\n"
                                EXTRA_HEADER;
  AddMockTransaction(&transaction);

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  RemoveMockTransaction(&transaction);
}

TEST(HttpCache, SimpleGET_ManyReaders) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  std::vector<Context*> context_list;
  const int kNumTransactions = 5;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(new Context());
    Context* c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_EQ(net::OK, c->result);
    EXPECT_EQ(net::LOAD_STATE_IDLE, c->trans->GetLoadState());

    c->result = c->trans->Start(
        &request, c->callback.callback(), net::BoundNetLog());
  }

  // All requests are waiting for the active entry.
  for (int i = 0; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    EXPECT_EQ(net::LOAD_STATE_WAITING_FOR_CACHE, c->trans->GetLoadState());
  }

  // Allow all requests to move from the Create queue to the active entry.
  base::MessageLoop::current()->RunUntilIdle();

  // The first request should be a writer at this point, and the subsequent
  // requests should be pending.

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // All requests depend on the writer, and the writer is between Start and
  // Read, i.e. idle.
  for (int i = 0; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    EXPECT_EQ(net::LOAD_STATE_IDLE, c->trans->GetLoadState());
  }

  for (int i = 0; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    if (c->result == net::ERR_IO_PENDING)
      c->result = c->callback.WaitForResult();
    ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
  }

  // We should not have had to re-open the disk entry

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  for (int i = 0; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    delete c;
  }
}

// This is a test for http://code.google.com/p/chromium/issues/detail?id=4769.
// If cancelling a request is racing with another request for the same resource
// finishing, we have to make sure that we remove both transactions from the
// entry.
TEST(HttpCache, SimpleGET_RacingReaders) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);
  MockHttpRequest reader_request(kSimpleGET_Transaction);
  reader_request.load_flags = net::LOAD_ONLY_FROM_CACHE;

  std::vector<Context*> context_list;
  const int kNumTransactions = 5;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(new Context());
    Context* c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_EQ(net::OK, c->result);

    MockHttpRequest* this_request = &request;
    if (i == 1 || i == 2)
      this_request = &reader_request;

    c->result = c->trans->Start(
        this_request, c->callback.callback(), net::BoundNetLog());
  }

  // Allow all requests to move from the Create queue to the active entry.
  base::MessageLoop::current()->RunUntilIdle();

  // The first request should be a writer at this point, and the subsequent
  // requests should be pending.

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  Context* c = context_list[0];
  ASSERT_EQ(net::ERR_IO_PENDING, c->result);
  c->result = c->callback.WaitForResult();
  ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);

  // Now we have 2 active readers and two queued transactions.

  EXPECT_EQ(net::LOAD_STATE_IDLE,
            context_list[2]->trans->GetLoadState());
  EXPECT_EQ(net::LOAD_STATE_WAITING_FOR_CACHE,
            context_list[3]->trans->GetLoadState());

  c = context_list[1];
  ASSERT_EQ(net::ERR_IO_PENDING, c->result);
  c->result = c->callback.WaitForResult();
  if (c->result == net::OK)
    ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);

  // At this point we have one reader, two pending transactions and a task on
  // the queue to move to the next transaction. Now we cancel the request that
  // is the current reader, and expect the queued task to be able to start the
  // next request.

  c = context_list[2];
  c->trans.reset();

  for (int i = 3; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    if (c->result == net::ERR_IO_PENDING)
      c->result = c->callback.WaitForResult();
    if (c->result == net::OK)
      ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
  }

  // We should not have had to re-open the disk entry.

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  for (int i = 0; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    delete c;
  }
}

// Tests that we can doom an entry with pending transactions and delete one of
// the pending transactions before the first one completes.
// See http://code.google.com/p/chromium/issues/detail?id=25588
TEST(HttpCache, SimpleGET_DoomWithPending) {
  // We need simultaneous doomed / not_doomed entries so let's use a real cache.
  MockHttpCache cache(net::HttpCache::DefaultBackend::InMemory(1024 * 1024));

  MockHttpRequest request(kSimpleGET_Transaction);
  MockHttpRequest writer_request(kSimpleGET_Transaction);
  writer_request.load_flags = net::LOAD_BYPASS_CACHE;

  ScopedVector<Context> context_list;
  const int kNumTransactions = 4;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(new Context());
    Context* c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_EQ(net::OK, c->result);

    MockHttpRequest* this_request = &request;
    if (i == 3)
      this_request = &writer_request;

    c->result = c->trans->Start(
        this_request, c->callback.callback(), net::BoundNetLog());
  }

  // The first request should be a writer at this point, and the two subsequent
  // requests should be pending. The last request doomed the first entry.

  EXPECT_EQ(2, cache.network_layer()->transaction_count());

  // Cancel the first queued transaction.
  delete context_list[1];
  context_list.get()[1] = NULL;

  for (int i = 0; i < kNumTransactions; ++i) {
    if (i == 1)
      continue;
    Context* c = context_list[i];
    ASSERT_EQ(net::ERR_IO_PENDING, c->result);
    c->result = c->callback.WaitForResult();
    ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
  }
}

// This is a test for http://code.google.com/p/chromium/issues/detail?id=4731.
// We may attempt to delete an entry synchronously with the act of adding a new
// transaction to said entry.
TEST(HttpCache, FastNoStoreGET_DoneWithPending) {
  MockHttpCache cache;

  // The headers will be served right from the call to Start() the request.
  MockHttpRequest request(kFastNoStoreGET_Transaction);
  FastTransactionServer request_handler;
  AddMockTransaction(&kFastNoStoreGET_Transaction);

  std::vector<Context*> context_list;
  const int kNumTransactions = 3;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(new Context());
    Context* c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_EQ(net::OK, c->result);

    c->result = c->trans->Start(
        &request, c->callback.callback(), net::BoundNetLog());
  }

  // Allow all requests to move from the Create queue to the active entry.
  base::MessageLoop::current()->RunUntilIdle();

  // The first request should be a writer at this point, and the subsequent
  // requests should be pending.

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Now, make sure that the second request asks for the entry not to be stored.
  request_handler.set_no_store(true);

  for (int i = 0; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    if (c->result == net::ERR_IO_PENDING)
      c->result = c->callback.WaitForResult();
    ReadAndVerifyTransaction(c->trans.get(), kFastNoStoreGET_Transaction);
    delete c;
  }

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kFastNoStoreGET_Transaction);
}

TEST(HttpCache, SimpleGET_ManyWriters_CancelFirst) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  std::vector<Context*> context_list;
  const int kNumTransactions = 2;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(new Context());
    Context* c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_EQ(net::OK, c->result);

    c->result = c->trans->Start(
        &request, c->callback.callback(), net::BoundNetLog());
  }

  // Allow all requests to move from the Create queue to the active entry.
  base::MessageLoop::current()->RunUntilIdle();

  // The first request should be a writer at this point, and the subsequent
  // requests should be pending.

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  for (int i = 0; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    if (c->result == net::ERR_IO_PENDING)
      c->result = c->callback.WaitForResult();
    // Destroy only the first transaction.
    if (i == 0) {
      delete c;
      context_list[i] = NULL;
    }
  }

  // Complete the rest of the transactions.
  for (int i = 1; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
  }

  // We should have had to re-open the disk entry.

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  for (int i = 1; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    delete c;
  }
}

// Tests that we can cancel requests that are queued waiting to open the disk
// cache entry.
TEST(HttpCache, SimpleGET_ManyWriters_CancelCreate) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  std::vector<Context*> context_list;
  const int kNumTransactions = 5;

  for (int i = 0; i < kNumTransactions; i++) {
    context_list.push_back(new Context());
    Context* c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_EQ(net::OK, c->result);

    c->result = c->trans->Start(
        &request, c->callback.callback(), net::BoundNetLog());
  }

  // The first request should be creating the disk cache entry and the others
  // should be pending.

  EXPECT_EQ(0, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Cancel a request from the pending queue.
  delete context_list[3];
  context_list[3] = NULL;

  // Cancel the request that is creating the entry. This will force the pending
  // operations to restart.
  delete context_list[0];
  context_list[0] = NULL;

  // Complete the rest of the transactions.
  for (int i = 1; i < kNumTransactions; i++) {
    Context* c = context_list[i];
    if (c) {
      c->result = c->callback.GetResult(c->result);
      ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
    }
  }

  // We should have had to re-create the disk entry.

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  for (int i = 1; i < kNumTransactions; ++i) {
    delete context_list[i];
  }
}

// Tests that we can cancel a single request to open a disk cache entry.
TEST(HttpCache, SimpleGET_CancelCreate) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  Context* c = new Context();

  c->result = cache.CreateTransaction(&c->trans);
  ASSERT_EQ(net::OK, c->result);

  c->result = c->trans->Start(
      &request, c->callback.callback(), net::BoundNetLog());
  EXPECT_EQ(net::ERR_IO_PENDING, c->result);

  // Release the reference that the mock disk cache keeps for this entry, so
  // that we test that the http cache handles the cancellation correctly.
  cache.disk_cache()->ReleaseAll();
  delete c;

  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that we delete/create entries even if multiple requests are queued.
TEST(HttpCache, SimpleGET_ManyWriters_BypassCache) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);
  request.load_flags = net::LOAD_BYPASS_CACHE;

  std::vector<Context*> context_list;
  const int kNumTransactions = 5;

  for (int i = 0; i < kNumTransactions; i++) {
    context_list.push_back(new Context());
    Context* c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_EQ(net::OK, c->result);

    c->result = c->trans->Start(
        &request, c->callback.callback(), net::BoundNetLog());
  }

  // The first request should be deleting the disk cache entry and the others
  // should be pending.

  EXPECT_EQ(0, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());

  // Complete the transactions.
  for (int i = 0; i < kNumTransactions; i++) {
    Context* c = context_list[i];
    c->result = c->callback.GetResult(c->result);
    ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
  }

  // We should have had to re-create the disk entry multiple times.

  EXPECT_EQ(5, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(5, cache.disk_cache()->create_count());

  for (int i = 0; i < kNumTransactions; ++i) {
    delete context_list[i];
  }
}

TEST(HttpCache, SimpleGET_AbandonedCacheRead) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  MockHttpRequest request(kSimpleGET_Transaction);
  net::TestCompletionCallback callback;

  scoped_ptr<net::HttpTransaction> trans;
  ASSERT_EQ(net::OK, cache.CreateTransaction(&trans));
  int rv = trans->Start(&request, callback.callback(), net::BoundNetLog());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(net::OK, rv);

  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(256));
  rv = trans->Read(buf.get(), 256, callback.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  // Test that destroying the transaction while it is reading from the cache
  // works properly.
  trans.reset();

  // Make sure we pump any pending events, which should include a call to
  // HttpCache::Transaction::OnCacheReadCompleted.
  base::MessageLoop::current()->RunUntilIdle();
}

// Tests that we can delete the HttpCache and deal with queued transactions
// ("waiting for the backend" as opposed to Active or Doomed entries).
TEST(HttpCache, SimpleGET_ManyWriters_DeleteCache) {
  scoped_ptr<MockHttpCache> cache(new MockHttpCache(
                                      new MockBackendNoCbFactory()));

  MockHttpRequest request(kSimpleGET_Transaction);

  std::vector<Context*> context_list;
  const int kNumTransactions = 5;

  for (int i = 0; i < kNumTransactions; i++) {
    context_list.push_back(new Context());
    Context* c = context_list[i];

    c->result = cache->CreateTransaction(&c->trans);
    ASSERT_EQ(net::OK, c->result);

    c->result = c->trans->Start(
        &request, c->callback.callback(), net::BoundNetLog());
  }

  // The first request should be creating the disk cache entry and the others
  // should be pending.

  EXPECT_EQ(0, cache->network_layer()->transaction_count());
  EXPECT_EQ(0, cache->disk_cache()->open_count());
  EXPECT_EQ(0, cache->disk_cache()->create_count());

  cache.reset();

  // There is not much to do with the transactions at this point... they are
  // waiting for a callback that will not fire.
  for (int i = 0; i < kNumTransactions; ++i) {
    delete context_list[i];
  }
}

// Tests that we queue requests when initializing the backend.
TEST(HttpCache, SimpleGET_WaitForBackend) {
  MockBlockingBackendFactory* factory = new MockBlockingBackendFactory();
  MockHttpCache cache(factory);

  MockHttpRequest request0(kSimpleGET_Transaction);
  MockHttpRequest request1(kTypicalGET_Transaction);
  MockHttpRequest request2(kETagGET_Transaction);

  std::vector<Context*> context_list;
  const int kNumTransactions = 3;

  for (int i = 0; i < kNumTransactions; i++) {
    context_list.push_back(new Context());
    Context* c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_EQ(net::OK, c->result);
  }

  context_list[0]->result = context_list[0]->trans->Start(
      &request0, context_list[0]->callback.callback(), net::BoundNetLog());
  context_list[1]->result = context_list[1]->trans->Start(
      &request1, context_list[1]->callback.callback(), net::BoundNetLog());
  context_list[2]->result = context_list[2]->trans->Start(
      &request2, context_list[2]->callback.callback(), net::BoundNetLog());

  // Just to make sure that everything is still pending.
  base::MessageLoop::current()->RunUntilIdle();

  // The first request should be creating the disk cache.
  EXPECT_FALSE(context_list[0]->callback.have_result());

  factory->FinishCreation();

  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(3, cache.disk_cache()->create_count());

  for (int i = 0; i < kNumTransactions; ++i) {
    EXPECT_TRUE(context_list[i]->callback.have_result());
    delete context_list[i];
  }
}

// Tests that we can cancel requests that are queued waiting for the backend
// to be initialized.
TEST(HttpCache, SimpleGET_WaitForBackend_CancelCreate) {
  MockBlockingBackendFactory* factory = new MockBlockingBackendFactory();
  MockHttpCache cache(factory);

  MockHttpRequest request0(kSimpleGET_Transaction);
  MockHttpRequest request1(kTypicalGET_Transaction);
  MockHttpRequest request2(kETagGET_Transaction);

  std::vector<Context*> context_list;
  const int kNumTransactions = 3;

  for (int i = 0; i < kNumTransactions; i++) {
    context_list.push_back(new Context());
    Context* c = context_list[i];

    c->result = cache.CreateTransaction(&c->trans);
    ASSERT_EQ(net::OK, c->result);
  }

  context_list[0]->result = context_list[0]->trans->Start(
      &request0, context_list[0]->callback.callback(), net::BoundNetLog());
  context_list[1]->result = context_list[1]->trans->Start(
      &request1, context_list[1]->callback.callback(), net::BoundNetLog());
  context_list[2]->result = context_list[2]->trans->Start(
      &request2, context_list[2]->callback.callback(), net::BoundNetLog());

  // Just to make sure that everything is still pending.
  base::MessageLoop::current()->RunUntilIdle();

  // The first request should be creating the disk cache.
  EXPECT_FALSE(context_list[0]->callback.have_result());

  // Cancel a request from the pending queue.
  delete context_list[1];
  context_list[1] = NULL;

  // Cancel the request that is creating the entry.
  delete context_list[0];
  context_list[0] = NULL;

  // Complete the last transaction.
  factory->FinishCreation();

  context_list[2]->result =
      context_list[2]->callback.GetResult(context_list[2]->result);
  ReadAndVerifyTransaction(context_list[2]->trans.get(), kETagGET_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  delete context_list[2];
}

// Tests that we can delete the cache while creating the backend.
TEST(HttpCache, DeleteCacheWaitingForBackend) {
  MockBlockingBackendFactory* factory = new MockBlockingBackendFactory();
  scoped_ptr<MockHttpCache> cache(new MockHttpCache(factory));

  MockHttpRequest request(kSimpleGET_Transaction);

  scoped_ptr<Context> c(new Context());
  c->result = cache->CreateTransaction(&c->trans);
  ASSERT_EQ(net::OK, c->result);

  c->trans->Start(&request, c->callback.callback(), net::BoundNetLog());

  // Just to make sure that everything is still pending.
  base::MessageLoop::current()->RunUntilIdle();

  // The request should be creating the disk cache.
  EXPECT_FALSE(c->callback.have_result());

  // We cannot call FinishCreation because the factory itself will go away with
  // the cache, so grab the callback and attempt to use it.
  net::CompletionCallback callback = factory->callback();
  scoped_ptr<disk_cache::Backend>* backend = factory->backend();

  cache.reset();
  base::MessageLoop::current()->RunUntilIdle();

  backend->reset();
  callback.Run(net::ERR_ABORTED);
}

// Tests that we can delete the cache while creating the backend, from within
// one of the callbacks.
TEST(HttpCache, DeleteCacheWaitingForBackend2) {
  MockBlockingBackendFactory* factory = new MockBlockingBackendFactory();
  MockHttpCache* cache = new MockHttpCache(factory);

  DeleteCacheCompletionCallback cb(cache);
  disk_cache::Backend* backend;
  int rv = cache->http_cache()->GetBackend(&backend, cb.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  // Now let's queue a regular transaction
  MockHttpRequest request(kSimpleGET_Transaction);

  scoped_ptr<Context> c(new Context());
  c->result = cache->CreateTransaction(&c->trans);
  ASSERT_EQ(net::OK, c->result);

  c->trans->Start(&request, c->callback.callback(), net::BoundNetLog());

  // And another direct backend request.
  net::TestCompletionCallback cb2;
  rv = cache->http_cache()->GetBackend(&backend, cb2.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  // Just to make sure that everything is still pending.
  base::MessageLoop::current()->RunUntilIdle();

  // The request should be queued.
  EXPECT_FALSE(c->callback.have_result());

  // Generate the callback.
  factory->FinishCreation();
  rv = cb.WaitForResult();

  // The cache should be gone by now.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(net::OK, c->callback.GetResult(c->result));
  EXPECT_FALSE(cb2.have_result());
}

TEST(HttpCache, TypicalGET_ConditionalRequest) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kTypicalGET_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Get the same URL again, but this time we expect it to result
  // in a conditional request.
  net::CapturingBoundNetLog log;
  net::LoadTimingInfo load_timing_info;
  RunTransactionTestAndGetTiming(cache.http_cache(), kTypicalGET_Transaction,
                                 log.bound(), &load_timing_info);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);
}

static void ETagGet_ConditionalRequest_Handler(
    const net::HttpRequestInfo* request,
    std::string* response_status,
    std::string* response_headers,
    std::string* response_data) {
  EXPECT_TRUE(
      request->extra_headers.HasHeader(net::HttpRequestHeaders::kIfNoneMatch));
  response_status->assign("HTTP/1.1 304 Not Modified");
  response_headers->assign(kETagGET_Transaction.response_headers);
  response_data->clear();
}

TEST(HttpCache, ETagGET_ConditionalRequest_304) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kETagGET_Transaction);

  // write to the cache
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Get the same URL again, but this time we expect it to result
  // in a conditional request.
  transaction.load_flags = net::LOAD_VALIDATE_CACHE;
  transaction.handler = ETagGet_ConditionalRequest_Handler;
  net::CapturingBoundNetLog log;
  net::LoadTimingInfo load_timing_info;
  RunTransactionTestAndGetTiming(cache.http_cache(), transaction, log.bound(),
                                 &load_timing_info);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);
}

class RevalidationServer {
 public:
  RevalidationServer() {
    s_etag_used_ = false;
    s_last_modified_used_ = false;
  }

  bool EtagUsed() { return s_etag_used_; }
  bool LastModifiedUsed() { return s_last_modified_used_; }

  static void Handler(const net::HttpRequestInfo* request,
                      std::string* response_status,
                      std::string* response_headers,
                      std::string* response_data);

 private:
  static bool s_etag_used_;
  static bool s_last_modified_used_;
};
bool RevalidationServer::s_etag_used_ = false;
bool RevalidationServer::s_last_modified_used_ = false;

void RevalidationServer::Handler(const net::HttpRequestInfo* request,
                                 std::string* response_status,
                                 std::string* response_headers,
                                 std::string* response_data) {
  if (request->extra_headers.HasHeader(net::HttpRequestHeaders::kIfNoneMatch))
      s_etag_used_ = true;

  if (request->extra_headers.HasHeader(
          net::HttpRequestHeaders::kIfModifiedSince)) {
      s_last_modified_used_ = true;
  }

  if (s_etag_used_ || s_last_modified_used_) {
    response_status->assign("HTTP/1.1 304 Not Modified");
    response_headers->assign(kTypicalGET_Transaction.response_headers);
    response_data->clear();
  } else {
    response_status->assign(kTypicalGET_Transaction.status);
    response_headers->assign(kTypicalGET_Transaction.response_headers);
    response_data->assign(kTypicalGET_Transaction.data);
  }
}

// Tests revalidation after a vary match.
TEST(HttpCache, SimpleGET_LoadValidateCache_VaryMatch) {
  MockHttpCache cache;

  // Write to the cache.
  MockTransaction transaction(kTypicalGET_Transaction);
  transaction.request_headers = "Foo: bar\r\n";
  transaction.response_headers =
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\n"
      "Last-Modified: Wed, 28 Nov 2007 00:40:09 GMT\n"
      "Etag: \"foopy\"\n"
      "Cache-Control: max-age=0\n"
      "Vary: Foo\n";
  AddMockTransaction(&transaction);
  RunTransactionTest(cache.http_cache(), transaction);

  // Read from the cache.
  RevalidationServer server;
  transaction.handler = server.Handler;
  net::CapturingBoundNetLog log;
  net::LoadTimingInfo load_timing_info;
  RunTransactionTestAndGetTiming(cache.http_cache(), transaction, log.bound(),
                                 &load_timing_info);

  EXPECT_TRUE(server.EtagUsed());
  EXPECT_TRUE(server.LastModifiedUsed());
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);
  RemoveMockTransaction(&transaction);
}

// Tests revalidation after a vary mismatch if etag is present.
TEST(HttpCache, SimpleGET_LoadValidateCache_VaryMismatch) {
  MockHttpCache cache;

  // Write to the cache.
  MockTransaction transaction(kTypicalGET_Transaction);
  transaction.request_headers = "Foo: bar\r\n";
  transaction.response_headers =
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\n"
      "Last-Modified: Wed, 28 Nov 2007 00:40:09 GMT\n"
      "Etag: \"foopy\"\n"
      "Cache-Control: max-age=0\n"
      "Vary: Foo\n";
  AddMockTransaction(&transaction);
  RunTransactionTest(cache.http_cache(), transaction);

  // Read from the cache and revalidate the entry.
  RevalidationServer server;
  transaction.handler = server.Handler;
  transaction.request_headers = "Foo: none\r\n";
  net::CapturingBoundNetLog log;
  net::LoadTimingInfo load_timing_info;
  RunTransactionTestAndGetTiming(cache.http_cache(), transaction, log.bound(),
                                 &load_timing_info);

  EXPECT_TRUE(server.EtagUsed());
  EXPECT_FALSE(server.LastModifiedUsed());
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);
  RemoveMockTransaction(&transaction);
}

// Tests lack of revalidation after a vary mismatch and no etag.
TEST(HttpCache, SimpleGET_LoadDontValidateCache_VaryMismatch) {
  MockHttpCache cache;

  // Write to the cache.
  MockTransaction transaction(kTypicalGET_Transaction);
  transaction.request_headers = "Foo: bar\r\n";
  transaction.response_headers =
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\n"
      "Last-Modified: Wed, 28 Nov 2007 00:40:09 GMT\n"
      "Cache-Control: max-age=0\n"
      "Vary: Foo\n";
  AddMockTransaction(&transaction);
  RunTransactionTest(cache.http_cache(), transaction);

  // Read from the cache and don't revalidate the entry.
  RevalidationServer server;
  transaction.handler = server.Handler;
  transaction.request_headers = "Foo: none\r\n";
  net::CapturingBoundNetLog log;
  net::LoadTimingInfo load_timing_info;
  RunTransactionTestAndGetTiming(cache.http_cache(), transaction, log.bound(),
                                 &load_timing_info);

  EXPECT_FALSE(server.EtagUsed());
  EXPECT_FALSE(server.LastModifiedUsed());
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);
  RemoveMockTransaction(&transaction);
}

static void ETagGet_UnconditionalRequest_Handler(
    const net::HttpRequestInfo* request,
    std::string* response_status,
    std::string* response_headers,
    std::string* response_data) {
  EXPECT_FALSE(
      request->extra_headers.HasHeader(net::HttpRequestHeaders::kIfNoneMatch));
}

TEST(HttpCache, ETagGET_Http10) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kETagGET_Transaction);
  transaction.status = "HTTP/1.0 200 OK";

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Get the same URL again, without generating a conditional request.
  transaction.load_flags = net::LOAD_VALIDATE_CACHE;
  transaction.handler = ETagGet_UnconditionalRequest_Handler;
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST(HttpCache, ETagGET_Http10_Range) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kETagGET_Transaction);
  transaction.status = "HTTP/1.0 200 OK";

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Get the same URL again, but use a byte range request.
  transaction.load_flags = net::LOAD_VALIDATE_CACHE;
  transaction.handler = ETagGet_UnconditionalRequest_Handler;
  transaction.request_headers = "Range: bytes = 5-\r\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

static void ETagGet_ConditionalRequest_NoStore_Handler(
    const net::HttpRequestInfo* request,
    std::string* response_status,
    std::string* response_headers,
    std::string* response_data) {
  EXPECT_TRUE(
      request->extra_headers.HasHeader(net::HttpRequestHeaders::kIfNoneMatch));
  response_status->assign("HTTP/1.1 304 Not Modified");
  response_headers->assign("Cache-Control: no-store\n");
  response_data->clear();
}

TEST(HttpCache, ETagGET_ConditionalRequest_304_NoStore) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kETagGET_Transaction);

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Get the same URL again, but this time we expect it to result
  // in a conditional request.
  transaction.load_flags = net::LOAD_VALIDATE_CACHE;
  transaction.handler = ETagGet_ConditionalRequest_NoStore_Handler;
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  ScopedMockTransaction transaction2(kETagGET_Transaction);

  // Write to the cache again. This should create a new entry.
  RunTransactionTest(cache.http_cache(), transaction2);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Helper that does 4 requests using HttpCache:
//
// (1) loads |kUrl| -- expects |net_response_1| to be returned.
// (2) loads |kUrl| from cache only -- expects |net_response_1| to be returned.
// (3) loads |kUrl| using |extra_request_headers| -- expects |net_response_2| to
//     be returned.
// (4) loads |kUrl| from cache only -- expects |cached_response_2| to be
//     returned.
static void ConditionalizedRequestUpdatesCacheHelper(
    const Response& net_response_1,
    const Response& net_response_2,
    const Response& cached_response_2,
    const char* extra_request_headers) {
  MockHttpCache cache;

  // The URL we will be requesting.
  const char* kUrl = "http://foobar.com/main.css";

  // Junk network response.
  static const Response kUnexpectedResponse = {
    "HTTP/1.1 500 Unexpected",
    "Server: unexpected_header",
    "unexpected body"
  };

  // We will control the network layer's responses for |kUrl| using
  // |mock_network_response|.
  MockTransaction mock_network_response = { 0 };
  mock_network_response.url = kUrl;
  AddMockTransaction(&mock_network_response);

  // Request |kUrl| for the first time. It should hit the network and
  // receive |kNetResponse1|, which it saves into the HTTP cache.

  MockTransaction request = { 0 };
  request.url = kUrl;
  request.method = "GET";
  request.request_headers = "";

  net_response_1.AssignTo(&mock_network_response);  // Network mock.
  net_response_1.AssignTo(&request);                // Expected result.

  std::string response_headers;
  RunTransactionTestWithResponse(
      cache.http_cache(), request, &response_headers);

  EXPECT_EQ(net_response_1.status_and_headers(), response_headers);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Request |kUrl| a second time. Now |kNetResponse1| it is in the HTTP
  // cache, so we don't hit the network.

  request.load_flags = net::LOAD_ONLY_FROM_CACHE;

  kUnexpectedResponse.AssignTo(&mock_network_response);  // Network mock.
  net_response_1.AssignTo(&request);                     // Expected result.

  RunTransactionTestWithResponse(
      cache.http_cache(), request, &response_headers);

  EXPECT_EQ(net_response_1.status_and_headers(), response_headers);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Request |kUrl| yet again, but this time give the request an
  // "If-Modified-Since" header. This will cause the request to re-hit the
  // network. However now the network response is going to be
  // different -- this simulates a change made to the CSS file.

  request.request_headers = extra_request_headers;
  request.load_flags = net::LOAD_NORMAL;

  net_response_2.AssignTo(&mock_network_response);  // Network mock.
  net_response_2.AssignTo(&request);                // Expected result.

  RunTransactionTestWithResponse(
      cache.http_cache(), request, &response_headers);

  EXPECT_EQ(net_response_2.status_and_headers(), response_headers);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Finally, request |kUrl| again. This request should be serviced from
  // the cache. Moreover, the value in the cache should be |kNetResponse2|
  // and NOT |kNetResponse1|. The previous step should have replaced the
  // value in the cache with the modified response.

  request.request_headers = "";
  request.load_flags = net::LOAD_ONLY_FROM_CACHE;

  kUnexpectedResponse.AssignTo(&mock_network_response);  // Network mock.
  cached_response_2.AssignTo(&request);                  // Expected result.

  RunTransactionTestWithResponse(
      cache.http_cache(), request, &response_headers);

  EXPECT_EQ(cached_response_2.status_and_headers(), response_headers);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&mock_network_response);
}

// Check that when an "if-modified-since" header is attached
// to the request, the result still updates the cached entry.
TEST(HttpCache, ConditionalizedRequestUpdatesCache1) {
  // First network response for |kUrl|.
  static const Response kNetResponse1 = {
    "HTTP/1.1 200 OK",
    "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    "body1"
  };

  // Second network response for |kUrl|.
  static const Response kNetResponse2 = {
    "HTTP/1.1 200 OK",
    "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
    "Last-Modified: Fri, 03 Jul 2009 02:14:27 GMT\n",
    "body2"
  };

  const char* extra_headers =
      "If-Modified-Since: Wed, 06 Feb 2008 22:38:21 GMT\r\n";

  ConditionalizedRequestUpdatesCacheHelper(
      kNetResponse1, kNetResponse2, kNetResponse2, extra_headers);
}

// Check that when an "if-none-match" header is attached
// to the request, the result updates the cached entry.
TEST(HttpCache, ConditionalizedRequestUpdatesCache2) {
  // First network response for |kUrl|.
  static const Response kNetResponse1 = {
    "HTTP/1.1 200 OK",
    "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
    "Etag: \"ETAG1\"\n"
    "Expires: Wed, 7 Sep 2033 21:46:42 GMT\n",  // Should never expire.
    "body1"
  };

  // Second network response for |kUrl|.
  static const Response kNetResponse2 = {
    "HTTP/1.1 200 OK",
    "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
    "Etag: \"ETAG2\"\n"
    "Expires: Wed, 7 Sep 2033 21:46:42 GMT\n",  // Should never expire.
    "body2"
  };

  const char* extra_headers = "If-None-Match: \"ETAG1\"\r\n";

  ConditionalizedRequestUpdatesCacheHelper(
      kNetResponse1, kNetResponse2, kNetResponse2, extra_headers);
}

// Check that when an "if-modified-since" header is attached
// to a request, the 304 (not modified result) result updates the cached
// headers, and the 304 response is returned rather than the cached response.
TEST(HttpCache, ConditionalizedRequestUpdatesCache3) {
  // First network response for |kUrl|.
  static const Response kNetResponse1 = {
    "HTTP/1.1 200 OK",
    "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
    "Server: server1\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    "body1"
  };

  // Second network response for |kUrl|.
  static const Response kNetResponse2 = {
    "HTTP/1.1 304 Not Modified",
    "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
    "Server: server2\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    ""
  };

  static const Response kCachedResponse2 = {
    "HTTP/1.1 200 OK",
    "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
    "Server: server2\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    "body1"
  };

  const char* extra_headers =
      "If-Modified-Since: Wed, 06 Feb 2008 22:38:21 GMT\r\n";

  ConditionalizedRequestUpdatesCacheHelper(
      kNetResponse1, kNetResponse2, kCachedResponse2, extra_headers);
}

// Test that when doing an externally conditionalized if-modified-since
// and there is no corresponding cache entry, a new cache entry is NOT
// created (304 response).
TEST(HttpCache, ConditionalizedRequestUpdatesCache4) {
  MockHttpCache cache;

  const char* kUrl = "http://foobar.com/main.css";

  static const Response kNetResponse = {
    "HTTP/1.1 304 Not Modified",
    "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    ""
  };

  const char* kExtraRequestHeaders =
      "If-Modified-Since: Wed, 06 Feb 2008 22:38:21 GMT\r\n";

  // We will control the network layer's responses for |kUrl| using
  // |mock_network_response|.
  MockTransaction mock_network_response = { 0 };
  mock_network_response.url = kUrl;
  AddMockTransaction(&mock_network_response);

  MockTransaction request = { 0 };
  request.url = kUrl;
  request.method = "GET";
  request.request_headers = kExtraRequestHeaders;

  kNetResponse.AssignTo(&mock_network_response);  // Network mock.
  kNetResponse.AssignTo(&request);                // Expected result.

  std::string response_headers;
  RunTransactionTestWithResponse(
      cache.http_cache(), request, &response_headers);

  EXPECT_EQ(kNetResponse.status_and_headers(), response_headers);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());

  RemoveMockTransaction(&mock_network_response);
}

// Test that when doing an externally conditionalized if-modified-since
// and there is no corresponding cache entry, a new cache entry is NOT
// created (200 response).
TEST(HttpCache, ConditionalizedRequestUpdatesCache5) {
  MockHttpCache cache;

  const char* kUrl = "http://foobar.com/main.css";

  static const Response kNetResponse = {
    "HTTP/1.1 200 OK",
    "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    "foobar!!!"
  };

  const char* kExtraRequestHeaders =
      "If-Modified-Since: Wed, 06 Feb 2008 22:38:21 GMT\r\n";

  // We will control the network layer's responses for |kUrl| using
  // |mock_network_response|.
  MockTransaction mock_network_response = { 0 };
  mock_network_response.url = kUrl;
  AddMockTransaction(&mock_network_response);

  MockTransaction request = { 0 };
  request.url = kUrl;
  request.method = "GET";
  request.request_headers = kExtraRequestHeaders;

  kNetResponse.AssignTo(&mock_network_response);  // Network mock.
  kNetResponse.AssignTo(&request);                // Expected result.

  std::string response_headers;
  RunTransactionTestWithResponse(
      cache.http_cache(), request, &response_headers);

  EXPECT_EQ(kNetResponse.status_and_headers(), response_headers);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());

  RemoveMockTransaction(&mock_network_response);
}

// Test that when doing an externally conditionalized if-modified-since
// if the date does not match the cache entry's last-modified date,
// then we do NOT use the response (304) to update the cache.
// (the if-modified-since date is 2 days AFTER the cache's modification date).
TEST(HttpCache, ConditionalizedRequestUpdatesCache6) {
  static const Response kNetResponse1 = {
    "HTTP/1.1 200 OK",
    "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
    "Server: server1\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    "body1"
  };

  // Second network response for |kUrl|.
  static const Response kNetResponse2 = {
    "HTTP/1.1 304 Not Modified",
    "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
    "Server: server2\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    ""
  };

  // This is two days in the future from the original response's last-modified
  // date!
  const char* kExtraRequestHeaders =
      "If-Modified-Since: Fri, 08 Feb 2008 22:38:21 GMT\r\n";

  ConditionalizedRequestUpdatesCacheHelper(
      kNetResponse1, kNetResponse2, kNetResponse1, kExtraRequestHeaders);
}

// Test that when doing an externally conditionalized if-none-match
// if the etag does not match the cache entry's etag, then we do not use the
// response (304) to update the cache.
TEST(HttpCache, ConditionalizedRequestUpdatesCache7) {
  static const Response kNetResponse1 = {
    "HTTP/1.1 200 OK",
    "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
    "Etag: \"Foo1\"\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    "body1"
  };

  // Second network response for |kUrl|.
  static const Response kNetResponse2 = {
    "HTTP/1.1 304 Not Modified",
    "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
    "Etag: \"Foo2\"\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    ""
  };

  // Different etag from original response.
  const char* kExtraRequestHeaders = "If-None-Match: \"Foo2\"\r\n";

  ConditionalizedRequestUpdatesCacheHelper(
      kNetResponse1, kNetResponse2, kNetResponse1, kExtraRequestHeaders);
}

// Test that doing an externally conditionalized request with both if-none-match
// and if-modified-since updates the cache.
TEST(HttpCache, ConditionalizedRequestUpdatesCache8) {
  static const Response kNetResponse1 = {
    "HTTP/1.1 200 OK",
    "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
    "Etag: \"Foo1\"\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    "body1"
  };

  // Second network response for |kUrl|.
  static const Response kNetResponse2 = {
    "HTTP/1.1 200 OK",
    "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
    "Etag: \"Foo2\"\n"
    "Last-Modified: Fri, 03 Jul 2009 02:14:27 GMT\n",
    "body2"
  };

  const char* kExtraRequestHeaders =
      "If-Modified-Since: Wed, 06 Feb 2008 22:38:21 GMT\r\n"
      "If-None-Match: \"Foo1\"\r\n";

  ConditionalizedRequestUpdatesCacheHelper(
      kNetResponse1, kNetResponse2, kNetResponse2, kExtraRequestHeaders);
}

// Test that doing an externally conditionalized request with both if-none-match
// and if-modified-since does not update the cache with only one match.
TEST(HttpCache, ConditionalizedRequestUpdatesCache9) {
  static const Response kNetResponse1 = {
    "HTTP/1.1 200 OK",
    "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
    "Etag: \"Foo1\"\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    "body1"
  };

  // Second network response for |kUrl|.
  static const Response kNetResponse2 = {
    "HTTP/1.1 200 OK",
    "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
    "Etag: \"Foo2\"\n"
    "Last-Modified: Fri, 03 Jul 2009 02:14:27 GMT\n",
    "body2"
  };

  // The etag doesn't match what we have stored.
  const char* kExtraRequestHeaders =
      "If-Modified-Since: Wed, 06 Feb 2008 22:38:21 GMT\r\n"
      "If-None-Match: \"Foo2\"\r\n";

  ConditionalizedRequestUpdatesCacheHelper(
      kNetResponse1, kNetResponse2, kNetResponse1, kExtraRequestHeaders);
}

// Test that doing an externally conditionalized request with both if-none-match
// and if-modified-since does not update the cache with only one match.
TEST(HttpCache, ConditionalizedRequestUpdatesCache10) {
  static const Response kNetResponse1 = {
    "HTTP/1.1 200 OK",
    "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
    "Etag: \"Foo1\"\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    "body1"
  };

  // Second network response for |kUrl|.
  static const Response kNetResponse2 = {
    "HTTP/1.1 200 OK",
    "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
    "Etag: \"Foo2\"\n"
    "Last-Modified: Fri, 03 Jul 2009 02:14:27 GMT\n",
    "body2"
  };

  // The modification date doesn't match what we have stored.
  const char* kExtraRequestHeaders =
      "If-Modified-Since: Fri, 08 Feb 2008 22:38:21 GMT\r\n"
      "If-None-Match: \"Foo1\"\r\n";

  ConditionalizedRequestUpdatesCacheHelper(
      kNetResponse1, kNetResponse2, kNetResponse1, kExtraRequestHeaders);
}

TEST(HttpCache, UrlContainingHash) {
  MockHttpCache cache;

  // Do a typical GET request -- should write an entry into our cache.
  MockTransaction trans(kTypicalGET_Transaction);
  RunTransactionTest(cache.http_cache(), trans);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Request the same URL, but this time with a reference section (hash).
  // Since the cache key strips the hash sections, this should be a cache hit.
  std::string url_with_hash = std::string(trans.url) + "#multiple#hashes";
  trans.url = url_with_hash.c_str();
  trans.load_flags = net::LOAD_ONLY_FROM_CACHE;

  RunTransactionTest(cache.http_cache(), trans);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that we skip the cache for POST requests that do not have an upload
// identifier.
TEST(HttpCache, SimplePOST_SkipsCache) {
  MockHttpCache cache;

  RunTransactionTest(cache.http_cache(), kSimplePOST_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimplePOST_LoadOnlyFromCache_Miss) {
  MockHttpCache cache;

  MockTransaction transaction(kSimplePOST_Transaction);
  transaction.load_flags |= net::LOAD_ONLY_FROM_CACHE;

  MockHttpRequest request(transaction);
  net::TestCompletionCallback callback;

  scoped_ptr<net::HttpTransaction> trans;
  ASSERT_EQ(net::OK, cache.CreateTransaction(&trans));
  ASSERT_TRUE(trans.get());

  int rv = trans->Start(&request, callback.callback(), net::BoundNetLog());
  ASSERT_EQ(net::ERR_CACHE_MISS, callback.GetResult(rv));

  trans.reset();

  EXPECT_EQ(0, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimplePOST_LoadOnlyFromCache_Hit) {
  MockHttpCache cache;

  // Test that we hit the cache for POST requests.

  MockTransaction transaction(kSimplePOST_Transaction);

  const int64 kUploadId = 1;  // Just a dummy value.

  ScopedVector<net::UploadElementReader> element_readers;
  element_readers.push_back(new net::UploadBytesElementReader("hello", 5));
  net::UploadDataStream upload_data_stream(element_readers.Pass(), kUploadId);
  MockHttpRequest request(transaction);
  request.upload_data_stream = &upload_data_stream;

  // Populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, request, NULL);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Load from cache.
  request.load_flags |= net::LOAD_ONLY_FROM_CACHE;
  RunTransactionTestWithRequest(cache.http_cache(), transaction, request, NULL);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Test that we don't hit the cache for POST requests if there is a byte range.
TEST(HttpCache, SimplePOST_WithRanges) {
  MockHttpCache cache;

  MockTransaction transaction(kSimplePOST_Transaction);
  transaction.request_headers = "Range: bytes = 0-4\r\n";

  const int64 kUploadId = 1;  // Just a dummy value.

  ScopedVector<net::UploadElementReader> element_readers;
  element_readers.push_back(new net::UploadBytesElementReader("hello", 5));
  net::UploadDataStream upload_data_stream(element_readers.Pass(), kUploadId);

  MockHttpRequest request(transaction);
  request.upload_data_stream = &upload_data_stream;

  // Attempt to populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, request, NULL);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

// Tests that a POST is cached separately from a previously cached GET.
TEST(HttpCache, SimplePOST_SeparateCache) {
  MockHttpCache cache;

  ScopedVector<net::UploadElementReader> element_readers;
  element_readers.push_back(new net::UploadBytesElementReader("hello", 5));
  net::UploadDataStream upload_data_stream(element_readers.Pass(), 1);

  MockTransaction transaction(kSimplePOST_Transaction);
  MockHttpRequest req1(transaction);
  req1.upload_data_stream = &upload_data_stream;

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, NULL);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  transaction.method = "GET";
  MockHttpRequest req2(transaction);

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req2, NULL);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that a successful POST invalidates a previously cached GET.
TEST(HttpCache, SimplePOST_Invalidate_205) {
  MockHttpCache cache;

  MockTransaction transaction(kSimpleGET_Transaction);
  AddMockTransaction(&transaction);
  MockHttpRequest req1(transaction);

  // Attempt to populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, NULL);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  ScopedVector<net::UploadElementReader> element_readers;
  element_readers.push_back(new net::UploadBytesElementReader("hello", 5));
  net::UploadDataStream upload_data_stream(element_readers.Pass(), 1);

  transaction.method = "POST";
  transaction.status = "HTTP/1.1 205 No Content";
  MockHttpRequest req2(transaction);
  req2.upload_data_stream = &upload_data_stream;

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req2, NULL);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, NULL);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(3, cache.disk_cache()->create_count());
  RemoveMockTransaction(&transaction);
}

// Tests that a successful POST invalidates a previously cached GET, even when
// there is no upload identifier.
TEST(HttpCache, SimplePOST_NoUploadId_Invalidate_205) {
  MockHttpCache cache;

  MockTransaction transaction(kSimpleGET_Transaction);
  AddMockTransaction(&transaction);
  MockHttpRequest req1(transaction);

  // Attempt to populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, NULL);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  ScopedVector<net::UploadElementReader> element_readers;
  element_readers.push_back(new net::UploadBytesElementReader("hello", 5));
  net::UploadDataStream upload_data_stream(element_readers.Pass(), 0);

  transaction.method = "POST";
  transaction.status = "HTTP/1.1 205 No Content";
  MockHttpRequest req2(transaction);
  req2.upload_data_stream = &upload_data_stream;

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req2, NULL);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, NULL);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
  RemoveMockTransaction(&transaction);
}

// Tests that processing a POST before creating the backend doesn't crash.
TEST(HttpCache, SimplePOST_NoUploadId_NoBackend) {
  // This will initialize a cache object with NULL backend.
  MockBlockingBackendFactory* factory = new MockBlockingBackendFactory();
  factory->set_fail(true);
  factory->FinishCreation();
  MockHttpCache cache(factory);

  ScopedVector<net::UploadElementReader> element_readers;
  element_readers.push_back(new net::UploadBytesElementReader("hello", 5));
  net::UploadDataStream upload_data_stream(element_readers.Pass(), 0);

  MockTransaction transaction(kSimplePOST_Transaction);
  AddMockTransaction(&transaction);
  MockHttpRequest req(transaction);
  req.upload_data_stream = &upload_data_stream;

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req, NULL);

  RemoveMockTransaction(&transaction);
}

// Tests that we don't invalidate entries as a result of a failed POST.
TEST(HttpCache, SimplePOST_DontInvalidate_100) {
  MockHttpCache cache;

  MockTransaction transaction(kSimpleGET_Transaction);
  AddMockTransaction(&transaction);
  MockHttpRequest req1(transaction);

  // Attempt to populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, NULL);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  ScopedVector<net::UploadElementReader> element_readers;
  element_readers.push_back(new net::UploadBytesElementReader("hello", 5));
  net::UploadDataStream upload_data_stream(element_readers.Pass(), 1);

  transaction.method = "POST";
  transaction.status = "HTTP/1.1 100 Continue";
  MockHttpRequest req2(transaction);
  req2.upload_data_stream = &upload_data_stream;

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req2, NULL);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, NULL);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
  RemoveMockTransaction(&transaction);
}

// Tests that we do not cache the response of a PUT.
TEST(HttpCache, SimplePUT_Miss) {
  MockHttpCache cache;

  MockTransaction transaction(kSimplePOST_Transaction);
  transaction.method = "PUT";

  ScopedVector<net::UploadElementReader> element_readers;
  element_readers.push_back(new net::UploadBytesElementReader("hello", 5));
  net::UploadDataStream upload_data_stream(element_readers.Pass(), 0);

  MockHttpRequest request(transaction);
  request.upload_data_stream = &upload_data_stream;

  // Attempt to populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, request, NULL);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

// Tests that we invalidate entries as a result of a PUT.
TEST(HttpCache, SimplePUT_Invalidate) {
  MockHttpCache cache;

  MockTransaction transaction(kSimpleGET_Transaction);
  MockHttpRequest req1(transaction);

  // Attempt to populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, NULL);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  ScopedVector<net::UploadElementReader> element_readers;
  element_readers.push_back(new net::UploadBytesElementReader("hello", 5));
  net::UploadDataStream upload_data_stream(element_readers.Pass(), 0);

  transaction.method = "PUT";
  MockHttpRequest req2(transaction);
  req2.upload_data_stream = &upload_data_stream;

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req2, NULL);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, NULL);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we invalidate entries as a result of a PUT.
TEST(HttpCache, SimplePUT_Invalidate_305) {
  MockHttpCache cache;

  MockTransaction transaction(kSimpleGET_Transaction);
  AddMockTransaction(&transaction);
  MockHttpRequest req1(transaction);

  // Attempt to populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, NULL);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  ScopedVector<net::UploadElementReader> element_readers;
  element_readers.push_back(new net::UploadBytesElementReader("hello", 5));
  net::UploadDataStream upload_data_stream(element_readers.Pass(), 0);

  transaction.method = "PUT";
  transaction.status = "HTTP/1.1 305 Use Proxy";
  MockHttpRequest req2(transaction);
  req2.upload_data_stream = &upload_data_stream;

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req2, NULL);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, NULL);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
  RemoveMockTransaction(&transaction);
}

// Tests that we don't invalidate entries as a result of a failed PUT.
TEST(HttpCache, SimplePUT_DontInvalidate_404) {
  MockHttpCache cache;

  MockTransaction transaction(kSimpleGET_Transaction);
  AddMockTransaction(&transaction);
  MockHttpRequest req1(transaction);

  // Attempt to populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, NULL);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  ScopedVector<net::UploadElementReader> element_readers;
  element_readers.push_back(new net::UploadBytesElementReader("hello", 5));
  net::UploadDataStream upload_data_stream(element_readers.Pass(), 0);

  transaction.method = "PUT";
  transaction.status = "HTTP/1.1 404 Not Found";
  MockHttpRequest req2(transaction);
  req2.upload_data_stream = &upload_data_stream;

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req2, NULL);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, NULL);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  RemoveMockTransaction(&transaction);
}

// Tests that we do not cache the response of a DELETE.
TEST(HttpCache, SimpleDELETE_Miss) {
  MockHttpCache cache;

  MockTransaction transaction(kSimplePOST_Transaction);
  transaction.method = "DELETE";

  ScopedVector<net::UploadElementReader> element_readers;
  element_readers.push_back(new net::UploadBytesElementReader("hello", 5));
  net::UploadDataStream upload_data_stream(element_readers.Pass(), 0);

  MockHttpRequest request(transaction);
  request.upload_data_stream = &upload_data_stream;

  // Attempt to populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, request, NULL);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

// Tests that we invalidate entries as a result of a DELETE.
TEST(HttpCache, SimpleDELETE_Invalidate) {
  MockHttpCache cache;

  MockTransaction transaction(kSimpleGET_Transaction);
  MockHttpRequest req1(transaction);

  // Attempt to populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, NULL);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  ScopedVector<net::UploadElementReader> element_readers;
  element_readers.push_back(new net::UploadBytesElementReader("hello", 5));
  net::UploadDataStream upload_data_stream(element_readers.Pass(), 0);

  transaction.method = "DELETE";
  MockHttpRequest req2(transaction);
  req2.upload_data_stream = &upload_data_stream;

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req2, NULL);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RunTransactionTestWithRequest(cache.http_cache(), transaction, req1, NULL);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we invalidate entries as a result of a DELETE.
TEST(HttpCache, SimpleDELETE_Invalidate_301) {
  MockHttpCache cache;

  MockTransaction transaction(kSimpleGET_Transaction);
  AddMockTransaction(&transaction);

  // Attempt to populate the cache.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  transaction.method = "DELETE";
  transaction.status = "HTTP/1.1 301 Moved Permanently ";

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  transaction.method = "GET";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
  RemoveMockTransaction(&transaction);
}

// Tests that we don't invalidate entries as a result of a failed DELETE.
TEST(HttpCache, SimpleDELETE_DontInvalidate_416) {
  MockHttpCache cache;

  MockTransaction transaction(kSimpleGET_Transaction);
  AddMockTransaction(&transaction);

  // Attempt to populate the cache.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  transaction.method = "DELETE";
  transaction.status = "HTTP/1.1 416 Requested Range Not Satisfiable";

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  transaction.method = "GET";
  transaction.status = "HTTP/1.1 200 OK";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  RemoveMockTransaction(&transaction);
}

// Tests that we don't invalidate entries after a failed network transaction.
TEST(HttpCache, SimpleGET_DontInvalidateOnFailure) {
  MockHttpCache cache;

  // Populate the cache.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());

  // Fail the network request.
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.return_code = net::ERR_FAILED;
  transaction.load_flags |= net::LOAD_VALIDATE_CACHE;

  AddMockTransaction(&transaction);
  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  RemoveMockTransaction(&transaction);

  transaction.load_flags = net::LOAD_ONLY_FROM_CACHE;
  transaction.return_code = net::OK;
  AddMockTransaction(&transaction);
  RunTransactionTest(cache.http_cache(), transaction);

  // Make sure the transaction didn't reach the network.
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  RemoveMockTransaction(&transaction);
}

TEST(HttpCache, RangeGET_SkipsCache) {
  MockHttpCache cache;

  // Test that we skip the cache for range GET requests.  Eventually, we will
  // want to cache these, but we'll still have cases where skipping the cache
  // makes sense, so we want to make sure that it works properly.

  RunTransactionTest(cache.http_cache(), kRangeGET_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());

  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "If-None-Match: foo\r\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());

  transaction.request_headers =
      "If-Modified-Since: Wed, 28 Nov 2007 00:45:20 GMT\r\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

// Test that we skip the cache for range requests that include a validation
// header.
TEST(HttpCache, RangeGET_SkipsCache2) {
  MockHttpCache cache;

  MockTransaction transaction(kRangeGET_Transaction);
  transaction.request_headers = "If-None-Match: foo\r\n"
                                EXTRA_HEADER
                                "Range: bytes = 40-49\r\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());

  transaction.request_headers =
      "If-Modified-Since: Wed, 28 Nov 2007 00:45:20 GMT\r\n"
      EXTRA_HEADER
      "Range: bytes = 40-49\r\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());

  transaction.request_headers = "If-Range: bla\r\n"
                                EXTRA_HEADER
                                "Range: bytes = 40-49\r\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

// Tests that receiving 206 for a regular request is handled correctly.
TEST(HttpCache, GET_Crazy206) {
  MockHttpCache cache;

  // Write to the cache.
  MockTransaction transaction(kRangeGET_TransactionOK);
  AddMockTransaction(&transaction);
  transaction.request_headers = EXTRA_HEADER;
  transaction.handler = NULL;
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // This should read again from the net.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
  RemoveMockTransaction(&transaction);
}

// Tests that receiving 416 for a regular request is handled correctly.
TEST(HttpCache, GET_Crazy416) {
  MockHttpCache cache;

  // Write to the cache.
  MockTransaction transaction(kSimpleGET_Transaction);
  AddMockTransaction(&transaction);
  transaction.status = "HTTP/1.1 416 Requested Range Not Satisfiable";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&transaction);
}

// Tests that we don't cache partial responses that can't be validated.
TEST(HttpCache, RangeGET_NoStrongValidators) {
  MockHttpCache cache;
  std::string headers;

  // Attempt to write to the cache (40-49).
  MockTransaction transaction(kRangeGET_TransactionOK);
  AddMockTransaction(&transaction);
  transaction.response_headers = "Content-Length: 10\n"
                                 "ETag: w/\"foo\"\n";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Now verify that there's no cached data.
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  RemoveMockTransaction(&transaction);
}

// Tests that we cache partial responses that lack content-length.
TEST(HttpCache, RangeGET_NoContentLength) {
  MockHttpCache cache;
  std::string headers;

  // Attempt to write to the cache (40-49).
  MockTransaction transaction(kRangeGET_TransactionOK);
  AddMockTransaction(&transaction);
  transaction.response_headers = "ETag: \"foo\"\n"
                                 "Accept-Ranges: bytes\n"
                                 "Content-Range: bytes 40-49/80\n";
  transaction.handler = NULL;
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Now verify that there's no cached data.
  transaction.handler = &RangeTransactionServer::RangeHandler;
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&transaction);
}

// Tests that we can cache range requests and fetch random blocks from the
// cache and the network.
TEST(HttpCache, RangeGET_OK) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);
  std::string headers;

  // Write to the cache (40-49).
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Read from the cache (40-49).
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure we are done with the previous transaction.
  base::MessageLoop::current()->RunUntilIdle();

  // Write to the cache (30-39).
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 30-39\r\n" EXTRA_HEADER;
  transaction.data = "rg: 30-39 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 30, 39);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure we are done with the previous transaction.
  base::MessageLoop::current()->RunUntilIdle();

  // Write and read from the cache (20-59).
  transaction.request_headers = "Range: bytes = 20-59\r\n" EXTRA_HEADER;
  transaction.data = "rg: 20-29 rg: 30-39 rg: 40-49 rg: 50-59 ";
  net::CapturingBoundNetLog log;
  net::LoadTimingInfo load_timing_info;
  RunTransactionTestWithResponseAndGetTiming(
      cache.http_cache(), transaction, &headers, log.bound(),
      &load_timing_info);

  Verify206Response(headers, 20, 59);
  EXPECT_EQ(4, cache.network_layer()->transaction_count());
  EXPECT_EQ(3, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Checks that with a cache backend having Sparse IO unimplementes the cache
// entry would be doomed after a range request.
// TODO(pasko): remove when the SimpleBackendImpl implements Sparse IO.
TEST(HttpCache, RangeGET_SparseNotImplemented) {
  MockHttpCache cache;
  cache.disk_cache()->set_fail_sparse_requests();

  // Run a cacheable request to prime the cache.
  MockTransaction transaction(kTypicalGET_Transaction);
  transaction.url = kRangeGET_TransactionOK.url;
  AddMockTransaction(&transaction);
  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Verify that we added the entry.
  disk_cache::Entry* entry;
  net::TestCompletionCallback cb;
  int rv = cache.disk_cache()->OpenEntry(transaction.url,
                                         &entry,
                                         cb.callback());
  ASSERT_EQ(net::OK, cb.GetResult(rv));
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  entry->Close();
  RemoveMockTransaction(&transaction);

  // Request the range with the backend that does not support it.
  MockTransaction transaction2(kRangeGET_TransactionOK);
  std::string headers;
  AddMockTransaction(&transaction2);
  RunTransactionTestWithResponse(cache.http_cache(), transaction2, &headers);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  // Mock cache would return net::ERR_CACHE_OPEN_FAILURE on a doomed entry, even
  // if it was re-created later, so this effectively checks that the old data is
  // gone.
  disk_cache::Entry* entry2;
  rv = cache.disk_cache()->OpenEntry(transaction2.url,
                                     &entry2,
                                     cb.callback());
  ASSERT_EQ(net::ERR_CACHE_OPEN_FAILURE, cb.GetResult(rv));
  RemoveMockTransaction(&transaction2);
}

TEST(HttpCache, RangeGET_SparseNotImplementedOnEmptyCache) {
  MockHttpCache cache;
  cache.disk_cache()->set_fail_sparse_requests();

  // Request the range with the backend that does not support it.
  MockTransaction transaction(kRangeGET_TransactionOK);
  std::string headers;
  AddMockTransaction(&transaction);
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Mock cache would return net::ERR_CACHE_OPEN_FAILURE on a doomed entry, even
  // if it was re-created later, so this effectively checks that the old data is
  // gone as a result of a failed range write.
  disk_cache::Entry* entry;
  net::TestCompletionCallback cb;
  int rv = cache.disk_cache()->OpenEntry(transaction.url,
                                         &entry,
                                         cb.callback());
  ASSERT_EQ(net::ERR_CACHE_OPEN_FAILURE, cb.GetResult(rv));
  RemoveMockTransaction(&transaction);
}

// Tests that we can cache range requests and fetch random blocks from the
// cache and the network, with synchronous responses.
TEST(HttpCache, RangeGET_SyncOK) {
  MockHttpCache cache;

  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.test_mode = TEST_MODE_SYNC_ALL;
  AddMockTransaction(&transaction);

  // Write to the cache (40-49).
  std::string headers;
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Read from the cache (40-49).
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure we are done with the previous transaction.
  base::MessageLoop::current()->RunUntilIdle();

  // Write to the cache (30-39).
  transaction.request_headers = "Range: bytes = 30-39\r\n" EXTRA_HEADER;
  transaction.data = "rg: 30-39 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 30, 39);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure we are done with the previous transaction.
  base::MessageLoop::current()->RunUntilIdle();

  // Write and read from the cache (20-59).
  transaction.request_headers = "Range: bytes = 20-59\r\n" EXTRA_HEADER;
  transaction.data = "rg: 20-29 rg: 30-39 rg: 40-49 rg: 50-59 ";
  net::CapturingBoundNetLog log;
  net::LoadTimingInfo load_timing_info;
  RunTransactionTestWithResponseAndGetTiming(
      cache.http_cache(), transaction, &headers, log.bound(),
      &load_timing_info);

  Verify206Response(headers, 20, 59);
  EXPECT_EQ(4, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);

  RemoveMockTransaction(&transaction);
}

// Tests that we don't revalidate an entry unless we are required to do so.
TEST(HttpCache, RangeGET_Revalidate1) {
  MockHttpCache cache;
  std::string headers;

  // Write to the cache (40-49).
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.response_headers =
      "Last-Modified: Sat, 18 Apr 2009 01:10:43 GMT\n"
      "Expires: Wed, 7 Sep 2033 21:46:42 GMT\n"  // Should never expire.
      "ETag: \"foo\"\n"
      "Accept-Ranges: bytes\n"
      "Content-Length: 10\n";
  AddMockTransaction(&transaction);
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Read from the cache (40-49).
  net::CapturingBoundNetLog log;
  net::LoadTimingInfo load_timing_info;
  RunTransactionTestWithResponseAndGetTiming(
      cache.http_cache(), transaction, &headers, log.bound(),
      &load_timing_info);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingCachedResponse(load_timing_info);

  // Read again forcing the revalidation.
  transaction.load_flags |= net::LOAD_VALIDATE_CACHE;
  RunTransactionTestWithResponseAndGetTiming(
      cache.http_cache(), transaction, &headers, log.bound(),
      &load_timing_info);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);

  RemoveMockTransaction(&transaction);
}

// Checks that we revalidate an entry when the headers say so.
TEST(HttpCache, RangeGET_Revalidate2) {
  MockHttpCache cache;
  std::string headers;

  // Write to the cache (40-49).
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.response_headers =
      "Last-Modified: Sat, 18 Apr 2009 01:10:43 GMT\n"
      "Expires: Sat, 18 Apr 2009 01:10:43 GMT\n"  // Expired.
      "ETag: \"foo\"\n"
      "Accept-Ranges: bytes\n"
      "Content-Length: 10\n";
  AddMockTransaction(&transaction);
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Read from the cache (40-49).
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);
  Verify206Response(headers, 40, 49);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&transaction);
}

// Tests that we deal with 304s for range requests.
TEST(HttpCache, RangeGET_304) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);
  std::string headers;

  // Write to the cache (40-49).
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Read from the cache (40-49).
  RangeTransactionServer handler;
  handler.set_not_modified(true);
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.load_flags |= net::LOAD_VALIDATE_CACHE;
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we deal with 206s when revalidating range requests.
TEST(HttpCache, RangeGET_ModifiedResult) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);
  std::string headers;

  // Write to the cache (40-49).
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Attempt to read from the cache (40-49).
  RangeTransactionServer handler;
  handler.set_modified(true);
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.load_flags |= net::LOAD_VALIDATE_CACHE;
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // And the entry should be gone.
  RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);
  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we cache 301s for range requests.
TEST(HttpCache, RangeGET_301) {
  MockHttpCache cache;
  ScopedMockTransaction transaction(kRangeGET_TransactionOK);
  transaction.status = "HTTP/1.1 301 Moved Permanently";
  transaction.response_headers = "Location: http://www.bar.com/\n";
  transaction.data = "";
  transaction.handler = NULL;
  AddMockTransaction(&transaction);

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Read from the cache.
  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&transaction);
}

// Tests that we can cache range requests when the start or end is unknown.
// We start with one suffix request, followed by a request from a given point.
TEST(HttpCache, UnknownRangeGET_1) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);
  std::string headers;

  // Write to the cache (70-79).
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = -10\r\n" EXTRA_HEADER;
  transaction.data = "rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 70, 79);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure we are done with the previous transaction.
  base::MessageLoop::current()->RunUntilIdle();

  // Write and read from the cache (60-79).
  transaction.request_headers = "Range: bytes = 60-\r\n" EXTRA_HEADER;
  transaction.data = "rg: 60-69 rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 60, 79);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we can cache range requests when the start or end is unknown.
// We start with one request from a given point, followed by a suffix request.
// We'll also verify that synchronous cache responses work as intended.
TEST(HttpCache, UnknownRangeGET_2) {
  MockHttpCache cache;
  std::string headers;

  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.test_mode = TEST_MODE_SYNC_CACHE_START |
                          TEST_MODE_SYNC_CACHE_READ |
                          TEST_MODE_SYNC_CACHE_WRITE;
  AddMockTransaction(&transaction);

  // Write to the cache (70-79).
  transaction.request_headers = "Range: bytes = 70-\r\n" EXTRA_HEADER;
  transaction.data = "rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 70, 79);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure we are done with the previous transaction.
  base::MessageLoop::current()->RunUntilIdle();

  // Write and read from the cache (60-79).
  transaction.request_headers = "Range: bytes = -20\r\n" EXTRA_HEADER;
  transaction.data = "rg: 60-69 rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 60, 79);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&transaction);
}

// Tests that receiving Not Modified when asking for an open range doesn't mess
// up things.
TEST(HttpCache, UnknownRangeGET_304) {
  MockHttpCache cache;
  std::string headers;

  MockTransaction transaction(kRangeGET_TransactionOK);
  AddMockTransaction(&transaction);

  RangeTransactionServer handler;
  handler.set_not_modified(true);

  // Ask for the end of the file, without knowing the length.
  transaction.request_headers = "Range: bytes = 70-\r\n" EXTRA_HEADER;
  transaction.data = "";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  // We just bypass the cache.
  EXPECT_EQ(0U, headers.find("HTTP/1.1 304 Not Modified\n"));
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  RemoveMockTransaction(&transaction);
}

// Tests that we can handle non-range requests when we have cached a range.
TEST(HttpCache, GET_Previous206) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);
  std::string headers;
  net::CapturingBoundNetLog log;
  net::LoadTimingInfo load_timing_info;

  // Write to the cache (40-49).
  RunTransactionTestWithResponseAndGetTiming(
      cache.http_cache(), kRangeGET_TransactionOK, &headers, log.bound(),
      &load_timing_info);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);

  // Write and read from the cache (0-79), when not asked for a range.
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = EXTRA_HEADER;
  transaction.data = "rg: 00-09 rg: 10-19 rg: 20-29 rg: 30-39 rg: 40-49 "
                     "rg: 50-59 rg: 60-69 rg: 70-79 ";
  RunTransactionTestWithResponseAndGetTiming(
      cache.http_cache(), transaction, &headers, log.bound(),
      &load_timing_info);

  EXPECT_EQ(0U, headers.find("HTTP/1.1 200 OK\n"));
  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we can handle non-range requests when we have cached the first
// part of the object and the server replies with 304 (Not Modified).
TEST(HttpCache, GET_Previous206_NotModified) {
  MockHttpCache cache;

  MockTransaction transaction(kRangeGET_TransactionOK);
  AddMockTransaction(&transaction);
  std::string headers;
  net::CapturingBoundNetLog log;
  net::LoadTimingInfo load_timing_info;

  // Write to the cache (0-9).
  transaction.request_headers = "Range: bytes = 0-9\r\n" EXTRA_HEADER;
  transaction.data = "rg: 00-09 ";
  RunTransactionTestWithResponseAndGetTiming(
      cache.http_cache(), transaction, &headers, log.bound(),
      &load_timing_info);
  Verify206Response(headers, 0, 9);
  TestLoadTimingNetworkRequest(load_timing_info);

  // Write to the cache (70-79).
  transaction.request_headers = "Range: bytes = 70-79\r\n" EXTRA_HEADER;
  transaction.data = "rg: 70-79 ";
  RunTransactionTestWithResponseAndGetTiming(
      cache.http_cache(), transaction, &headers, log.bound(),
      &load_timing_info);
  Verify206Response(headers, 70, 79);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);

  // Read from the cache (0-9), write and read from cache (10 - 79).
  transaction.load_flags |= net::LOAD_VALIDATE_CACHE;
  transaction.request_headers = "Foo: bar\r\n" EXTRA_HEADER;
  transaction.data = "rg: 00-09 rg: 10-19 rg: 20-29 rg: 30-39 rg: 40-49 "
                     "rg: 50-59 rg: 60-69 rg: 70-79 ";
  RunTransactionTestWithResponseAndGetTiming(
      cache.http_cache(), transaction, &headers, log.bound(),
      &load_timing_info);

  EXPECT_EQ(0U, headers.find("HTTP/1.1 200 OK\n"));
  EXPECT_EQ(4, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);

  RemoveMockTransaction(&transaction);
}

// Tests that we can handle a regular request to a sparse entry, that results in
// new content provided by the server (206).
TEST(HttpCache, GET_Previous206_NewContent) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);
  std::string headers;

  // Write to the cache (0-9).
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 0-9\r\n" EXTRA_HEADER;
  transaction.data = "rg: 00-09 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 0, 9);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Now we'll issue a request without any range that should result first in a
  // 206 (when revalidating), and then in a weird standard answer: the test
  // server will not modify the response so we'll get the default range... a
  // real server will answer with 200.
  MockTransaction transaction2(kRangeGET_TransactionOK);
  transaction2.request_headers = EXTRA_HEADER;
  transaction2.load_flags |= net::LOAD_VALIDATE_CACHE;
  transaction2.data = "Not a range";
  RangeTransactionServer handler;
  handler.set_modified(true);
  net::CapturingBoundNetLog log;
  net::LoadTimingInfo load_timing_info;
  RunTransactionTestWithResponseAndGetTiming(
      cache.http_cache(), transaction2, &headers, log.bound(),
      &load_timing_info);

  EXPECT_EQ(0U, headers.find("HTTP/1.1 200 OK\n"));
  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);

  // Verify that the previous request deleted the entry.
  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  RemoveMockTransaction(&transaction);
}

// Tests that we can handle cached 206 responses that are not sparse.
TEST(HttpCache, GET_Previous206_NotSparse) {
  MockHttpCache cache;

  // Create a disk cache entry that stores 206 headers while not being sparse.
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.CreateBackendEntry(kSimpleGET_Transaction.url, &entry,
                                       NULL));

  std::string raw_headers(kRangeGET_TransactionOK.status);
  raw_headers.append("\n");
  raw_headers.append(kRangeGET_TransactionOK.response_headers);
  raw_headers = net::HttpUtil::AssembleRawHeaders(raw_headers.data(),
                                                  raw_headers.size());

  net::HttpResponseInfo response;
  response.headers = new net::HttpResponseHeaders(raw_headers);
  EXPECT_TRUE(MockHttpCache::WriteResponseInfo(entry, &response, true, false));

  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(500));
  int len = static_cast<int>(base::strlcpy(buf->data(),
                                           kRangeGET_TransactionOK.data, 500));
  net::TestCompletionCallback cb;
  int rv = entry->WriteData(1, 0, buf.get(), len, cb.callback(), true);
  EXPECT_EQ(len, cb.GetResult(rv));
  entry->Close();

  // Now see that we don't use the stored entry.
  std::string headers;
  net::CapturingBoundNetLog log;
  net::LoadTimingInfo load_timing_info;
  RunTransactionTestWithResponseAndGetTiming(
      cache.http_cache(), kSimpleGET_Transaction, &headers, log.bound(),
      &load_timing_info);

  // We are expecting a 200.
  std::string expected_headers(kSimpleGET_Transaction.status);
  expected_headers.append("\n");
  expected_headers.append(kSimpleGET_Transaction.response_headers);
  EXPECT_EQ(expected_headers, headers);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
  TestLoadTimingNetworkRequest(load_timing_info);
}

// Tests that we can handle cached 206 responses that are not sparse. This time
// we issue a range request and expect to receive a range.
TEST(HttpCache, RangeGET_Previous206_NotSparse_2) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);

  // Create a disk cache entry that stores 206 headers while not being sparse.
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.CreateBackendEntry(kRangeGET_TransactionOK.url, &entry,
                                       NULL));

  std::string raw_headers(kRangeGET_TransactionOK.status);
  raw_headers.append("\n");
  raw_headers.append(kRangeGET_TransactionOK.response_headers);
  raw_headers = net::HttpUtil::AssembleRawHeaders(raw_headers.data(),
                                                  raw_headers.size());

  net::HttpResponseInfo response;
  response.headers = new net::HttpResponseHeaders(raw_headers);
  EXPECT_TRUE(MockHttpCache::WriteResponseInfo(entry, &response, true, false));

  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(500));
  int len = static_cast<int>(base::strlcpy(buf->data(),
                                           kRangeGET_TransactionOK.data, 500));
  net::TestCompletionCallback cb;
  int rv = entry->WriteData(1, 0, buf.get(), len, cb.callback(), true);
  EXPECT_EQ(len, cb.GetResult(rv));
  entry->Close();

  // Now see that we don't use the stored entry.
  std::string headers;
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  // We are expecting a 206.
  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we can handle cached 206 responses that can't be validated.
TEST(HttpCache, GET_Previous206_NotValidation) {
  MockHttpCache cache;

  // Create a disk cache entry that stores 206 headers.
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.CreateBackendEntry(kSimpleGET_Transaction.url, &entry,
                                       NULL));

  // Make sure that the headers cannot be validated with the server.
  std::string raw_headers(kRangeGET_TransactionOK.status);
  raw_headers.append("\n");
  raw_headers.append("Content-Length: 80\n");
  raw_headers = net::HttpUtil::AssembleRawHeaders(raw_headers.data(),
                                                  raw_headers.size());

  net::HttpResponseInfo response;
  response.headers = new net::HttpResponseHeaders(raw_headers);
  EXPECT_TRUE(MockHttpCache::WriteResponseInfo(entry, &response, true, false));

  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(500));
  int len = static_cast<int>(base::strlcpy(buf->data(),
                                           kRangeGET_TransactionOK.data, 500));
  net::TestCompletionCallback cb;
  int rv = entry->WriteData(1, 0, buf.get(), len, cb.callback(), true);
  EXPECT_EQ(len, cb.GetResult(rv));
  entry->Close();

  // Now see that we don't use the stored entry.
  std::string headers;
  RunTransactionTestWithResponse(cache.http_cache(), kSimpleGET_Transaction,
                                 &headers);

  // We are expecting a 200.
  std::string expected_headers(kSimpleGET_Transaction.status);
  expected_headers.append("\n");
  expected_headers.append(kSimpleGET_Transaction.response_headers);
  EXPECT_EQ(expected_headers, headers);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we can handle range requests with cached 200 responses.
TEST(HttpCache, RangeGET_Previous200) {
  MockHttpCache cache;

  // Store the whole thing with status 200.
  MockTransaction transaction(kTypicalGET_Transaction);
  transaction.url = kRangeGET_TransactionOK.url;
  transaction.data = "rg: 00-09 rg: 10-19 rg: 20-29 rg: 30-39 rg: 40-49 "
                     "rg: 50-59 rg: 60-69 rg: 70-79 ";
  AddMockTransaction(&transaction);
  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&transaction);
  AddMockTransaction(&kRangeGET_TransactionOK);

  // Now see that we use the stored entry.
  std::string headers;
  MockTransaction transaction2(kRangeGET_TransactionOK);
  RangeTransactionServer handler;
  handler.set_not_modified(true);
  RunTransactionTestWithResponse(cache.http_cache(), transaction2, &headers);

  // We are expecting a 206.
  Verify206Response(headers, 40, 49);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // The last transaction has finished so make sure the entry is deactivated.
  base::MessageLoop::current()->RunUntilIdle();

  // Make a request for an invalid range.
  MockTransaction transaction3(kRangeGET_TransactionOK);
  transaction3.request_headers = "Range: bytes = 80-90\r\n" EXTRA_HEADER;
  transaction3.data = transaction.data;
  transaction3.load_flags = net::LOAD_PREFERRING_CACHE;
  RunTransactionTestWithResponse(cache.http_cache(), transaction3, &headers);
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(0U, headers.find("HTTP/1.1 200 "));
  EXPECT_EQ(std::string::npos, headers.find("Content-Range:"));
  EXPECT_EQ(std::string::npos, headers.find("Content-Length: 80"));

  // Make sure the entry is deactivated.
  base::MessageLoop::current()->RunUntilIdle();

  // Even though the request was invalid, we should have the entry.
  RunTransactionTest(cache.http_cache(), transaction2);
  EXPECT_EQ(3, cache.disk_cache()->open_count());

  // Make sure the entry is deactivated.
  base::MessageLoop::current()->RunUntilIdle();

  // Now we should receive a range from the server and drop the stored entry.
  handler.set_not_modified(false);
  transaction2.request_headers = kRangeGET_TransactionOK.request_headers;
  RunTransactionTestWithResponse(cache.http_cache(), transaction2, &headers);
  Verify206Response(headers, 40, 49);
  EXPECT_EQ(4, cache.network_layer()->transaction_count());
  EXPECT_EQ(4, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RunTransactionTest(cache.http_cache(), transaction2);
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we can handle a 200 response when dealing with sparse entries.
TEST(HttpCache, RangeRequestResultsIn200) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);
  std::string headers;

  // Write to the cache (70-79).
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = -10\r\n" EXTRA_HEADER;
  transaction.data = "rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 70, 79);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Now we'll issue a request that results in a plain 200 response, but to
  // the to the same URL that we used to store sparse data, and making sure
  // that we ask for a range.
  RemoveMockTransaction(&kRangeGET_TransactionOK);
  MockTransaction transaction2(kSimpleGET_Transaction);
  transaction2.url = kRangeGET_TransactionOK.url;
  transaction2.request_headers = kRangeGET_TransactionOK.request_headers;
  AddMockTransaction(&transaction2);

  RunTransactionTestWithResponse(cache.http_cache(), transaction2, &headers);

  std::string expected_headers(kSimpleGET_Transaction.status);
  expected_headers.append("\n");
  expected_headers.append(kSimpleGET_Transaction.response_headers);
  EXPECT_EQ(expected_headers, headers);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&transaction2);
}

// Tests that a range request that falls outside of the size that we know about
// only deletes the entry if the resource has indeed changed.
TEST(HttpCache, RangeGET_MoreThanCurrentSize) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);
  std::string headers;

  // Write to the cache (40-49).
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // A weird request should not delete this entry. Ask for bytes 120-.
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 120-\r\n" EXTRA_HEADER;
  transaction.data = "";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_EQ(0U, headers.find("HTTP/1.1 416 "));
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we don't delete a sparse entry when we cancel a request.
TEST(HttpCache, RangeGET_Cancel) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);

  MockHttpRequest request(kRangeGET_TransactionOK);

  Context* c = new Context();
  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_EQ(net::OK, rv);

  rv = c->trans->Start(&request, c->callback.callback(), net::BoundNetLog());
  if (rv == net::ERR_IO_PENDING)
    rv = c->callback.WaitForResult();

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure that the entry has some data stored.
  scoped_refptr<net::IOBufferWithSize> buf(new net::IOBufferWithSize(10));
  rv = c->trans->Read(buf.get(), buf->size(), c->callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = c->callback.WaitForResult();
  EXPECT_EQ(buf->size(), rv);

  // Destroy the transaction.
  delete c;

  // Verify that the entry has not been deleted.
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.OpenBackendEntry(kRangeGET_TransactionOK.url, &entry));
  entry->Close();
  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we don't delete a sparse entry when we start a new request after
// cancelling the previous one.
TEST(HttpCache, RangeGET_Cancel2) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);

  RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);
  MockHttpRequest request(kRangeGET_TransactionOK);
  request.load_flags |= net::LOAD_VALIDATE_CACHE;

  Context* c = new Context();
  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_EQ(net::OK, rv);

  rv = c->trans->Start(&request, c->callback.callback(), net::BoundNetLog());
  if (rv == net::ERR_IO_PENDING)
    rv = c->callback.WaitForResult();

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure that we revalidate the entry and read from the cache (a single
  // read will return while waiting for the network).
  scoped_refptr<net::IOBufferWithSize> buf(new net::IOBufferWithSize(5));
  rv = c->trans->Read(buf.get(), buf->size(), c->callback.callback());
  EXPECT_EQ(5, c->callback.GetResult(rv));
  rv = c->trans->Read(buf.get(), buf->size(), c->callback.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  // Destroy the transaction before completing the read.
  delete c;

  // We have the read and the delete (OnProcessPendingQueue) waiting on the
  // message loop. This means that a new transaction will just reuse the same
  // active entry (no open or create).

  RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// A slight variation of the previous test, this time we cancel two requests in
// a row, making sure that the second is waiting for the entry to be ready.
TEST(HttpCache, RangeGET_Cancel3) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);

  RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);
  MockHttpRequest request(kRangeGET_TransactionOK);
  request.load_flags |= net::LOAD_VALIDATE_CACHE;

  Context* c = new Context();
  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_EQ(net::OK, rv);

  rv = c->trans->Start(&request, c->callback.callback(), net::BoundNetLog());
  EXPECT_EQ(net::ERR_IO_PENDING, rv);
  rv = c->callback.WaitForResult();

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure that we revalidate the entry and read from the cache (a single
  // read will return while waiting for the network).
  scoped_refptr<net::IOBufferWithSize> buf(new net::IOBufferWithSize(5));
  rv = c->trans->Read(buf.get(), buf->size(), c->callback.callback());
  EXPECT_EQ(5, c->callback.GetResult(rv));
  rv = c->trans->Read(buf.get(), buf->size(), c->callback.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  // Destroy the transaction before completing the read.
  delete c;

  // We have the read and the delete (OnProcessPendingQueue) waiting on the
  // message loop. This means that a new transaction will just reuse the same
  // active entry (no open or create).

  c = new Context();
  rv = cache.CreateTransaction(&c->trans);
  ASSERT_EQ(net::OK, rv);

  rv = c->trans->Start(&request, c->callback.callback(), net::BoundNetLog());
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  MockDiskEntry::IgnoreCallbacks(true);
  base::MessageLoop::current()->RunUntilIdle();
  MockDiskEntry::IgnoreCallbacks(false);

  // The new transaction is waiting for the query range callback.
  delete c;

  // And we should not crash when the callback is delivered.
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that an invalid range response results in no cached entry.
TEST(HttpCache, RangeGET_InvalidResponse1) {
  MockHttpCache cache;
  std::string headers;

  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.handler = NULL;
  transaction.response_headers = "Content-Range: bytes 40-49/45\n"
                                 "Content-Length: 10\n";
  AddMockTransaction(&transaction);
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  std::string expected(transaction.status);
  expected.append("\n");
  expected.append(transaction.response_headers);
  EXPECT_EQ(expected, headers);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Verify that we don't have a cached entry.
  disk_cache::Entry* entry;
  EXPECT_FALSE(cache.OpenBackendEntry(kRangeGET_TransactionOK.url, &entry));

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we reject a range that doesn't match the content-length.
TEST(HttpCache, RangeGET_InvalidResponse2) {
  MockHttpCache cache;
  std::string headers;

  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.handler = NULL;
  transaction.response_headers = "Content-Range: bytes 40-49/80\n"
                                 "Content-Length: 20\n";
  AddMockTransaction(&transaction);
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  std::string expected(transaction.status);
  expected.append("\n");
  expected.append(transaction.response_headers);
  EXPECT_EQ(expected, headers);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Verify that we don't have a cached entry.
  disk_cache::Entry* entry;
  EXPECT_FALSE(cache.OpenBackendEntry(kRangeGET_TransactionOK.url, &entry));

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that if a server tells us conflicting information about a resource we
// ignore the response.
TEST(HttpCache, RangeGET_InvalidResponse3) {
  MockHttpCache cache;
  std::string headers;

  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.handler = NULL;
  transaction.request_headers = "Range: bytes = 50-59\r\n" EXTRA_HEADER;
  std::string response_headers(transaction.response_headers);
  response_headers.append("Content-Range: bytes 50-59/160\n");
  transaction.response_headers = response_headers.c_str();
  AddMockTransaction(&transaction);
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 50, 59);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&transaction);
  AddMockTransaction(&kRangeGET_TransactionOK);

  // This transaction will report a resource size of 80 bytes, and we think it's
  // 160 so we should ignore the response.
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Verify that we cached the first response but not the second one.
  disk_cache::Entry* en;
  ASSERT_TRUE(cache.OpenBackendEntry(kRangeGET_TransactionOK.url, &en));

  int64 cached_start = 0;
  net::TestCompletionCallback cb;
  int rv = en->GetAvailableRange(40, 20, &cached_start, cb.callback());
  EXPECT_EQ(10, cb.GetResult(rv));
  EXPECT_EQ(50, cached_start);
  en->Close();

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we handle large range values properly.
TEST(HttpCache, RangeGET_LargeValues) {
  // We need a real sparse cache for this test.
  MockHttpCache cache(net::HttpCache::DefaultBackend::InMemory(1024 * 1024));
  std::string headers;

  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.handler = NULL;
  transaction.request_headers = "Range: bytes = 4294967288-4294967297\r\n"
                                EXTRA_HEADER;
  transaction.response_headers =
      "ETag: \"foo\"\n"
      "Content-Range: bytes 4294967288-4294967297/4294967299\n"
      "Content-Length: 10\n";
  AddMockTransaction(&transaction);
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  std::string expected(transaction.status);
  expected.append("\n");
  expected.append(transaction.response_headers);
  EXPECT_EQ(expected, headers);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());

  // Verify that we have a cached entry.
  disk_cache::Entry* en;
  ASSERT_TRUE(cache.OpenBackendEntry(kRangeGET_TransactionOK.url, &en));
  en->Close();

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we don't crash with a range request if the disk cache was not
// initialized properly.
TEST(HttpCache, RangeGET_NoDiskCache) {
  MockBlockingBackendFactory* factory = new MockBlockingBackendFactory();
  factory->set_fail(true);
  factory->FinishCreation();  // We'll complete synchronously.
  MockHttpCache cache(factory);

  AddMockTransaction(&kRangeGET_TransactionOK);

  RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we handle byte range requests that skip the cache.
TEST(HttpCache, RangeHEAD) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);

  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = -10\r\n" EXTRA_HEADER;
  transaction.method = "HEAD";
  transaction.data = "rg: 70-79 ";

  std::string headers;
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  Verify206Response(headers, 70, 79);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we don't crash when after reading from the cache we issue a
// request for the next range and the server gives us a 200 synchronously.
TEST(HttpCache, RangeGET_FastFlakyServer) {
  MockHttpCache cache;

  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 40-\r\n" EXTRA_HEADER;
  transaction.test_mode = TEST_MODE_SYNC_NET_START;
  transaction.load_flags |= net::LOAD_VALIDATE_CACHE;
  AddMockTransaction(&transaction);

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);

  // And now read from the cache and the network.
  RangeTransactionServer handler;
  handler.set_bad_200(true);
  transaction.data = "Not a range";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&transaction);
}

// Tests that when the server gives us less data than expected, we don't keep
// asking for more data.
TEST(HttpCache, RangeGET_FastFlakyServer2) {
  MockHttpCache cache;

  // First, check with an empty cache (WRITE mode).
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 40-49\r\n" EXTRA_HEADER;
  transaction.data = "rg: 40-";  // Less than expected.
  transaction.handler = NULL;
  std::string headers(transaction.response_headers);
  headers.append("Content-Range: bytes 40-49/80\n");
  transaction.response_headers = headers.c_str();

  AddMockTransaction(&transaction);

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Now verify that even in READ_WRITE mode, we forward the bad response to
  // the caller.
  transaction.request_headers = "Range: bytes = 60-69\r\n" EXTRA_HEADER;
  transaction.data = "rg: 60-";  // Less than expected.
  headers = kRangeGET_TransactionOK.response_headers;
  headers.append("Content-Range: bytes 60-69/80\n");
  transaction.response_headers = headers.c_str();

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&transaction);
}

#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
// This test hits a NOTREACHED so it is a release mode only test.
TEST(HttpCache, RangeGET_OK_LoadOnlyFromCache) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);

  // Write to the cache (40-49).
  RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Force this transaction to read from the cache.
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.load_flags |= net::LOAD_ONLY_FROM_CACHE;

  MockHttpRequest request(transaction);
  net::TestCompletionCallback callback;

  scoped_ptr<net::HttpTransaction> trans;
  int rv = cache.http_cache()->CreateTransaction(net::DEFAULT_PRIORITY, &trans);
  EXPECT_EQ(net::OK, rv);
  ASSERT_TRUE(trans.get());

  rv = trans->Start(&request, callback.callback(), net::BoundNetLog());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(net::ERR_CACHE_MISS, rv);

  trans.reset();

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}
#endif

// Tests the handling of the "truncation" flag.
TEST(HttpCache, WriteResponseInfo_Truncated) {
  MockHttpCache cache;
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.CreateBackendEntry("http://www.google.com", &entry,
                                       NULL));

  std::string headers("HTTP/1.1 200 OK");
  headers = net::HttpUtil::AssembleRawHeaders(headers.data(), headers.size());
  net::HttpResponseInfo response;
  response.headers = new net::HttpResponseHeaders(headers);

  // Set the last argument for this to be an incomplete request.
  EXPECT_TRUE(MockHttpCache::WriteResponseInfo(entry, &response, true, true));
  bool truncated = false;
  EXPECT_TRUE(MockHttpCache::ReadResponseInfo(entry, &response, &truncated));
  EXPECT_TRUE(truncated);

  // And now test the opposite case.
  EXPECT_TRUE(MockHttpCache::WriteResponseInfo(entry, &response, true, false));
  truncated = true;
  EXPECT_TRUE(MockHttpCache::ReadResponseInfo(entry, &response, &truncated));
  EXPECT_FALSE(truncated);
  entry->Close();
}

// Tests basic pickling/unpickling of HttpResponseInfo.
TEST(HttpCache, PersistHttpResponseInfo) {
  // Set some fields (add more if needed.)
  net::HttpResponseInfo response1;
  response1.was_cached = false;
  response1.socket_address = net::HostPortPair("1.2.3.4", 80);
  response1.headers = new net::HttpResponseHeaders("HTTP/1.1 200 OK");

  // Pickle.
  Pickle pickle;
  response1.Persist(&pickle, false, false);

  // Unpickle.
  net::HttpResponseInfo response2;
  bool response_truncated;
  EXPECT_TRUE(response2.InitFromPickle(pickle, &response_truncated));
  EXPECT_FALSE(response_truncated);

  // Verify fields.
  EXPECT_TRUE(response2.was_cached);  // InitFromPickle sets this flag.
  EXPECT_EQ("1.2.3.4", response2.socket_address.host());
  EXPECT_EQ(80, response2.socket_address.port());
  EXPECT_EQ("HTTP/1.1 200 OK", response2.headers->GetStatusLine());
}

// Tests that we delete an entry when the request is cancelled before starting
// to read from the network.
TEST(HttpCache, DoomOnDestruction) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  Context* c = new Context();
  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_EQ(net::OK, rv);

  rv = c->trans->Start(&request, c->callback.callback(), net::BoundNetLog());
  if (rv == net::ERR_IO_PENDING)
    c->result = c->callback.WaitForResult();

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Destroy the transaction. We only have the headers so we should delete this
  // entry.
  delete c;

  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we delete an entry when the request is cancelled if the response
// does not have content-length and strong validators.
TEST(HttpCache, DoomOnDestruction2) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  Context* c = new Context();
  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_EQ(net::OK, rv);

  rv = c->trans->Start(&request, c->callback.callback(), net::BoundNetLog());
  if (rv == net::ERR_IO_PENDING)
    rv = c->callback.WaitForResult();

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure that the entry has some data stored.
  scoped_refptr<net::IOBufferWithSize> buf(new net::IOBufferWithSize(10));
  rv = c->trans->Read(buf.get(), buf->size(), c->callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = c->callback.WaitForResult();
  EXPECT_EQ(buf->size(), rv);

  // Destroy the transaction.
  delete c;

  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we delete an entry when the request is cancelled if the response
// has an "Accept-Ranges: none" header.
TEST(HttpCache, DoomOnDestruction3) {
  MockHttpCache cache;

  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers =
      "Last-Modified: Wed, 28 Nov 2007 00:40:09 GMT\n"
      "Content-Length: 22\n"
      "Accept-Ranges: none\n"
      "Etag: \"foopy\"\n";
  AddMockTransaction(&transaction);
  MockHttpRequest request(transaction);

  Context* c = new Context();
  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_EQ(net::OK, rv);

  rv = c->trans->Start(&request, c->callback.callback(), net::BoundNetLog());
  if (rv == net::ERR_IO_PENDING)
    rv = c->callback.WaitForResult();

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure that the entry has some data stored.
  scoped_refptr<net::IOBufferWithSize> buf(new net::IOBufferWithSize(10));
  rv = c->trans->Read(buf.get(), buf->size(), c->callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = c->callback.WaitForResult();
  EXPECT_EQ(buf->size(), rv);

  // Destroy the transaction.
  delete c;

  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  RemoveMockTransaction(&transaction);
}

// Tests that we mark an entry as incomplete when the request is cancelled.
TEST(HttpCache, SetTruncatedFlag) {
  MockHttpCache cache;

  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers =
      "Last-Modified: Wed, 28 Nov 2007 00:40:09 GMT\n"
      "Content-Length: 22\n"
      "Etag: \"foopy\"\n";
  AddMockTransaction(&transaction);
  MockHttpRequest request(transaction);

  scoped_ptr<Context> c(new Context());

  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_EQ(net::OK, rv);

  rv = c->trans->Start(&request, c->callback.callback(), net::BoundNetLog());
  if (rv == net::ERR_IO_PENDING)
    rv = c->callback.WaitForResult();

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure that the entry has some data stored.
  scoped_refptr<net::IOBufferWithSize> buf(new net::IOBufferWithSize(10));
  rv = c->trans->Read(buf.get(), buf->size(), c->callback.callback());
  if (rv == net::ERR_IO_PENDING)
    rv = c->callback.WaitForResult();
  EXPECT_EQ(buf->size(), rv);

  // We want to cancel the request when the transaction is busy.
  rv = c->trans->Read(buf.get(), buf->size(), c->callback.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, rv);
  EXPECT_FALSE(c->callback.have_result());

  MockHttpCache::SetTestMode(TEST_MODE_SYNC_ALL);

  // Destroy the transaction.
  c->trans.reset();
  MockHttpCache::SetTestMode(0);


  // Make sure that we don't invoke the callback. We may have an issue if the
  // UrlRequestJob is killed directly (without cancelling the UrlRequest) so we
  // could end up with the transaction being deleted twice if we send any
  // notification from the transaction destructor (see http://crbug.com/31723).
  EXPECT_FALSE(c->callback.have_result());

  // Verify that the entry is marked as incomplete.
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.OpenBackendEntry(kSimpleGET_Transaction.url, &entry));
  net::HttpResponseInfo response;
  bool truncated = false;
  EXPECT_TRUE(MockHttpCache::ReadResponseInfo(entry, &response, &truncated));
  EXPECT_TRUE(truncated);
  entry->Close();

  RemoveMockTransaction(&transaction);
}

// Tests that we don't mark an entry as truncated when we read everything.
TEST(HttpCache, DontSetTruncatedFlag) {
  MockHttpCache cache;

  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers =
      "Last-Modified: Wed, 28 Nov 2007 00:40:09 GMT\n"
      "Content-Length: 22\n"
      "Etag: \"foopy\"\n";
  AddMockTransaction(&transaction);
  MockHttpRequest request(transaction);

  scoped_ptr<Context> c(new Context());
  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_EQ(net::OK, rv);

  rv = c->trans->Start(&request, c->callback.callback(), net::BoundNetLog());
  EXPECT_EQ(net::OK, c->callback.GetResult(rv));

  // Read everything.
  scoped_refptr<net::IOBufferWithSize> buf(new net::IOBufferWithSize(22));
  rv = c->trans->Read(buf.get(), buf->size(), c->callback.callback());
  EXPECT_EQ(buf->size(), c->callback.GetResult(rv));

  // Destroy the transaction.
  c->trans.reset();

  // Verify that the entry is not marked as truncated.
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.OpenBackendEntry(kSimpleGET_Transaction.url, &entry));
  net::HttpResponseInfo response;
  bool truncated = true;
  EXPECT_TRUE(MockHttpCache::ReadResponseInfo(entry, &response, &truncated));
  EXPECT_FALSE(truncated);
  entry->Close();

  RemoveMockTransaction(&transaction);
}

// Tests that we can continue with a request that was interrupted.
TEST(HttpCache, GET_IncompleteResource) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);

  std::string raw_headers("HTTP/1.1 200 OK\n"
                          "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
                          "ETag: \"foo\"\n"
                          "Accept-Ranges: bytes\n"
                          "Content-Length: 80\n");
  CreateTruncatedEntry(raw_headers, &cache);

  // Now make a regular request.
  std::string headers;
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = EXTRA_HEADER;
  transaction.data = "rg: 00-09 rg: 10-19 rg: 20-29 rg: 30-39 rg: 40-49 "
                     "rg: 50-59 rg: 60-69 rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  // We update the headers with the ones received while revalidating.
  std::string expected_headers(
      "HTTP/1.1 200 OK\n"
      "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
      "Accept-Ranges: bytes\n"
      "ETag: \"foo\"\n"
      "Content-Length: 80\n");

  EXPECT_EQ(expected_headers, headers);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Verify that the disk entry was updated.
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.OpenBackendEntry(kRangeGET_TransactionOK.url, &entry));
  EXPECT_EQ(80, entry->GetDataSize(1));
  bool truncated = true;
  net::HttpResponseInfo response;
  EXPECT_TRUE(MockHttpCache::ReadResponseInfo(entry, &response, &truncated));
  EXPECT_FALSE(truncated);
  entry->Close();

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests the handling of no-store when revalidating a truncated entry.
TEST(HttpCache, GET_IncompleteResource_NoStore) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);

  std::string raw_headers("HTTP/1.1 200 OK\n"
                          "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
                          "ETag: \"foo\"\n"
                          "Accept-Ranges: bytes\n"
                          "Content-Length: 80\n");
  CreateTruncatedEntry(raw_headers, &cache);
  RemoveMockTransaction(&kRangeGET_TransactionOK);

  // Now make a regular request.
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = EXTRA_HEADER;
  std::string response_headers(transaction.response_headers);
  response_headers += ("Cache-Control: no-store\n");
  transaction.response_headers = response_headers.c_str();
  transaction.data = "rg: 00-09 rg: 10-19 rg: 20-29 rg: 30-39 rg: 40-49 "
                     "rg: 50-59 rg: 60-69 rg: 70-79 ";
  AddMockTransaction(&transaction);

  std::string headers;
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  // We update the headers with the ones received while revalidating.
  std::string expected_headers(
      "HTTP/1.1 200 OK\n"
      "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
      "Accept-Ranges: bytes\n"
      "Cache-Control: no-store\n"
      "ETag: \"foo\"\n"
      "Content-Length: 80\n");

  EXPECT_EQ(expected_headers, headers);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Verify that the disk entry was deleted.
  disk_cache::Entry* entry;
  EXPECT_FALSE(cache.OpenBackendEntry(kRangeGET_TransactionOK.url, &entry));
  RemoveMockTransaction(&transaction);
}

// Tests cancelling a request after the server sent no-store.
TEST(HttpCache, GET_IncompleteResource_Cancel) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);

  std::string raw_headers("HTTP/1.1 200 OK\n"
                          "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
                          "ETag: \"foo\"\n"
                          "Accept-Ranges: bytes\n"
                          "Content-Length: 80\n");
  CreateTruncatedEntry(raw_headers, &cache);
  RemoveMockTransaction(&kRangeGET_TransactionOK);

  // Now make a regular request.
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = EXTRA_HEADER;
  std::string response_headers(transaction.response_headers);
  response_headers += ("Cache-Control: no-store\n");
  transaction.response_headers = response_headers.c_str();
  transaction.data = "rg: 00-09 rg: 10-19 rg: 20-29 rg: 30-39 rg: 40-49 "
                     "rg: 50-59 rg: 60-69 rg: 70-79 ";
  AddMockTransaction(&transaction);

  MockHttpRequest request(transaction);
  Context* c = new Context();

  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_EQ(net::OK, rv);

  // Queue another request to this transaction. We have to start this request
  // before the first one gets the response from the server and dooms the entry,
  // otherwise it will just create a new entry without being queued to the first
  // request.
  Context* pending = new Context();
  ASSERT_EQ(net::OK, cache.CreateTransaction(&pending->trans));

  rv = c->trans->Start(&request, c->callback.callback(), net::BoundNetLog());
  EXPECT_EQ(net::ERR_IO_PENDING,
            pending->trans->Start(&request, pending->callback.callback(),
                                  net::BoundNetLog()));
  EXPECT_EQ(net::OK, c->callback.GetResult(rv));

  // Make sure that the entry has some data stored.
  scoped_refptr<net::IOBufferWithSize> buf(new net::IOBufferWithSize(5));
  rv = c->trans->Read(buf.get(), buf->size(), c->callback.callback());
  EXPECT_EQ(5, c->callback.GetResult(rv));

  // Cancel the requests.
  delete c;
  delete pending;

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  base::MessageLoop::current()->RunUntilIdle();
  RemoveMockTransaction(&transaction);
}

// Tests that we delete truncated entries if the server changes its mind midway.
TEST(HttpCache, GET_IncompleteResource2) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);

  // Content-length will be intentionally bad.
  std::string raw_headers("HTTP/1.1 200 OK\n"
                          "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
                          "ETag: \"foo\"\n"
                          "Accept-Ranges: bytes\n"
                          "Content-Length: 50\n");
  CreateTruncatedEntry(raw_headers, &cache);

  // Now make a regular request. We expect the code to fail the validation and
  // retry the request without using byte ranges.
  std::string headers;
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = EXTRA_HEADER;
  transaction.data = "Not a range";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  // The server will return 200 instead of a byte range.
  std::string expected_headers(
      "HTTP/1.1 200 OK\n"
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\n");

  EXPECT_EQ(expected_headers, headers);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Verify that the disk entry was deleted.
  disk_cache::Entry* entry;
  ASSERT_FALSE(cache.OpenBackendEntry(kRangeGET_TransactionOK.url, &entry));
  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we always validate a truncated request.
TEST(HttpCache, GET_IncompleteResource3) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);

  // This should not require validation for 10 hours.
  std::string raw_headers("HTTP/1.1 200 OK\n"
                          "Last-Modified: Sat, 18 Apr 2009 01:10:43 GMT\n"
                          "ETag: \"foo\"\n"
                          "Cache-Control: max-age= 36000\n"
                          "Accept-Ranges: bytes\n"
                          "Content-Length: 80\n");
  CreateTruncatedEntry(raw_headers, &cache);

  // Now make a regular request.
  std::string headers;
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = EXTRA_HEADER;
  transaction.data = "rg: 00-09 rg: 10-19 rg: 20-29 rg: 30-39 rg: 40-49 "
                     "rg: 50-59 rg: 60-69 rg: 70-79 ";

  scoped_ptr<Context> c(new Context);
  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_EQ(net::OK, rv);

  MockHttpRequest request(transaction);
  rv = c->trans->Start(&request, c->callback.callback(), net::BoundNetLog());
  EXPECT_EQ(net::OK, c->callback.GetResult(rv));

  // We should have checked with the server before finishing Start().
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we handle 401s for truncated resources.
TEST(HttpCache, GET_IncompleteResourceWithAuth) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);

  std::string raw_headers("HTTP/1.1 200 OK\n"
                          "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
                          "ETag: \"foo\"\n"
                          "Accept-Ranges: bytes\n"
                          "Content-Length: 80\n");
  CreateTruncatedEntry(raw_headers, &cache);

  // Now make a regular request.
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "X-Require-Mock-Auth: dummy\r\n"
                                EXTRA_HEADER;
  transaction.data = "rg: 00-09 rg: 10-19 rg: 20-29 rg: 30-39 rg: 40-49 "
                     "rg: 50-59 rg: 60-69 rg: 70-79 ";
  RangeTransactionServer handler;

  scoped_ptr<Context> c(new Context);
  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_EQ(net::OK, rv);

  MockHttpRequest request(transaction);
  rv = c->trans->Start(&request, c->callback.callback(), net::BoundNetLog());
  EXPECT_EQ(net::OK, c->callback.GetResult(rv));

  const net::HttpResponseInfo* response = c->trans->GetResponseInfo();
  ASSERT_TRUE(response);
  ASSERT_EQ(401, response->headers->response_code());
  rv = c->trans->RestartWithAuth(net::AuthCredentials(),
                                 c->callback.callback());
  EXPECT_EQ(net::OK, c->callback.GetResult(rv));
  response = c->trans->GetResponseInfo();
  ASSERT_TRUE(response);
  ASSERT_EQ(200, response->headers->response_code());

  ReadAndVerifyTransaction(c->trans.get(), transaction);
  c.reset();  // The destructor could delete the entry.
  EXPECT_EQ(2, cache.network_layer()->transaction_count());

  // Verify that the entry was not deleted.
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.OpenBackendEntry(kRangeGET_TransactionOK.url, &entry));
  entry->Close();

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we cache a 200 response to the validation request.
TEST(HttpCache, GET_IncompleteResource4) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);

  std::string raw_headers("HTTP/1.1 200 OK\n"
                          "Last-Modified: Sat, 18 Apr 2009 01:10:43 GMT\n"
                          "ETag: \"foo\"\n"
                          "Accept-Ranges: bytes\n"
                          "Content-Length: 80\n");
  CreateTruncatedEntry(raw_headers, &cache);

  // Now make a regular request.
  std::string headers;
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = EXTRA_HEADER;
  transaction.data = "Not a range";
  RangeTransactionServer handler;
  handler.set_bad_200(true);
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Verify that the disk entry was updated.
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.OpenBackendEntry(kRangeGET_TransactionOK.url, &entry));
  EXPECT_EQ(11, entry->GetDataSize(1));
  bool truncated = true;
  net::HttpResponseInfo response;
  EXPECT_TRUE(MockHttpCache::ReadResponseInfo(entry, &response, &truncated));
  EXPECT_FALSE(truncated);
  entry->Close();

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that when we cancel a request that was interrupted, we mark it again
// as truncated.
TEST(HttpCache, GET_CancelIncompleteResource) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);

  std::string raw_headers("HTTP/1.1 200 OK\n"
                          "Last-Modified: Sat, 18 Apr 2009 01:10:43 GMT\n"
                          "ETag: \"foo\"\n"
                          "Accept-Ranges: bytes\n"
                          "Content-Length: 80\n");
  CreateTruncatedEntry(raw_headers, &cache);

  // Now make a regular request.
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = EXTRA_HEADER;

  MockHttpRequest request(transaction);
  Context* c = new Context();
  int rv = cache.CreateTransaction(&c->trans);
  ASSERT_EQ(net::OK, rv);

  rv = c->trans->Start(&request, c->callback.callback(), net::BoundNetLog());
  EXPECT_EQ(net::OK, c->callback.GetResult(rv));

  // Read 20 bytes from the cache, and 10 from the net.
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(100));
  rv = c->trans->Read(buf.get(), 20, c->callback.callback());
  EXPECT_EQ(20, c->callback.GetResult(rv));
  rv = c->trans->Read(buf.get(), 10, c->callback.callback());
  EXPECT_EQ(10, c->callback.GetResult(rv));

  // At this point, we are already reading so canceling the request should leave
  // a truncated one.
  delete c;

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Verify that the disk entry was updated: now we have 30 bytes.
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.OpenBackendEntry(kRangeGET_TransactionOK.url, &entry));
  EXPECT_EQ(30, entry->GetDataSize(1));
  bool truncated = false;
  net::HttpResponseInfo response;
  EXPECT_TRUE(MockHttpCache::ReadResponseInfo(entry, &response, &truncated));
  EXPECT_TRUE(truncated);
  entry->Close();
  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we can handle range requests when we have a truncated entry.
TEST(HttpCache, RangeGET_IncompleteResource) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);

  // Content-length will be intentionally bogus.
  std::string raw_headers("HTTP/1.1 200 OK\n"
                          "Last-Modified: something\n"
                          "ETag: \"foo\"\n"
                          "Accept-Ranges: bytes\n"
                          "Content-Length: 10\n");
  CreateTruncatedEntry(raw_headers, &cache);

  // Now make a range request.
  std::string headers;
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  Verify206Response(headers, 40, 49);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

TEST(HttpCache, SyncRead) {
  MockHttpCache cache;

  // This test ensures that a read that completes synchronously does not cause
  // any problems.

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.test_mode |= (TEST_MODE_SYNC_CACHE_START |
                            TEST_MODE_SYNC_CACHE_READ |
                            TEST_MODE_SYNC_CACHE_WRITE);

  MockHttpRequest r1(transaction),
                  r2(transaction),
                  r3(transaction);

  TestTransactionConsumer c1(net::DEFAULT_PRIORITY, cache.http_cache()),
                          c2(net::DEFAULT_PRIORITY, cache.http_cache()),
                          c3(net::DEFAULT_PRIORITY, cache.http_cache());

  c1.Start(&r1, net::BoundNetLog());

  r2.load_flags |= net::LOAD_ONLY_FROM_CACHE;
  c2.Start(&r2, net::BoundNetLog());

  r3.load_flags |= net::LOAD_ONLY_FROM_CACHE;
  c3.Start(&r3, net::BoundNetLog());

  base::MessageLoop::current()->Run();

  EXPECT_TRUE(c1.is_done());
  EXPECT_TRUE(c2.is_done());
  EXPECT_TRUE(c3.is_done());

  EXPECT_EQ(net::OK, c1.error());
  EXPECT_EQ(net::OK, c2.error());
  EXPECT_EQ(net::OK, c3.error());
}

TEST(HttpCache, ValidationResultsIn200) {
  MockHttpCache cache;

  // This test ensures that a conditional request, which results in a 200
  // instead of a 304, properly truncates the existing response data.

  // write to the cache
  RunTransactionTest(cache.http_cache(), kETagGET_Transaction);

  // force this transaction to validate the cache
  MockTransaction transaction(kETagGET_Transaction);
  transaction.load_flags |= net::LOAD_VALIDATE_CACHE;
  RunTransactionTest(cache.http_cache(), transaction);

  // read from the cache
  RunTransactionTest(cache.http_cache(), kETagGET_Transaction);
}

TEST(HttpCache, CachedRedirect) {
  MockHttpCache cache;

  ScopedMockTransaction kTestTransaction(kSimpleGET_Transaction);
  kTestTransaction.status = "HTTP/1.1 301 Moved Permanently";
  kTestTransaction.response_headers = "Location: http://www.bar.com/\n";

  MockHttpRequest request(kTestTransaction);
  net::TestCompletionCallback callback;

  // Write to the cache.
  {
    scoped_ptr<net::HttpTransaction> trans;
    ASSERT_EQ(net::OK, cache.CreateTransaction(&trans));

    int rv = trans->Start(&request, callback.callback(), net::BoundNetLog());
    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();
    ASSERT_EQ(net::OK, rv);

    const net::HttpResponseInfo* info = trans->GetResponseInfo();
    ASSERT_TRUE(info);

    EXPECT_EQ(info->headers->response_code(), 301);

    std::string location;
    info->headers->EnumerateHeader(NULL, "Location", &location);
    EXPECT_EQ(location, "http://www.bar.com/");

    // Mark the transaction as completed so it is cached.
    trans->DoneReading();

    // Destroy transaction when going out of scope. We have not actually
    // read the response body -- want to test that it is still getting cached.
  }
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Active entries in the cache are not retired synchronously. Make
  // sure the next run hits the MockHttpCache and open_count is
  // correct.
  base::MessageLoop::current()->RunUntilIdle();

  // Read from the cache.
  {
    scoped_ptr<net::HttpTransaction> trans;
    ASSERT_EQ(net::OK, cache.CreateTransaction(&trans));

    int rv = trans->Start(&request, callback.callback(), net::BoundNetLog());
    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();
    ASSERT_EQ(net::OK, rv);

    const net::HttpResponseInfo* info = trans->GetResponseInfo();
    ASSERT_TRUE(info);

    EXPECT_EQ(info->headers->response_code(), 301);

    std::string location;
    info->headers->EnumerateHeader(NULL, "Location", &location);
    EXPECT_EQ(location, "http://www.bar.com/");

    // Mark the transaction as completed so it is cached.
    trans->DoneReading();

    // Destroy transaction when going out of scope. We have not actually
    // read the response body -- want to test that it is still getting cached.
  }
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Verify that no-cache resources are stored in cache, but are not fetched from
// cache during normal loads.
TEST(HttpCache, CacheControlNoCacheNormalLoad) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers = "cache-control: no-cache\n";

  // Initial load.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Try loading again; it should result in a network fetch.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  disk_cache::Entry* entry;
  EXPECT_TRUE(cache.OpenBackendEntry(transaction.url, &entry));
  entry->Close();
}

// Verify that no-cache resources are stored in cache and fetched from cache
// when the LOAD_PREFERRING_CACHE flag is set.
TEST(HttpCache, CacheControlNoCacheHistoryLoad) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers = "cache-control: no-cache\n";

  // Initial load.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Try loading again with LOAD_PREFERRING_CACHE.
  transaction.load_flags = net::LOAD_PREFERRING_CACHE;
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  disk_cache::Entry* entry;
  EXPECT_TRUE(cache.OpenBackendEntry(transaction.url, &entry));
  entry->Close();
}

TEST(HttpCache, CacheControlNoStore) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers = "cache-control: no-store\n";

  // initial load
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // try loading again; it should result in a network fetch
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  disk_cache::Entry* entry;
  EXPECT_FALSE(cache.OpenBackendEntry(transaction.url, &entry));
}

TEST(HttpCache, CacheControlNoStore2) {
  // this test is similar to the above test, except that the initial response
  // is cachable, but when it is validated, no-store is received causing the
  // cached document to be deleted.
  MockHttpCache cache;

  ScopedMockTransaction transaction(kETagGET_Transaction);

  // initial load
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // try loading again; it should result in a network fetch
  transaction.load_flags = net::LOAD_VALIDATE_CACHE;
  transaction.response_headers = "cache-control: no-store\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  disk_cache::Entry* entry;
  EXPECT_FALSE(cache.OpenBackendEntry(transaction.url, &entry));
}

TEST(HttpCache, CacheControlNoStore3) {
  // this test is similar to the above test, except that the response is a 304
  // instead of a 200.  this should never happen in practice, but it seems like
  // a good thing to verify that we still destroy the cache entry.
  MockHttpCache cache;

  ScopedMockTransaction transaction(kETagGET_Transaction);

  // initial load
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // try loading again; it should result in a network fetch
  transaction.load_flags = net::LOAD_VALIDATE_CACHE;
  transaction.response_headers = "cache-control: no-store\n";
  transaction.status = "HTTP/1.1 304 Not Modified";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  disk_cache::Entry* entry;
  EXPECT_FALSE(cache.OpenBackendEntry(transaction.url, &entry));
}

// Ensure that we don't cache requests served over bad HTTPS.
TEST(HttpCache, SimpleGET_SSLError) {
  MockHttpCache cache;

  MockTransaction transaction = kSimpleGET_Transaction;
  transaction.cert_status = net::CERT_STATUS_REVOKED;
  ScopedMockTransaction scoped_transaction(transaction);

  // write to the cache
  RunTransactionTest(cache.http_cache(), transaction);

  // Test that it was not cached.
  transaction.load_flags |= net::LOAD_ONLY_FROM_CACHE;

  MockHttpRequest request(transaction);
  net::TestCompletionCallback callback;

  scoped_ptr<net::HttpTransaction> trans;
  ASSERT_EQ(net::OK, cache.CreateTransaction(&trans));

  int rv = trans->Start(&request, callback.callback(), net::BoundNetLog());
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(net::ERR_CACHE_MISS, rv);
}

// Ensure that we don't crash by if left-behind transactions.
TEST(HttpCache, OutlivedTransactions) {
  MockHttpCache* cache = new MockHttpCache;

  scoped_ptr<net::HttpTransaction> trans;
  EXPECT_EQ(net::OK, cache->CreateTransaction(&trans));

  delete cache;
  trans.reset();
}

// Test that the disabled mode works.
TEST(HttpCache, CacheDisabledMode) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // go into disabled mode
  cache.http_cache()->set_mode(net::HttpCache::DISABLE);

  // force this transaction to write to the cache again
  MockTransaction transaction(kSimpleGET_Transaction);

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Other tests check that the response headers of the cached response
// get updated on 304. Here we specifically check that the
// HttpResponseHeaders::request_time and HttpResponseHeaders::response_time
// fields also gets updated.
// http://crbug.com/20594.
TEST(HttpCache, UpdatesRequestResponseTimeOn304) {
  MockHttpCache cache;

  const char* kUrl = "http://foobar";
  const char* kData = "body";

  MockTransaction mock_network_response = { 0 };
  mock_network_response.url = kUrl;

  AddMockTransaction(&mock_network_response);

  // Request |kUrl|, causing |kNetResponse1| to be written to the cache.

  MockTransaction request = { 0 };
  request.url = kUrl;
  request.method = "GET";
  request.request_headers = "\r\n";
  request.data = kData;

  static const Response kNetResponse1 = {
    "HTTP/1.1 200 OK",
    "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    kData
  };

  kNetResponse1.AssignTo(&mock_network_response);

  RunTransactionTest(cache.http_cache(), request);

  // Request |kUrl| again, this time validating the cache and getting
  // a 304 back.

  request.load_flags = net::LOAD_VALIDATE_CACHE;

  static const Response kNetResponse2 = {
    "HTTP/1.1 304 Not Modified",
    "Date: Wed, 22 Jul 2009 03:15:26 GMT\n",
    ""
  };

  kNetResponse2.AssignTo(&mock_network_response);

  base::Time request_time = base::Time() + base::TimeDelta::FromHours(1234);
  base::Time response_time = base::Time() + base::TimeDelta::FromHours(1235);

  mock_network_response.request_time = request_time;
  mock_network_response.response_time = response_time;

  net::HttpResponseInfo response;
  RunTransactionTestWithResponseInfo(cache.http_cache(), request, &response);

  // The request and response times should have been updated.
  EXPECT_EQ(request_time.ToInternalValue(),
            response.request_time.ToInternalValue());
  EXPECT_EQ(response_time.ToInternalValue(),
            response.response_time.ToInternalValue());

  std::string headers;
  response.headers->GetNormalizedHeaders(&headers);

  EXPECT_EQ("HTTP/1.1 200 OK\n"
            "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
            "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
            headers);

  RemoveMockTransaction(&mock_network_response);
}

// Tests that we can write metadata to an entry.
TEST(HttpCache, WriteMetadata_OK) {
  MockHttpCache cache;

  // Write to the cache
  net::HttpResponseInfo response;
  RunTransactionTestWithResponseInfo(cache.http_cache(), kSimpleGET_Transaction,
                                     &response);
  EXPECT_TRUE(response.metadata.get() == NULL);

  // Trivial call.
  cache.http_cache()->WriteMetadata(GURL("foo"), net::DEFAULT_PRIORITY,
                                    Time::Now(), NULL, 0);

  // Write meta data to the same entry.
  scoped_refptr<net::IOBufferWithSize> buf(new net::IOBufferWithSize(50));
  memset(buf->data(), 0, buf->size());
  base::strlcpy(buf->data(), "Hi there", buf->size());
  cache.http_cache()->WriteMetadata(GURL(kSimpleGET_Transaction.url),
                                    net::DEFAULT_PRIORITY,
                                    response.response_time,
                                    buf.get(),
                                    buf->size());

  // Release the buffer before the operation takes place.
  buf = NULL;

  // Makes sure we finish pending operations.
  base::MessageLoop::current()->RunUntilIdle();

  RunTransactionTestWithResponseInfo(cache.http_cache(), kSimpleGET_Transaction,
                                     &response);
  ASSERT_TRUE(response.metadata.get() != NULL);
  EXPECT_EQ(50, response.metadata->size());
  EXPECT_EQ(0, strcmp(response.metadata->data(), "Hi there"));

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that we only write metadata to an entry if the time stamp matches.
TEST(HttpCache, WriteMetadata_Fail) {
  MockHttpCache cache;

  // Write to the cache
  net::HttpResponseInfo response;
  RunTransactionTestWithResponseInfo(cache.http_cache(), kSimpleGET_Transaction,
                                     &response);
  EXPECT_TRUE(response.metadata.get() == NULL);

  // Attempt to write meta data to the same entry.
  scoped_refptr<net::IOBufferWithSize> buf(new net::IOBufferWithSize(50));
  memset(buf->data(), 0, buf->size());
  base::strlcpy(buf->data(), "Hi there", buf->size());
  base::Time expected_time = response.response_time -
                             base::TimeDelta::FromMilliseconds(20);
  cache.http_cache()->WriteMetadata(GURL(kSimpleGET_Transaction.url),
                                    net::DEFAULT_PRIORITY,
                                    expected_time,
                                    buf.get(),
                                    buf->size());

  // Makes sure we finish pending operations.
  base::MessageLoop::current()->RunUntilIdle();

  RunTransactionTestWithResponseInfo(cache.http_cache(), kSimpleGET_Transaction,
                                     &response);
  EXPECT_TRUE(response.metadata.get() == NULL);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that we can read metadata after validating the entry and with READ mode
// transactions.
TEST(HttpCache, ReadMetadata) {
  MockHttpCache cache;

  // Write to the cache
  net::HttpResponseInfo response;
  RunTransactionTestWithResponseInfo(cache.http_cache(),
                                     kTypicalGET_Transaction, &response);
  EXPECT_TRUE(response.metadata.get() == NULL);

  // Write meta data to the same entry.
  scoped_refptr<net::IOBufferWithSize> buf(new net::IOBufferWithSize(50));
  memset(buf->data(), 0, buf->size());
  base::strlcpy(buf->data(), "Hi there", buf->size());
  cache.http_cache()->WriteMetadata(GURL(kTypicalGET_Transaction.url),
                                    net::DEFAULT_PRIORITY,
                                    response.response_time,
                                    buf.get(),
                                    buf->size());

  // Makes sure we finish pending operations.
  base::MessageLoop::current()->RunUntilIdle();

  // Start with a READ mode transaction.
  MockTransaction trans1(kTypicalGET_Transaction);
  trans1.load_flags = net::LOAD_ONLY_FROM_CACHE;

  RunTransactionTestWithResponseInfo(cache.http_cache(), trans1, &response);
  ASSERT_TRUE(response.metadata.get() != NULL);
  EXPECT_EQ(50, response.metadata->size());
  EXPECT_EQ(0, strcmp(response.metadata->data(), "Hi there"));

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  base::MessageLoop::current()->RunUntilIdle();

  // Now make sure that the entry is re-validated with the server.
  trans1.load_flags = net::LOAD_VALIDATE_CACHE;
  trans1.status = "HTTP/1.1 304 Not Modified";
  AddMockTransaction(&trans1);

  response.metadata = NULL;
  RunTransactionTestWithResponseInfo(cache.http_cache(), trans1, &response);
  EXPECT_TRUE(response.metadata.get() != NULL);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(3, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  base::MessageLoop::current()->RunUntilIdle();
  RemoveMockTransaction(&trans1);

  // Now return 200 when validating the entry so the metadata will be lost.
  MockTransaction trans2(kTypicalGET_Transaction);
  trans2.load_flags = net::LOAD_VALIDATE_CACHE;
  RunTransactionTestWithResponseInfo(cache.http_cache(), trans2, &response);
  EXPECT_TRUE(response.metadata.get() == NULL);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(4, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that we don't mark entries as truncated when a filter detects the end
// of the stream.
TEST(HttpCache, FilterCompletion) {
  MockHttpCache cache;
  net::TestCompletionCallback callback;

  {
    scoped_ptr<net::HttpTransaction> trans;
    ASSERT_EQ(net::OK, cache.CreateTransaction(&trans));

    MockHttpRequest request(kSimpleGET_Transaction);
    int rv = trans->Start(&request, callback.callback(), net::BoundNetLog());
    EXPECT_EQ(net::OK, callback.GetResult(rv));

    scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(256));
    rv = trans->Read(buf.get(), 256, callback.callback());
    EXPECT_GT(callback.GetResult(rv), 0);

    // Now make sure that the entry is preserved.
    trans->DoneReading();
  }

  // Make sure that the ActiveEntry is gone.
  base::MessageLoop::current()->RunUntilIdle();

  // Read from the cache.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that we don't mark entries as truncated and release the cache
// entry when DoneReading() is called before any Read() calls, such as
// for a redirect.
TEST(HttpCache, DoneReading) {
  MockHttpCache cache;
  net::TestCompletionCallback callback;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.data = "";

  scoped_ptr<net::HttpTransaction> trans;
  ASSERT_EQ(net::OK, cache.CreateTransaction(&trans));

  MockHttpRequest request(transaction);
  int rv = trans->Start(&request, callback.callback(), net::BoundNetLog());
  EXPECT_EQ(net::OK, callback.GetResult(rv));

  trans->DoneReading();
  // Leave the transaction around.

  // Make sure that the ActiveEntry is gone.
  base::MessageLoop::current()->RunUntilIdle();

  // Read from the cache. This should not deadlock.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Tests that we stop caching when told.
TEST(HttpCache, StopCachingDeletesEntry) {
  MockHttpCache cache;
  net::TestCompletionCallback callback;
  MockHttpRequest request(kSimpleGET_Transaction);

  {
    scoped_ptr<net::HttpTransaction> trans;
    ASSERT_EQ(net::OK, cache.CreateTransaction(&trans));

    int rv = trans->Start(&request, callback.callback(), net::BoundNetLog());
    EXPECT_EQ(net::OK, callback.GetResult(rv));

    scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(256));
    rv = trans->Read(buf.get(), 10, callback.callback());
    EXPECT_EQ(10, callback.GetResult(rv));

    trans->StopCaching();

    // We should be able to keep reading.
    rv = trans->Read(buf.get(), 256, callback.callback());
    EXPECT_GT(callback.GetResult(rv), 0);
    rv = trans->Read(buf.get(), 256, callback.callback());
    EXPECT_EQ(0, callback.GetResult(rv));
  }

  // Make sure that the ActiveEntry is gone.
  base::MessageLoop::current()->RunUntilIdle();

  // Verify that the entry is gone.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we stop caching when told, even if DoneReading is called
// after StopCaching.
TEST(HttpCache, StopCachingThenDoneReadingDeletesEntry) {
  MockHttpCache cache;
  net::TestCompletionCallback callback;
  MockHttpRequest request(kSimpleGET_Transaction);

  {
    scoped_ptr<net::HttpTransaction> trans;
    ASSERT_EQ(net::OK, cache.CreateTransaction(&trans));

    int rv = trans->Start(&request, callback.callback(), net::BoundNetLog());
    EXPECT_EQ(net::OK, callback.GetResult(rv));

    scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(256));
    rv = trans->Read(buf.get(), 10, callback.callback());
    EXPECT_EQ(10, callback.GetResult(rv));

    trans->StopCaching();

    // We should be able to keep reading.
    rv = trans->Read(buf.get(), 256, callback.callback());
    EXPECT_GT(callback.GetResult(rv), 0);
    rv = trans->Read(buf.get(), 256, callback.callback());
    EXPECT_EQ(0, callback.GetResult(rv));

    // We should be able to call DoneReading.
    trans->DoneReading();
  }

  // Make sure that the ActiveEntry is gone.
  base::MessageLoop::current()->RunUntilIdle();

  // Verify that the entry is gone.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we stop caching when told, when using auth.
TEST(HttpCache, StopCachingWithAuthDeletesEntry) {
  MockHttpCache cache;
  net::TestCompletionCallback callback;
  MockTransaction mock_transaction(kSimpleGET_Transaction);
  mock_transaction.status = "HTTP/1.1 401 Unauthorized";
  AddMockTransaction(&mock_transaction);
  MockHttpRequest request(mock_transaction);

  {
    scoped_ptr<net::HttpTransaction> trans;
    ASSERT_EQ(net::OK, cache.CreateTransaction(&trans));

    int rv = trans->Start(&request, callback.callback(), net::BoundNetLog());
    EXPECT_EQ(net::OK, callback.GetResult(rv));

    trans->StopCaching();

    scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(256));
    rv = trans->Read(buf.get(), 10, callback.callback());
    EXPECT_EQ(callback.GetResult(rv), 10);
  }
  RemoveMockTransaction(&mock_transaction);

  // Make sure that the ActiveEntry is gone.
  base::MessageLoop::current()->RunUntilIdle();

  // Verify that the entry is gone.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that when we are told to stop caching we don't throw away valid data.
TEST(HttpCache, StopCachingSavesEntry) {
  MockHttpCache cache;
  net::TestCompletionCallback callback;
  MockHttpRequest request(kSimpleGET_Transaction);

  {
    scoped_ptr<net::HttpTransaction> trans;
    ASSERT_EQ(net::OK, cache.CreateTransaction(&trans));

    // Force a response that can be resumed.
    MockTransaction mock_transaction(kSimpleGET_Transaction);
    AddMockTransaction(&mock_transaction);
    mock_transaction.response_headers = "Cache-Control: max-age=10000\n"
                                        "Content-Length: 42\n"
                                        "Etag: \"foo\"\n";

    int rv = trans->Start(&request, callback.callback(), net::BoundNetLog());
    EXPECT_EQ(net::OK, callback.GetResult(rv));

    scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(256));
    rv = trans->Read(buf.get(), 10, callback.callback());
    EXPECT_EQ(callback.GetResult(rv), 10);

    trans->StopCaching();

    // We should be able to keep reading.
    rv = trans->Read(buf.get(), 256, callback.callback());
    EXPECT_GT(callback.GetResult(rv), 0);
    rv = trans->Read(buf.get(), 256, callback.callback());
    EXPECT_EQ(callback.GetResult(rv), 0);

    RemoveMockTransaction(&mock_transaction);
  }

  // Verify that the entry is marked as incomplete.
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.OpenBackendEntry(kSimpleGET_Transaction.url, &entry));
  net::HttpResponseInfo response;
  bool truncated = false;
  EXPECT_TRUE(MockHttpCache::ReadResponseInfo(entry, &response, &truncated));
  EXPECT_TRUE(truncated);
  entry->Close();
}

// Tests that we handle truncated enries when StopCaching is called.
TEST(HttpCache, StopCachingTruncatedEntry) {
  MockHttpCache cache;
  net::TestCompletionCallback callback;
  MockHttpRequest request(kRangeGET_TransactionOK);
  request.extra_headers.Clear();
  request.extra_headers.AddHeaderFromString(EXTRA_HEADER_LINE);
  AddMockTransaction(&kRangeGET_TransactionOK);

  std::string raw_headers("HTTP/1.1 200 OK\n"
                          "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
                          "ETag: \"foo\"\n"
                          "Accept-Ranges: bytes\n"
                          "Content-Length: 80\n");
  CreateTruncatedEntry(raw_headers, &cache);

  {
    // Now make a regular request.
    scoped_ptr<net::HttpTransaction> trans;
    ASSERT_EQ(net::OK, cache.CreateTransaction(&trans));

    int rv = trans->Start(&request, callback.callback(), net::BoundNetLog());
    EXPECT_EQ(net::OK, callback.GetResult(rv));

    scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(256));
    rv = trans->Read(buf.get(), 10, callback.callback());
    EXPECT_EQ(callback.GetResult(rv), 10);

    // This is actually going to do nothing.
    trans->StopCaching();

    // We should be able to keep reading.
    rv = trans->Read(buf.get(), 256, callback.callback());
    EXPECT_GT(callback.GetResult(rv), 0);
    rv = trans->Read(buf.get(), 256, callback.callback());
    EXPECT_GT(callback.GetResult(rv), 0);
    rv = trans->Read(buf.get(), 256, callback.callback());
    EXPECT_EQ(callback.GetResult(rv), 0);
  }

  // Verify that the disk entry was updated.
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.OpenBackendEntry(kRangeGET_TransactionOK.url, &entry));
  EXPECT_EQ(80, entry->GetDataSize(1));
  bool truncated = true;
  net::HttpResponseInfo response;
  EXPECT_TRUE(MockHttpCache::ReadResponseInfo(entry, &response, &truncated));
  EXPECT_FALSE(truncated);
  entry->Close();

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we detect truncated resources from the net when there is
// a Content-Length header.
TEST(HttpCache, TruncatedByContentLength) {
  MockHttpCache cache;
  net::TestCompletionCallback callback;

  MockTransaction transaction(kSimpleGET_Transaction);
  AddMockTransaction(&transaction);
  transaction.response_headers = "Cache-Control: max-age=10000\n"
                                 "Content-Length: 100\n";
  RunTransactionTest(cache.http_cache(), transaction);
  RemoveMockTransaction(&transaction);

  // Read from the cache.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we actually flag entries as truncated when we detect an error
// from the net.
TEST(HttpCache, TruncatedByContentLength2) {
  MockHttpCache cache;
  net::TestCompletionCallback callback;

  MockTransaction transaction(kSimpleGET_Transaction);
  AddMockTransaction(&transaction);
  transaction.response_headers = "Cache-Control: max-age=10000\n"
                                 "Content-Length: 100\n"
                                 "Etag: \"foo\"\n";
  RunTransactionTest(cache.http_cache(), transaction);
  RemoveMockTransaction(&transaction);

  // Verify that the entry is marked as incomplete.
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.OpenBackendEntry(kSimpleGET_Transaction.url, &entry));
  net::HttpResponseInfo response;
  bool truncated = false;
  EXPECT_TRUE(MockHttpCache::ReadResponseInfo(entry, &response, &truncated));
  EXPECT_TRUE(truncated);
  entry->Close();
}

// Make sure that calling SetPriority on a cache transaction passes on
// its priority updates to its underlying network transaction.
TEST(HttpCache, SetPriority) {
  MockHttpCache cache;

  scoped_ptr<net::HttpTransaction> trans;
  ASSERT_EQ(net::OK, cache.http_cache()->CreateTransaction(net::IDLE, &trans));

  // Shouldn't crash, but doesn't do anything either.
  trans->SetPriority(net::LOW);

  EXPECT_FALSE(cache.network_layer()->last_transaction());
  EXPECT_EQ(net::DEFAULT_PRIORITY,
            cache.network_layer()->last_create_transaction_priority());

  net::HttpRequestInfo info;
  info.url = GURL(kSimpleGET_Transaction.url);
  net::TestCompletionCallback callback;
  EXPECT_EQ(net::ERR_IO_PENDING,
            trans->Start(&info, callback.callback(), net::BoundNetLog()));

  EXPECT_TRUE(cache.network_layer()->last_transaction());
  if (cache.network_layer()->last_transaction()) {
    EXPECT_EQ(net::LOW,
              cache.network_layer()->last_create_transaction_priority());
    EXPECT_EQ(net::LOW,
              cache.network_layer()->last_transaction()->priority());
  }

  trans->SetPriority(net::HIGHEST);

  if (cache.network_layer()->last_transaction()) {
    EXPECT_EQ(net::LOW,
              cache.network_layer()->last_create_transaction_priority());
    EXPECT_EQ(net::HIGHEST,
              cache.network_layer()->last_transaction()->priority());
  }

  EXPECT_EQ(net::OK, callback.WaitForResult());
}

// Make sure that calling SetWebSocketHandshakeStreamCreateHelper on a cache
// transaction passes on its argument to the underlying network transaction.
TEST(HttpCache, SetWebSocketHandshakeStreamCreateHelper) {
  MockHttpCache cache;

  FakeWebSocketHandshakeStreamCreateHelper create_helper;
  scoped_ptr<net::HttpTransaction> trans;
  ASSERT_EQ(net::OK, cache.http_cache()->CreateTransaction(net::IDLE, &trans));

  EXPECT_FALSE(cache.network_layer()->last_transaction());

  net::HttpRequestInfo info;
  info.url = GURL(kSimpleGET_Transaction.url);
  net::TestCompletionCallback callback;
  EXPECT_EQ(net::ERR_IO_PENDING,
            trans->Start(&info, callback.callback(), net::BoundNetLog()));

  ASSERT_TRUE(cache.network_layer()->last_transaction());
  EXPECT_FALSE(cache.network_layer()->last_transaction()->
               websocket_handshake_stream_create_helper());
  trans->SetWebSocketHandshakeStreamCreateHelper(&create_helper);
  EXPECT_EQ(&create_helper,
            cache.network_layer()->last_transaction()->
            websocket_handshake_stream_create_helper());
  EXPECT_EQ(net::OK, callback.WaitForResult());
}

// Make sure that a cache transaction passes on its priority to
// newly-created network transactions.
TEST(HttpCache, SetPriorityNewTransaction) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);

  std::string raw_headers("HTTP/1.1 200 OK\n"
                          "Last-Modified: Sat, 18 Apr 2007 01:10:43 GMT\n"
                          "ETag: \"foo\"\n"
                          "Accept-Ranges: bytes\n"
                          "Content-Length: 80\n");
  CreateTruncatedEntry(raw_headers, &cache);

  // Now make a regular request.
  std::string headers;
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = EXTRA_HEADER;
  transaction.data = "rg: 00-09 rg: 10-19 rg: 20-29 rg: 30-39 rg: 40-49 "
                     "rg: 50-59 rg: 60-69 rg: 70-79 ";

  scoped_ptr<net::HttpTransaction> trans;
  ASSERT_EQ(net::OK,
            cache.http_cache()->CreateTransaction(net::MEDIUM, &trans));
  EXPECT_EQ(net::DEFAULT_PRIORITY,
            cache.network_layer()->last_create_transaction_priority());

  MockHttpRequest info(transaction);
  net::TestCompletionCallback callback;
  EXPECT_EQ(net::ERR_IO_PENDING,
            trans->Start(&info, callback.callback(), net::BoundNetLog()));
  EXPECT_EQ(net::OK, callback.WaitForResult());

  EXPECT_EQ(net::MEDIUM,
            cache.network_layer()->last_create_transaction_priority());

  trans->SetPriority(net::HIGHEST);
  // Should trigger a new network transaction and pick up the new
  // priority.
  ReadAndVerifyTransaction(trans.get(), transaction);

  EXPECT_EQ(net::HIGHEST,
            cache.network_layer()->last_create_transaction_priority());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

int64 RunTransactionAndGetReceivedBytes(
    MockHttpCache& cache,
    const MockTransaction& trans_info) {
  int64 received_bytes = -1;
  RunTransactionTestBase(cache.http_cache(), trans_info,
                         MockHttpRequest(trans_info), NULL, net::BoundNetLog(),
                         NULL, &received_bytes);
  return received_bytes;
}

int64 TransactionSize(const MockTransaction& transaction) {
  return strlen(transaction.status) + strlen(transaction.response_headers) +
         strlen(transaction.data);
}

TEST(HttpCache, ReceivedBytesCacheMissAndThenHit) {
  MockHttpCache cache;

  MockTransaction transaction(kSimpleGET_Transaction);
  int64 received_bytes = RunTransactionAndGetReceivedBytes(cache, transaction);
  EXPECT_EQ(TransactionSize(transaction), received_bytes);

  received_bytes = RunTransactionAndGetReceivedBytes(cache, transaction);
  EXPECT_EQ(0, received_bytes);
}

TEST(HttpCache, ReceivedBytesConditionalRequest304) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kETagGET_Transaction);
  int64 received_bytes = RunTransactionAndGetReceivedBytes(cache, transaction);
  EXPECT_EQ(TransactionSize(transaction), received_bytes);

  transaction.load_flags = net::LOAD_VALIDATE_CACHE;
  transaction.handler = ETagGet_ConditionalRequest_Handler;
  received_bytes = RunTransactionAndGetReceivedBytes(cache, transaction);
  EXPECT_EQ(TransactionSize(transaction), received_bytes);
}

TEST(HttpCache, ReceivedBytesConditionalRequest200) {
  MockHttpCache cache;

  MockTransaction transaction(kTypicalGET_Transaction);
  transaction.request_headers = "Foo: bar\r\n";
  transaction.response_headers =
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\n"
      "Last-Modified: Wed, 28 Nov 2007 00:40:09 GMT\n"
      "Etag: \"foopy\"\n"
      "Cache-Control: max-age=0\n"
      "Vary: Foo\n";
  AddMockTransaction(&transaction);
  int64 received_bytes = RunTransactionAndGetReceivedBytes(cache, transaction);
  EXPECT_EQ(TransactionSize(transaction), received_bytes);

  RevalidationServer server;
  transaction.handler = server.Handler;
  transaction.request_headers = "Foo: none\r\n";
  received_bytes = RunTransactionAndGetReceivedBytes(cache, transaction);
  EXPECT_EQ(TransactionSize(transaction), received_bytes);

  RemoveMockTransaction(&transaction);
}

TEST(HttpCache, ReceivedBytesRange) {
  MockHttpCache cache;
  AddMockTransaction(&kRangeGET_TransactionOK);
  MockTransaction transaction(kRangeGET_TransactionOK);

  // Read bytes 40-49 from the network.
  int64 received_bytes = RunTransactionAndGetReceivedBytes(cache, transaction);
  int64 range_response_size = TransactionSize(transaction);
  EXPECT_EQ(range_response_size, received_bytes);

  // Read bytes 40-49 from the cache.
  received_bytes = RunTransactionAndGetReceivedBytes(cache, transaction);
  EXPECT_EQ(0, received_bytes);
  base::MessageLoop::current()->RunUntilIdle();

  // Read bytes 30-39 from the network.
  transaction.request_headers = "Range: bytes = 30-39\r\n" EXTRA_HEADER;
  transaction.data = "rg: 30-39 ";
  received_bytes = RunTransactionAndGetReceivedBytes(cache, transaction);
  EXPECT_EQ(range_response_size, received_bytes);
  base::MessageLoop::current()->RunUntilIdle();

  // Read bytes 20-29 and 50-59 from the network, bytes 30-49 from the cache.
  transaction.request_headers = "Range: bytes = 20-59\r\n" EXTRA_HEADER;
  transaction.data = "rg: 20-29 rg: 30-39 rg: 40-49 rg: 50-59 ";
  received_bytes = RunTransactionAndGetReceivedBytes(cache, transaction);
  EXPECT_EQ(range_response_size * 2, received_bytes);

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}
