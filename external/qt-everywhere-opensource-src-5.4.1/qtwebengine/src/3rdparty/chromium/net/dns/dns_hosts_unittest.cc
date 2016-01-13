// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_hosts.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(DnsHostsTest, ParseHosts) {
  std::string contents =
      "127.0.0.1       localhost\tlocalhost.localdomain # standard\n"
      "\n"
      "1.0.0.1 localhost # ignored, first hit above\n"
      "fe00::x example company # ignored, malformed IPv6\n"
      "1.0.0.300 company # ignored, malformed IPv4\n"
      "1.0.0.1 # ignored, missing hostname\n"
      "1.0.0.1\t CoMpANy # normalized to 'company' \n"
      "::1\tlocalhost ip6-localhost ip6-loopback # comment # within a comment\n"
      "\t fe00::0 ip6-localnet\r\n"
      "2048::2 example\n"
      "2048::1 company example # ignored for 'example' \n"
      "127.0.0.1 cache1\n"
      "127.0.0.1 cache2 # should reuse parsed IP\n"
      "256.0.0.0 cache3 # bogus IP should not clear parsed IP cache\n"
      "127.0.0.1 cache4 # should still be reused\n"
      "127.0.0.2 cache5\n"
      "gibberish";

  const struct {
    const char* host;
    AddressFamily family;
    const char* ip;
  } entries[] = {
    { "localhost", ADDRESS_FAMILY_IPV4, "127.0.0.1" },
    { "localhost.localdomain", ADDRESS_FAMILY_IPV4, "127.0.0.1" },
    { "company", ADDRESS_FAMILY_IPV4, "1.0.0.1" },
    { "localhost", ADDRESS_FAMILY_IPV6, "::1" },
    { "ip6-localhost", ADDRESS_FAMILY_IPV6, "::1" },
    { "ip6-loopback", ADDRESS_FAMILY_IPV6, "::1" },
    { "ip6-localnet", ADDRESS_FAMILY_IPV6, "fe00::0" },
    { "company", ADDRESS_FAMILY_IPV6, "2048::1" },
    { "example", ADDRESS_FAMILY_IPV6, "2048::2" },
    { "cache1", ADDRESS_FAMILY_IPV4, "127.0.0.1" },
    { "cache2", ADDRESS_FAMILY_IPV4, "127.0.0.1" },
    { "cache4", ADDRESS_FAMILY_IPV4, "127.0.0.1" },
    { "cache5", ADDRESS_FAMILY_IPV4, "127.0.0.2" },
  };

  DnsHosts expected;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(entries); ++i) {
    DnsHostsKey key(entries[i].host, entries[i].family);
    IPAddressNumber& ip = expected[key];
    ASSERT_TRUE(ip.empty());
    ASSERT_TRUE(ParseIPLiteralToNumber(entries[i].ip, &ip));
    ASSERT_EQ(ip.size(), (entries[i].family == ADDRESS_FAMILY_IPV4) ? 4u : 16u);
  }

  DnsHosts hosts;
  ParseHosts(contents, &hosts);
  ASSERT_EQ(expected, hosts);
}

TEST(DnsHostsTest, HostsParser_Empty) {
  DnsHosts hosts;
  ParseHosts("", &hosts);
  EXPECT_EQ(0u, hosts.size());
}

TEST(DnsHostsTest, HostsParser_OnlyWhitespace) {
  DnsHosts hosts;
  ParseHosts(" ", &hosts);
  EXPECT_EQ(0u, hosts.size());
}

TEST(DnsHostsTest, HostsParser_EndsWithNothing) {
  DnsHosts hosts;
  ParseHosts("127.0.0.1 localhost", &hosts);
  EXPECT_EQ(1u, hosts.size());
}

TEST(DnsHostsTest, HostsParser_EndsWithWhitespace) {
  DnsHosts hosts;
  ParseHosts("127.0.0.1 localhost ", &hosts);
  EXPECT_EQ(1u, hosts.size());
}

TEST(DnsHostsTest, HostsParser_EndsWithComment) {
  DnsHosts hosts;
  ParseHosts("127.0.0.1 localhost # comment", &hosts);
  EXPECT_EQ(1u, hosts.size());
}

TEST(DnsHostsTest, HostsParser_EndsWithNewline) {
  DnsHosts hosts;
  ParseHosts("127.0.0.1 localhost\n", &hosts);
  EXPECT_EQ(1u, hosts.size());
}

TEST(DnsHostsTest, HostsParser_EndsWithTwoNewlines) {
  DnsHosts hosts;
  ParseHosts("127.0.0.1 localhost\n\n", &hosts);
  EXPECT_EQ(1u, hosts.size());
}

TEST(DnsHostsTest, HostsParser_EndsWithNewlineAndWhitespace) {
  DnsHosts hosts;
  ParseHosts("127.0.0.1 localhost\n ", &hosts);
  EXPECT_EQ(1u, hosts.size());
}

TEST(DnsHostsTest, HostsParser_EndsWithNewlineAndToken) {
  DnsHosts hosts;
  ParseHosts("127.0.0.1 localhost\ntoken", &hosts);
  EXPECT_EQ(1u, hosts.size());
}

}  // namespace

}  // namespace net

