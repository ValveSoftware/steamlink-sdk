// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_address_mismatch.h"

#include "net/base/ip_endpoint.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

// Test all cases of the GetAddressMismatch function.
TEST(QuicAddressMismatchTest, GetAddressMismatch) {
  IPAddressNumber ip4_1;
  IPAddressNumber ip4_2;
  IPAddressNumber ip6_1;
  IPAddressNumber ip6_2;
  IPAddressNumber ip4_mapped_1;
  IPAddressNumber ip4_mapped_2;
  ASSERT_TRUE(ParseIPLiteralToNumber("1.2.3.4", &ip4_1));
  ASSERT_TRUE(ParseIPLiteralToNumber("5.6.7.8", &ip4_2));
  ASSERT_TRUE(ParseIPLiteralToNumber("1234::1", &ip6_1));
  ASSERT_TRUE(ParseIPLiteralToNumber("1234::2", &ip6_2));
  ip4_mapped_1 = ConvertIPv4NumberToIPv6Number(ip4_1);
  ip4_mapped_2 = ConvertIPv4NumberToIPv6Number(ip4_2);
  ASSERT_NE(ip4_1, ip4_2);
  ASSERT_NE(ip6_1, ip6_2);
  ASSERT_NE(ip4_mapped_1, ip4_mapped_2);

  EXPECT_EQ(-1, GetAddressMismatch(IPEndPoint(), IPEndPoint()));
  EXPECT_EQ(-1, GetAddressMismatch(IPEndPoint(), IPEndPoint(ip4_1, 443)));
  EXPECT_EQ(-1, GetAddressMismatch(IPEndPoint(ip4_1, 443), IPEndPoint()));

  EXPECT_EQ(QUIC_ADDRESS_AND_PORT_MATCH_V4_V4,
            GetAddressMismatch(IPEndPoint(ip4_1, 443),
                               IPEndPoint(ip4_1, 443)));
  EXPECT_EQ(QUIC_ADDRESS_AND_PORT_MATCH_V4_V4,
            GetAddressMismatch(IPEndPoint(ip4_1, 443),
                               IPEndPoint(ip4_mapped_1, 443)));
  EXPECT_EQ(QUIC_ADDRESS_AND_PORT_MATCH_V4_V4,
            GetAddressMismatch(IPEndPoint(ip4_mapped_1, 443),
                               IPEndPoint(ip4_mapped_1, 443)));
  EXPECT_EQ(QUIC_ADDRESS_AND_PORT_MATCH_V6_V6,
            GetAddressMismatch(IPEndPoint(ip6_1, 443),
                               IPEndPoint(ip6_1, 443)));

  EXPECT_EQ(QUIC_PORT_MISMATCH_V4_V4,
            GetAddressMismatch(IPEndPoint(ip4_1, 80),
                               IPEndPoint(ip4_1, 443)));
  EXPECT_EQ(QUIC_PORT_MISMATCH_V4_V4,
            GetAddressMismatch(IPEndPoint(ip4_1, 80),
                               IPEndPoint(ip4_mapped_1, 443)));
  EXPECT_EQ(QUIC_PORT_MISMATCH_V4_V4,
            GetAddressMismatch(IPEndPoint(ip4_mapped_1, 80),
                               IPEndPoint(ip4_mapped_1, 443)));
  EXPECT_EQ(QUIC_PORT_MISMATCH_V6_V6,
            GetAddressMismatch(IPEndPoint(ip6_1, 80),
                               IPEndPoint(ip6_1, 443)));

  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V4_V4,
            GetAddressMismatch(IPEndPoint(ip4_1, 443),
                               IPEndPoint(ip4_2, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V4_V4,
            GetAddressMismatch(IPEndPoint(ip4_1, 443),
                               IPEndPoint(ip4_mapped_2, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V4_V4,
            GetAddressMismatch(IPEndPoint(ip4_mapped_1, 443),
                               IPEndPoint(ip4_mapped_2, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V4_V4,
            GetAddressMismatch(IPEndPoint(ip4_1, 80),
                               IPEndPoint(ip4_2, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V4_V4,
            GetAddressMismatch(IPEndPoint(ip4_1, 80),
                               IPEndPoint(ip4_mapped_2, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V4_V4,
            GetAddressMismatch(IPEndPoint(ip4_mapped_1, 80),
                               IPEndPoint(ip4_mapped_2, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V6_V6,
            GetAddressMismatch(IPEndPoint(ip6_1, 443),
                               IPEndPoint(ip6_2, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V6_V6,
            GetAddressMismatch(IPEndPoint(ip6_1, 80),
                               IPEndPoint(ip6_2, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V4_V6,
            GetAddressMismatch(IPEndPoint(ip4_1, 443),
                               IPEndPoint(ip6_1, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V4_V6,
            GetAddressMismatch(IPEndPoint(ip4_mapped_1, 443),
                               IPEndPoint(ip6_1, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V4_V6,
            GetAddressMismatch(IPEndPoint(ip4_1, 80),
                               IPEndPoint(ip6_1, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V4_V6,
            GetAddressMismatch(IPEndPoint(ip4_mapped_1, 80),
                               IPEndPoint(ip6_1, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V6_V4,
            GetAddressMismatch(IPEndPoint(ip6_1, 443),
                               IPEndPoint(ip4_1, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V6_V4,
            GetAddressMismatch(IPEndPoint(ip6_1, 443),
                               IPEndPoint(ip4_mapped_1, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V6_V4,
            GetAddressMismatch(IPEndPoint(ip6_1, 80),
                               IPEndPoint(ip4_1, 443)));
  EXPECT_EQ(QUIC_ADDRESS_MISMATCH_V6_V4,
            GetAddressMismatch(IPEndPoint(ip6_1, 80),
                               IPEndPoint(ip4_mapped_1, 443)));
}

}  // namespace test
}  // namespace net
