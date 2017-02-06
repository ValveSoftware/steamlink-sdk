// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_basic_state.h"

#include "net/base/completion_callback.h"
#include "net/base/request_priority.h"
#include "net/http/http_request_info.h"
#include "net/socket/client_socket_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

TEST(HttpBasicStateTest, ConstructsProperly) {
  ClientSocketHandle* const handle = new ClientSocketHandle;
  // Ownership of handle is passed to |state|.
  const HttpBasicState state(handle, true);
  EXPECT_EQ(handle, state.connection());
  EXPECT_TRUE(state.using_proxy());
}

TEST(HttpBasicStateTest, UsingProxyCanBeFalse) {
  const HttpBasicState state(new ClientSocketHandle(), false);
  EXPECT_FALSE(state.using_proxy());
}

TEST(HttpBasicStateTest, ReleaseConnectionWorks) {
  ClientSocketHandle* const handle = new ClientSocketHandle;
  HttpBasicState state(handle, false);
  const std::unique_ptr<ClientSocketHandle> released_connection(
      state.ReleaseConnection());
  EXPECT_EQ(NULL, state.connection());
  EXPECT_EQ(handle, released_connection.get());
}

TEST(HttpBasicStateTest, InitializeWorks) {
  HttpBasicState state(new ClientSocketHandle(), false);
  const HttpRequestInfo request_info;
  EXPECT_EQ(OK,
            state.Initialize(
                &request_info, LOW, BoundNetLog(), CompletionCallback()));
  EXPECT_TRUE(state.parser());
}

TEST(HttpBasicStateTest, DeleteParser) {
  HttpBasicState state(new ClientSocketHandle(), false);
  const HttpRequestInfo request_info;
  state.Initialize(&request_info, LOW, BoundNetLog(), CompletionCallback());
  EXPECT_TRUE(state.parser());
  state.DeleteParser();
  EXPECT_EQ(NULL, state.parser());
}

TEST(HttpBasicStateTest, GenerateRequestLineNoProxy) {
  const bool use_proxy = false;
  HttpBasicState state(new ClientSocketHandle(), use_proxy);
  HttpRequestInfo request_info;
  request_info.url = GURL("http://www.example.com/path?foo=bar#hoge");
  request_info.method = "PUT";
  state.Initialize(&request_info, LOW, BoundNetLog(), CompletionCallback());
  EXPECT_EQ("PUT /path?foo=bar HTTP/1.1\r\n", state.GenerateRequestLine());
}

TEST(HttpBasicStateTest, GenerateRequestLineWithProxy) {
  const bool use_proxy = true;
  HttpBasicState state(new ClientSocketHandle(), use_proxy);
  HttpRequestInfo request_info;
  request_info.url = GURL("http://www.example.com/path?foo=bar#hoge");
  request_info.method = "PUT";
  state.Initialize(&request_info, LOW, BoundNetLog(), CompletionCallback());
  EXPECT_EQ("PUT http://www.example.com/path?foo=bar HTTP/1.1\r\n",
            state.GenerateRequestLine());
}

}  // namespace
}  // namespace net
