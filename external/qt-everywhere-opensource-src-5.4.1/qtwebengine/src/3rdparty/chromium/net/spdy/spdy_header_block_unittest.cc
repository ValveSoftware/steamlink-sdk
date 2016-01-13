// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_header_block.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "net/base/net_log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(SpdyHeaderBlockTest, ToNetLogParamAndBackAgain) {
  SpdyHeaderBlock headers;
  headers["A"] = "a";
  headers["B"] = "b";

  scoped_ptr<base::Value> event_param(
      SpdyHeaderBlockNetLogCallback(&headers, NetLog::LOG_ALL_BUT_BYTES));

  SpdyHeaderBlock headers2;
  ASSERT_TRUE(SpdyHeaderBlockFromNetLogParam(event_param.get(), &headers2));
  EXPECT_EQ(headers, headers2);
}

}  // namespace

}  // namespace net
