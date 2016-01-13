// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

TEST(ProxyInfoTest, ProxyInfoIsDirectOnly) {
  // Test the is_direct_only() predicate.
  ProxyInfo info;

  // An empty ProxyInfo is not considered direct.
  EXPECT_FALSE(info.is_direct_only());

  info.UseDirect();
  EXPECT_TRUE(info.is_direct_only());

  info.UsePacString("DIRECT");
  EXPECT_TRUE(info.is_direct_only());

  info.UsePacString("PROXY myproxy:80");
  EXPECT_FALSE(info.is_direct_only());

  info.UsePacString("DIRECT; PROXY myproxy:80");
  EXPECT_TRUE(info.is_direct());
  EXPECT_FALSE(info.is_direct_only());

  info.UsePacString("PROXY myproxy:80; DIRECT");
  EXPECT_FALSE(info.is_direct());
  EXPECT_FALSE(info.is_direct_only());
  // After falling back to direct, we shouldn't consider it DIRECT only.
  EXPECT_TRUE(info.Fallback(BoundNetLog()));
  EXPECT_TRUE(info.is_direct());
  EXPECT_FALSE(info.is_direct_only());
}

}  // namespace
}  // namespace net
