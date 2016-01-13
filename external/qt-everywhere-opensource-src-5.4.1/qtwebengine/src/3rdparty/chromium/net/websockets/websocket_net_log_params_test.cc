// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_net_log_params.h"

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(NetLogWebSocketHandshakeParameterTest, ToValue) {
  base::ListValue* list = new base::ListValue();
  list->Append(new base::StringValue("GET /demo HTTP/1.1"));
  list->Append(new base::StringValue("Host: example.com"));
  list->Append(new base::StringValue("Connection: Upgrade"));
  list->Append(new base::StringValue("Sec-WebSocket-Key2: 12998 5 Y3 1  .P00"));
  list->Append(new base::StringValue("Sec-WebSocket-Protocol: sample"));
  list->Append(new base::StringValue("Upgrade: WebSocket"));
  list->Append(new base::StringValue(
      "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5"));
  list->Append(new base::StringValue("Origin: http://example.com"));
  list->Append(new base::StringValue(std::string()));
  list->Append(new base::StringValue(
      "\\x00\\x01\\x0a\\x0d\\xff\\xfe\\x0d\\x0a"));

  base::DictionaryValue expected;
  expected.Set("headers", list);

  const std::string key("\x00\x01\x0a\x0d\xff\xfe\x0d\x0a", 8);
  const std::string testInput =
    "GET /demo HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00\r\n"
    "Sec-WebSocket-Protocol: sample\r\n"
    "Upgrade: WebSocket\r\n"
    "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5\r\n"
    "Origin: http://example.com\r\n"
    "\r\n" +
    key;

  scoped_ptr<base::Value> actual(
      net::NetLogWebSocketHandshakeCallback(&testInput,
                                            net::NetLog::LOG_ALL));

  EXPECT_TRUE(expected.Equals(actual.get()));
}
