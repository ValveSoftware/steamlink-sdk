// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ip_endpoint.h"

#include "base/strings/string_number_conversions.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#if defined(OS_WIN)
#include <winsock2.h>
#elif defined(OS_POSIX)
#include <netinet/in.h>
#endif

namespace net {

namespace {

struct TestData {
  std::string host;
  std::string host_normalized;
  bool ipv6;
  IPAddressNumber ip_address;
} tests[] = {
  { "127.0.00.1", "127.0.0.1", false},
  { "192.168.1.1", "192.168.1.1", false },
  { "::1", "[::1]", true },
  { "2001:db8:0::42", "[2001:db8::42]", true },
};
int test_count = ARRAYSIZE_UNSAFE(tests);

class IPEndPointTest : public PlatformTest {
 public:
  virtual void SetUp() {
    // This is where we populate the TestData.
    for (int index = 0; index < test_count; ++index) {
      EXPECT_TRUE(ParseIPLiteralToNumber(tests[index].host,
          &tests[index].ip_address));
    }
  }
};

TEST_F(IPEndPointTest, Constructor) {
  IPEndPoint endpoint;
  EXPECT_EQ(0, endpoint.port());

  for (int index = 0; index < test_count; ++index) {
    IPEndPoint endpoint(tests[index].ip_address, 80);
    EXPECT_EQ(80, endpoint.port());
    EXPECT_EQ(tests[index].ip_address, endpoint.address());
  }
}

TEST_F(IPEndPointTest, Assignment) {
  for (int index = 0; index < test_count; ++index) {
    IPEndPoint src(tests[index].ip_address, index);
    IPEndPoint dest = src;

    EXPECT_EQ(src.port(), dest.port());
    EXPECT_EQ(src.address(), dest.address());
  }
}

TEST_F(IPEndPointTest, Copy) {
  for (int index = 0; index < test_count; ++index) {
    IPEndPoint src(tests[index].ip_address, index);
    IPEndPoint dest(src);

    EXPECT_EQ(src.port(), dest.port());
    EXPECT_EQ(src.address(), dest.address());
  }
}

TEST_F(IPEndPointTest, ToFromSockAddr) {
  for (int index = 0; index < test_count; ++index) {
    IPEndPoint ip_endpoint(tests[index].ip_address, index);

    // Convert to a sockaddr.
    SockaddrStorage storage;
    EXPECT_TRUE(ip_endpoint.ToSockAddr(storage.addr, &storage.addr_len));

    // Basic verification.
    socklen_t expected_size = tests[index].ipv6 ?
        sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
    EXPECT_EQ(expected_size, storage.addr_len);
    EXPECT_EQ(ip_endpoint.port(), GetPortFromSockaddr(storage.addr,
                                                      storage.addr_len));

    // And convert back to an IPEndPoint.
    IPEndPoint ip_endpoint2;
    EXPECT_TRUE(ip_endpoint2.FromSockAddr(storage.addr, storage.addr_len));
    EXPECT_EQ(ip_endpoint.port(), ip_endpoint2.port());
    EXPECT_EQ(ip_endpoint.address(), ip_endpoint2.address());
  }
}

TEST_F(IPEndPointTest, ToSockAddrBufTooSmall) {
  for (int index = 0; index < test_count; ++index) {
    IPEndPoint ip_endpoint(tests[index].ip_address, index);

    SockaddrStorage storage;
    storage.addr_len = index;  // size is too small!
    EXPECT_FALSE(ip_endpoint.ToSockAddr(storage.addr, &storage.addr_len));
  }
}

TEST_F(IPEndPointTest, FromSockAddrBufTooSmall) {
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  IPEndPoint ip_endpoint;
  struct sockaddr* sockaddr = reinterpret_cast<struct sockaddr*>(&addr);
  EXPECT_FALSE(ip_endpoint.FromSockAddr(sockaddr, sizeof(addr) - 1));
}

TEST_F(IPEndPointTest, Equality) {
  for (int index = 0; index < test_count; ++index) {
    IPEndPoint src(tests[index].ip_address, index);
    IPEndPoint dest(src);
    EXPECT_TRUE(src == dest);
  }
}

TEST_F(IPEndPointTest, LessThan) {
  // Vary by port.
  IPEndPoint ip_endpoint1(tests[0].ip_address, 100);
  IPEndPoint ip_endpoint2(tests[0].ip_address, 1000);
  EXPECT_TRUE(ip_endpoint1 < ip_endpoint2);
  EXPECT_FALSE(ip_endpoint2 < ip_endpoint1);

  // IPv4 vs IPv6
  ip_endpoint1 = IPEndPoint(tests[0].ip_address, 81);
  ip_endpoint2 = IPEndPoint(tests[2].ip_address, 80);
  EXPECT_TRUE(ip_endpoint1 < ip_endpoint2);
  EXPECT_FALSE(ip_endpoint2 < ip_endpoint1);

  // IPv4 vs IPv4
  ip_endpoint1 = IPEndPoint(tests[0].ip_address, 81);
  ip_endpoint2 = IPEndPoint(tests[1].ip_address, 80);
  EXPECT_TRUE(ip_endpoint1 < ip_endpoint2);
  EXPECT_FALSE(ip_endpoint2 < ip_endpoint1);

  // IPv6 vs IPv6
  ip_endpoint1 = IPEndPoint(tests[2].ip_address, 81);
  ip_endpoint2 = IPEndPoint(tests[3].ip_address, 80);
  EXPECT_TRUE(ip_endpoint1 < ip_endpoint2);
  EXPECT_FALSE(ip_endpoint2 < ip_endpoint1);

  // Compare equivalent endpoints.
  ip_endpoint1 = IPEndPoint(tests[0].ip_address, 80);
  ip_endpoint2 = IPEndPoint(tests[0].ip_address, 80);
  EXPECT_FALSE(ip_endpoint1 < ip_endpoint2);
  EXPECT_FALSE(ip_endpoint2 < ip_endpoint1);
}

TEST_F(IPEndPointTest, ToString) {
  IPEndPoint endpoint;
  EXPECT_EQ(0, endpoint.port());

  for (int index = 0; index < test_count; ++index) {
    int port = 100 + index;
    IPEndPoint endpoint(tests[index].ip_address, port);
    const std::string result = endpoint.ToString();
    EXPECT_EQ(tests[index].host_normalized + ":" + base::IntToString(port),
              result);
  }
}

}  // namespace

}  // namespace net
