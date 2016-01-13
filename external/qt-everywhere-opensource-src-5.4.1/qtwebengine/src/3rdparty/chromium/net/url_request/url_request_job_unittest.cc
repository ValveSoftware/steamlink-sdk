// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_job.h"

#include "base/run_loop.h"
#include "net/base/request_priority.h"
#include "net/http/http_transaction_test_util.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// This is a header that signals the end of the data.
const char kGzipData[] = "\x1f\x08b\x08\0\0\0\0\0\0\3\3\0\0\0\0\0\0\0\0";
const char kGzipDataWithName[] =
    "\x1f\x08b\x08\x08\0\0\0\0\0\0name\0\3\0\0\0\0\0\0\0\0";

void GZipServer(const HttpRequestInfo* request,
                std::string* response_status,
                std::string* response_headers,
                std::string* response_data) {
  response_data->assign(kGzipData, sizeof(kGzipData));
}

void BigGZipServer(const HttpRequestInfo* request,
                   std::string* response_status,
                   std::string* response_headers,
                   std::string* response_data) {
  response_data->assign(kGzipDataWithName, sizeof(kGzipDataWithName));
  response_data->insert(10, 64 * 1024, 'a');
}

const MockTransaction kGZip_Transaction = {
    "http://www.google.com/gzyp",
    "GET",
    base::Time(),
    "",
    LOAD_NORMAL,
    "HTTP/1.1 200 OK",
    "Cache-Control: max-age=10000\n"
    "Content-Encoding: gzip\n"
    "Content-Length: 30\n",  // Intentionally wrong.
    base::Time(),
    "",
    TEST_MODE_NORMAL,
    &GZipServer,
    0,
    OK
};

const MockTransaction kRedirect_Transaction = {
    "http://www.google.com/redirect",
    "GET",
    base::Time(),
    "",
    LOAD_NORMAL,
    "HTTP/1.1 302 Found",
    "Cache-Control: max-age=10000\n"
    "Location: http://www.google.com/destination\n"
    "Content-Length: 5\n",
    base::Time(),
    "hello",
    TEST_MODE_NORMAL,
    NULL,
    0,
    OK
};

}  // namespace

TEST(URLRequestJob, TransactionNotifiedWhenDone) {
  MockNetworkLayer network_layer;
  TestURLRequestContext context;
  context.set_http_transaction_factory(&network_layer);

  TestDelegate d;
  TestURLRequest req(
      GURL(kGZip_Transaction.url), DEFAULT_PRIORITY, &d, &context);
  AddMockTransaction(&kGZip_Transaction);

  req.set_method("GET");
  req.Start();

  base::MessageLoop::current()->Run();

  EXPECT_TRUE(network_layer.done_reading_called());

  RemoveMockTransaction(&kGZip_Transaction);
}

TEST(URLRequestJob, SyncTransactionNotifiedWhenDone) {
  MockNetworkLayer network_layer;
  TestURLRequestContext context;
  context.set_http_transaction_factory(&network_layer);

  TestDelegate d;
  TestURLRequest req(
      GURL(kGZip_Transaction.url), DEFAULT_PRIORITY, &d, &context);
  MockTransaction transaction(kGZip_Transaction);
  transaction.test_mode = TEST_MODE_SYNC_ALL;
  AddMockTransaction(&transaction);

  req.set_method("GET");
  req.Start();

  base::RunLoop().Run();

  EXPECT_TRUE(network_layer.done_reading_called());

  RemoveMockTransaction(&transaction);
}

// Tests processing a large gzip header one byte at a time.
TEST(URLRequestJob, SyncSlowTransaction) {
  MockNetworkLayer network_layer;
  TestURLRequestContext context;
  context.set_http_transaction_factory(&network_layer);

  TestDelegate d;
  TestURLRequest req(
      GURL(kGZip_Transaction.url), DEFAULT_PRIORITY, &d, &context);
  MockTransaction transaction(kGZip_Transaction);
  transaction.test_mode = TEST_MODE_SYNC_ALL | TEST_MODE_SLOW_READ;
  transaction.handler = &BigGZipServer;
  AddMockTransaction(&transaction);

  req.set_method("GET");
  req.Start();

  base::RunLoop().Run();

  EXPECT_TRUE(network_layer.done_reading_called());

  RemoveMockTransaction(&transaction);
}

TEST(URLRequestJob, RedirectTransactionNotifiedWhenDone) {
  MockNetworkLayer network_layer;
  TestURLRequestContext context;
  context.set_http_transaction_factory(&network_layer);

  TestDelegate d;
  TestURLRequest req(
      GURL(kRedirect_Transaction.url), DEFAULT_PRIORITY, &d, &context);
  AddMockTransaction(&kRedirect_Transaction);

  req.set_method("GET");
  req.Start();

  base::RunLoop().Run();

  EXPECT_TRUE(network_layer.done_reading_called());

  RemoveMockTransaction(&kRedirect_Transaction);
}

TEST(URLRequestJob, TransactionNotCachedWhenNetworkDelegateRedirects) {
  MockNetworkLayer network_layer;
  TestNetworkDelegate network_delegate;
  network_delegate.set_redirect_on_headers_received_url(GURL("http://foo"));
  TestURLRequestContext context;
  context.set_http_transaction_factory(&network_layer);
  context.set_network_delegate(&network_delegate);

  TestDelegate d;
  TestURLRequest req(GURL(kGZip_Transaction.url), DEFAULT_PRIORITY, &d,
                     &context);
  AddMockTransaction(&kGZip_Transaction);

  req.set_method("GET");
  req.Start();

  base::RunLoop().Run();

  EXPECT_TRUE(network_layer.stop_caching_called());

  RemoveMockTransaction(&kGZip_Transaction);
}

}  // namespace net
