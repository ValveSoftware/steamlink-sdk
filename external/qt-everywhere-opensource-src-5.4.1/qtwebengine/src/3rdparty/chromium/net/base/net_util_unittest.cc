// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_util.h"

#include <string.h>

#include <ostream>

#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_byteorder.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include <iphlpapi.h>
#include <objbase.h>
#include "base/win/windows_version.h"
#elif !defined(OS_ANDROID)
#include <net/if.h>
#endif  // OS_WIN

using base::ASCIIToUTF16;
using base::WideToUTF16;

namespace net {

namespace {

struct HeaderCase {
  const char* header_name;
  const char* expected;
};

struct CompliantHostCase {
  const char* host;
  const char* desired_tld;
  bool expected_output;
};

// Fills in sockaddr for the given 32-bit address (IPv4.)
// |bytes| should be an array of length 4.
void MakeIPv4Address(const uint8* bytes, int port, SockaddrStorage* storage) {
  memset(&storage->addr_storage, 0, sizeof(storage->addr_storage));
  storage->addr_len = sizeof(struct sockaddr_in);
  struct sockaddr_in* addr4 = reinterpret_cast<sockaddr_in*>(storage->addr);
  addr4->sin_port = base::HostToNet16(port);
  addr4->sin_family = AF_INET;
  memcpy(&addr4->sin_addr, bytes, 4);
}

// Fills in sockaddr for the given 128-bit address (IPv6.)
// |bytes| should be an array of length 16.
void MakeIPv6Address(const uint8* bytes, int port, SockaddrStorage* storage) {
  memset(&storage->addr_storage, 0, sizeof(storage->addr_storage));
  storage->addr_len = sizeof(struct sockaddr_in6);
  struct sockaddr_in6* addr6 = reinterpret_cast<sockaddr_in6*>(storage->addr);
  addr6->sin6_port = base::HostToNet16(port);
  addr6->sin6_family = AF_INET6;
  memcpy(&addr6->sin6_addr, bytes, 16);
}

// Helper to strignize an IP number (used to define expectations).
std::string DumpIPNumber(const IPAddressNumber& v) {
  std::string out;
  for (size_t i = 0; i < v.size(); ++i) {
    if (i != 0)
      out.append(",");
    out.append(base::IntToString(static_cast<int>(v[i])));
  }
  return out;
}

}  // anonymous namespace

TEST(NetUtilTest, GetIdentityFromURL) {
  struct {
    const char* input_url;
    const char* expected_username;
    const char* expected_password;
  } tests[] = {
    {
      "http://username:password@google.com",
      "username",
      "password",
    },
    { // Test for http://crbug.com/19200
      "http://username:p@ssword@google.com",
      "username",
      "p@ssword",
    },
    { // Special URL characters should be unescaped.
      "http://username:p%3fa%26s%2fs%23@google.com",
      "username",
      "p?a&s/s#",
    },
    { // Username contains %20.
      "http://use rname:password@google.com",
      "use rname",
      "password",
    },
    { // Keep %00 as is.
      "http://use%00rname:password@google.com",
      "use%00rname",
      "password",
    },
    { // Use a '+' in the username.
      "http://use+rname:password@google.com",
      "use+rname",
      "password",
    },
    { // Use a '&' in the password.
      "http://username:p&ssword@google.com",
      "username",
      "p&ssword",
    },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i,
                                    tests[i].input_url));
    GURL url(tests[i].input_url);

    base::string16 username, password;
    GetIdentityFromURL(url, &username, &password);

    EXPECT_EQ(ASCIIToUTF16(tests[i].expected_username), username);
    EXPECT_EQ(ASCIIToUTF16(tests[i].expected_password), password);
  }
}

// Try extracting a username which was encoded with UTF8.
TEST(NetUtilTest, GetIdentityFromURL_UTF8) {
  GURL url(WideToUTF16(L"http://foo:\x4f60\x597d@blah.com"));

  EXPECT_EQ("foo", url.username());
  EXPECT_EQ("%E4%BD%A0%E5%A5%BD", url.password());

  // Extract the unescaped identity.
  base::string16 username, password;
  GetIdentityFromURL(url, &username, &password);

  // Verify that it was decoded as UTF8.
  EXPECT_EQ(ASCIIToUTF16("foo"), username);
  EXPECT_EQ(WideToUTF16(L"\x4f60\x597d"), password);
}

// Just a bunch of fake headers.
const char* google_headers =
    "HTTP/1.1 200 OK\n"
    "Content-TYPE: text/html; charset=utf-8\n"
    "Content-disposition: attachment; filename=\"download.pdf\"\n"
    "Content-Length: 378557\n"
    "X-Google-Google1: 314159265\n"
    "X-Google-Google2: aaaa2:7783,bbb21:9441\n"
    "X-Google-Google4: home\n"
    "Transfer-Encoding: chunked\n"
    "Set-Cookie: HEHE_AT=6666x66beef666x6-66xx6666x66; Path=/mail\n"
    "Set-Cookie: HEHE_HELP=owned:0;Path=/\n"
    "Set-Cookie: S=gmail=Xxx-beefbeefbeef_beefb:gmail_yj=beefbeef000beefbee"
       "fbee:gmproxy=bee-fbeefbe; Domain=.google.com; Path=/\n"
    "X-Google-Google2: /one/two/three/four/five/six/seven-height/nine:9411\n"
    "Server: GFE/1.3\n"
    "Transfer-Encoding: chunked\n"
    "Date: Mon, 13 Nov 2006 21:38:09 GMT\n"
    "Expires: Tue, 14 Nov 2006 19:23:58 GMT\n"
    "X-Malformed: bla; arg=test\"\n"
    "X-Malformed2: bla; arg=\n"
    "X-Test: bla; arg1=val1; arg2=val2";

TEST(NetUtilTest, GetSpecificHeader) {
  const HeaderCase tests[] = {
    {"content-type", "text/html; charset=utf-8"},
    {"CONTENT-LENGTH", "378557"},
    {"Date", "Mon, 13 Nov 2006 21:38:09 GMT"},
    {"Bad-Header", ""},
    {"", ""},
  };

  // Test first with google_headers.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string result =
        GetSpecificHeader(google_headers, tests[i].header_name);
    EXPECT_EQ(result, tests[i].expected);
  }

  // Test again with empty headers.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string result = GetSpecificHeader(std::string(), tests[i].header_name);
    EXPECT_EQ(result, std::string());
  }
}

TEST(NetUtilTest, CompliantHost) {
  const CompliantHostCase compliant_host_cases[] = {
    {"", "", false},
    {"a", "", true},
    {"-", "", false},
    {".", "", false},
    {"9", "", true},
    {"9a", "", true},
    {"a.", "", true},
    {"a.a", "", true},
    {"9.a", "", true},
    {"a.9", "", true},
    {"_9a", "", false},
    {"-9a", "", false},
    {"-9a", "a", true},
    {"a.a9", "", true},
    {"a.-a9", "", false},
    {"a+9a", "", false},
    {"-a.a9", "", true},
    {"1-.a-b", "", true},
    {"1_.a-b", "", false},
    {"1-2.a_b", "", true},
    {"a.b.c.d.e", "", true},
    {"1.2.3.4.5", "", true},
    {"1.2.3.4.5.", "", true},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(compliant_host_cases); ++i) {
    EXPECT_EQ(compliant_host_cases[i].expected_output,
        IsCanonicalizedHostCompliant(compliant_host_cases[i].host,
                                     compliant_host_cases[i].desired_tld));
  }
}

TEST(NetUtilTest, ParseHostAndPort) {
  const struct {
    const char* input;
    bool success;
    const char* expected_host;
    int expected_port;
  } tests[] = {
    // Valid inputs:
    {"foo:10", true, "foo", 10},
    {"foo", true, "foo", -1},
    {
      "[1080:0:0:0:8:800:200C:4171]:11",
      true,
      "[1080:0:0:0:8:800:200C:4171]",
      11,
    },
    // Invalid inputs:
    {"foo:bar", false, "", -1},
    {"foo:", false, "", -1},
    {":", false, "", -1},
    {":80", false, "", -1},
    {"", false, "", -1},
    {"porttoolong:300000", false, "", -1},
    {"usrname@host", false, "", -1},
    {"usrname:password@host", false, "", -1},
    {":password@host", false, "", -1},
    {":password@host:80", false, "", -1},
    {":password@host", false, "", -1},
    {"@host", false, "", -1},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string host;
    int port;
    bool ok = ParseHostAndPort(tests[i].input, &host, &port);

    EXPECT_EQ(tests[i].success, ok);

    if (tests[i].success) {
      EXPECT_EQ(tests[i].expected_host, host);
      EXPECT_EQ(tests[i].expected_port, port);
    }
  }
}

TEST(NetUtilTest, GetHostAndPort) {
  const struct {
    GURL url;
    const char* expected_host_and_port;
  } tests[] = {
    { GURL("http://www.foo.com/x"), "www.foo.com:80"},
    { GURL("http://www.foo.com:21/x"), "www.foo.com:21"},

    // For IPv6 literals should always include the brackets.
    { GURL("http://[1::2]/x"), "[1::2]:80"},
    { GURL("http://[::a]:33/x"), "[::a]:33"},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string host_and_port = GetHostAndPort(tests[i].url);
    EXPECT_EQ(std::string(tests[i].expected_host_and_port), host_and_port);
  }
}

TEST(NetUtilTest, GetHostAndOptionalPort) {
  const struct {
    GURL url;
    const char* expected_host_and_port;
  } tests[] = {
    { GURL("http://www.foo.com/x"), "www.foo.com"},
    { GURL("http://www.foo.com:21/x"), "www.foo.com:21"},

    // For IPv6 literals should always include the brackets.
    { GURL("http://[1::2]/x"), "[1::2]"},
    { GURL("http://[::a]:33/x"), "[::a]:33"},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string host_and_port = GetHostAndOptionalPort(tests[i].url);
    EXPECT_EQ(std::string(tests[i].expected_host_and_port), host_and_port);
  }
}

TEST(NetUtilTest, IPAddressToString) {
  uint8 addr1[4] = {0, 0, 0, 0};
  EXPECT_EQ("0.0.0.0", IPAddressToString(addr1, sizeof(addr1)));

  uint8 addr2[4] = {192, 168, 0, 1};
  EXPECT_EQ("192.168.0.1", IPAddressToString(addr2, sizeof(addr2)));

  uint8 addr3[16] = {0xFE, 0xDC, 0xBA, 0x98};
  EXPECT_EQ("fedc:ba98::", IPAddressToString(addr3, sizeof(addr3)));
}

TEST(NetUtilTest, IPAddressToStringWithPort) {
  uint8 addr1[4] = {0, 0, 0, 0};
  EXPECT_EQ("0.0.0.0:3", IPAddressToStringWithPort(addr1, sizeof(addr1), 3));

  uint8 addr2[4] = {192, 168, 0, 1};
  EXPECT_EQ("192.168.0.1:99",
            IPAddressToStringWithPort(addr2, sizeof(addr2), 99));

  uint8 addr3[16] = {0xFE, 0xDC, 0xBA, 0x98};
  EXPECT_EQ("[fedc:ba98::]:8080",
            IPAddressToStringWithPort(addr3, sizeof(addr3), 8080));
}

TEST(NetUtilTest, NetAddressToString_IPv4) {
  const struct {
    uint8 addr[4];
    const char* result;
  } tests[] = {
    {{0, 0, 0, 0}, "0.0.0.0"},
    {{127, 0, 0, 1}, "127.0.0.1"},
    {{192, 168, 0, 1}, "192.168.0.1"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    SockaddrStorage storage;
    MakeIPv4Address(tests[i].addr, 80, &storage);
    std::string result = NetAddressToString(storage.addr, storage.addr_len);
    EXPECT_EQ(std::string(tests[i].result), result);
  }
}

TEST(NetUtilTest, NetAddressToString_IPv6) {
  const struct {
    uint8 addr[16];
    const char* result;
  } tests[] = {
    {{0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10, 0xFE, 0xDC, 0xBA,
      0x98, 0x76, 0x54, 0x32, 0x10},
     "fedc:ba98:7654:3210:fedc:ba98:7654:3210"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    SockaddrStorage storage;
    MakeIPv6Address(tests[i].addr, 80, &storage);
    EXPECT_EQ(std::string(tests[i].result),
        NetAddressToString(storage.addr, storage.addr_len));
  }
}

TEST(NetUtilTest, NetAddressToStringWithPort_IPv4) {
  uint8 addr[] = {127, 0, 0, 1};
  SockaddrStorage storage;
  MakeIPv4Address(addr, 166, &storage);
  std::string result = NetAddressToStringWithPort(storage.addr,
                                                  storage.addr_len);
  EXPECT_EQ("127.0.0.1:166", result);
}

TEST(NetUtilTest, NetAddressToStringWithPort_IPv6) {
  uint8 addr[] = {
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10, 0xFE, 0xDC, 0xBA,
      0x98, 0x76, 0x54, 0x32, 0x10
  };
  SockaddrStorage storage;
  MakeIPv6Address(addr, 361, &storage);
  std::string result = NetAddressToStringWithPort(storage.addr,
                                                  storage.addr_len);

  // May fail on systems that don't support IPv6.
  if (!result.empty())
    EXPECT_EQ("[fedc:ba98:7654:3210:fedc:ba98:7654:3210]:361", result);
}

TEST(NetUtilTest, GetHostName) {
  // We can't check the result of GetHostName() directly, since the result
  // will differ across machines. Our goal here is to simply exercise the
  // code path, and check that things "look about right".
  std::string hostname = GetHostName();
  EXPECT_FALSE(hostname.empty());
}

TEST(NetUtilTest, SimplifyUrlForRequest) {
  struct {
    const char* input_url;
    const char* expected_simplified_url;
  } tests[] = {
    {
      // Reference section should be stripped.
      "http://www.google.com:78/foobar?query=1#hash",
      "http://www.google.com:78/foobar?query=1",
    },
    {
      // Reference section can itself contain #.
      "http://192.168.0.1?query=1#hash#10#11#13#14",
      "http://192.168.0.1?query=1",
    },
    { // Strip username/password.
      "http://user:pass@google.com",
      "http://google.com/",
    },
    { // Strip both the reference and the username/password.
      "http://user:pass@google.com:80/sup?yo#X#X",
      "http://google.com/sup?yo",
    },
    { // Try an HTTPS URL -- strip both the reference and the username/password.
      "https://user:pass@google.com:80/sup?yo#X#X",
      "https://google.com:80/sup?yo",
    },
    { // Try an FTP URL -- strip both the reference and the username/password.
      "ftp://user:pass@google.com:80/sup?yo#X#X",
      "ftp://google.com:80/sup?yo",
    },
    { // Try a nonstandard URL
      "foobar://user:pass@google.com:80/sup?yo#X#X",
      "foobar://user:pass@google.com:80/sup?yo",
    },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i,
                                    tests[i].input_url));
    GURL input_url(GURL(tests[i].input_url));
    GURL expected_url(GURL(tests[i].expected_simplified_url));
    EXPECT_EQ(expected_url, SimplifyUrlForRequest(input_url));
  }
}

TEST(NetUtilTest, SetExplicitlyAllowedPortsTest) {
  std::string invalid[] = { "1,2,a", "'1','2'", "1, 2, 3", "1 0,11,12" };
  std::string valid[] = { "", "1", "1,2", "1,2,3", "10,11,12,13" };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(invalid); ++i) {
    SetExplicitlyAllowedPorts(invalid[i]);
    EXPECT_EQ(0, static_cast<int>(GetCountOfExplicitlyAllowedPorts()));
  }

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(valid); ++i) {
    SetExplicitlyAllowedPorts(valid[i]);
    EXPECT_EQ(i, GetCountOfExplicitlyAllowedPorts());
  }
}

TEST(NetUtilTest, GetHostOrSpecFromURL) {
  EXPECT_EQ("example.com",
            GetHostOrSpecFromURL(GURL("http://example.com/test")));
  EXPECT_EQ("example.com",
            GetHostOrSpecFromURL(GURL("http://example.com./test")));
  EXPECT_EQ("file:///tmp/test.html",
            GetHostOrSpecFromURL(GURL("file:///tmp/test.html")));
}

TEST(NetUtilTest, GetAddressFamily) {
  IPAddressNumber number;
  EXPECT_TRUE(ParseIPLiteralToNumber("192.168.0.1", &number));
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, GetAddressFamily(number));
  EXPECT_TRUE(ParseIPLiteralToNumber("1:abcd::3:4:ff", &number));
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, GetAddressFamily(number));
}

// Test that invalid IP literals fail to parse.
TEST(NetUtilTest, ParseIPLiteralToNumber_FailParse) {
  IPAddressNumber number;

  EXPECT_FALSE(ParseIPLiteralToNumber("bad value", &number));
  EXPECT_FALSE(ParseIPLiteralToNumber("bad:value", &number));
  EXPECT_FALSE(ParseIPLiteralToNumber(std::string(), &number));
  EXPECT_FALSE(ParseIPLiteralToNumber("192.168.0.1:30", &number));
  EXPECT_FALSE(ParseIPLiteralToNumber("  192.168.0.1  ", &number));
  EXPECT_FALSE(ParseIPLiteralToNumber("[::1]", &number));
}

// Test parsing an IPv4 literal.
TEST(NetUtilTest, ParseIPLiteralToNumber_IPv4) {
  IPAddressNumber number;
  EXPECT_TRUE(ParseIPLiteralToNumber("192.168.0.1", &number));
  EXPECT_EQ("192,168,0,1", DumpIPNumber(number));
  EXPECT_EQ("192.168.0.1", IPAddressToString(number));
}

// Test parsing an IPv6 literal.
TEST(NetUtilTest, ParseIPLiteralToNumber_IPv6) {
  IPAddressNumber number;
  EXPECT_TRUE(ParseIPLiteralToNumber("1:abcd::3:4:ff", &number));
  EXPECT_EQ("0,1,171,205,0,0,0,0,0,0,0,3,0,4,0,255", DumpIPNumber(number));
  EXPECT_EQ("1:abcd::3:4:ff", IPAddressToString(number));
}

// Test mapping an IPv4 address to an IPv6 address.
TEST(NetUtilTest, ConvertIPv4NumberToIPv6Number) {
  IPAddressNumber ipv4_number;
  EXPECT_TRUE(ParseIPLiteralToNumber("192.168.0.1", &ipv4_number));

  IPAddressNumber ipv6_number =
      ConvertIPv4NumberToIPv6Number(ipv4_number);

  // ::ffff:192.168.0.1
  EXPECT_EQ("0,0,0,0,0,0,0,0,0,0,255,255,192,168,0,1",
            DumpIPNumber(ipv6_number));
  EXPECT_EQ("::ffff:c0a8:1", IPAddressToString(ipv6_number));
}

TEST(NetUtilTest, IsIPv4Mapped) {
  IPAddressNumber ipv4_number;
  EXPECT_TRUE(ParseIPLiteralToNumber("192.168.0.1", &ipv4_number));
  EXPECT_FALSE(IsIPv4Mapped(ipv4_number));

  IPAddressNumber ipv6_number;
  EXPECT_TRUE(ParseIPLiteralToNumber("::1", &ipv4_number));
  EXPECT_FALSE(IsIPv4Mapped(ipv6_number));

  IPAddressNumber ipv4mapped_number;
  EXPECT_TRUE(ParseIPLiteralToNumber("::ffff:0101:1", &ipv4mapped_number));
  EXPECT_TRUE(IsIPv4Mapped(ipv4mapped_number));
}

TEST(NetUtilTest, ConvertIPv4MappedToIPv4) {
  IPAddressNumber ipv4mapped_number;
  EXPECT_TRUE(ParseIPLiteralToNumber("::ffff:0101:1", &ipv4mapped_number));
  IPAddressNumber expected;
  EXPECT_TRUE(ParseIPLiteralToNumber("1.1.0.1", &expected));
  IPAddressNumber result = ConvertIPv4MappedToIPv4(ipv4mapped_number);
  EXPECT_EQ(expected, result);
}

// Test parsing invalid CIDR notation literals.
TEST(NetUtilTest, ParseCIDRBlock_Invalid) {
  const char* bad_literals[] = {
      "foobar",
      "",
      "192.168.0.1",
      "::1",
      "/",
      "/1",
      "1",
      "192.168.1.1/-1",
      "192.168.1.1/33",
      "::1/-3",
      "a::3/129",
      "::1/x",
      "192.168.0.1//11"
  };

  for (size_t i = 0; i < arraysize(bad_literals); ++i) {
    IPAddressNumber ip_number;
    size_t prefix_length_in_bits;

    EXPECT_FALSE(ParseCIDRBlock(bad_literals[i],
                                     &ip_number,
                                     &prefix_length_in_bits));
  }
}

// Test parsing a valid CIDR notation literal.
TEST(NetUtilTest, ParseCIDRBlock_Valid) {
  IPAddressNumber ip_number;
  size_t prefix_length_in_bits;

  EXPECT_TRUE(ParseCIDRBlock("192.168.0.1/11",
                                  &ip_number,
                                  &prefix_length_in_bits));

  EXPECT_EQ("192,168,0,1", DumpIPNumber(ip_number));
  EXPECT_EQ(11u, prefix_length_in_bits);
}

TEST(NetUtilTest, IPNumberMatchesPrefix) {
  struct {
    const char* cidr_literal;
    const char* ip_literal;
    bool expected_to_match;
  } tests[] = {
    // IPv4 prefix with IPv4 inputs.
    {
      "10.10.1.32/27",
      "10.10.1.44",
      true
    },
    {
      "10.10.1.32/27",
      "10.10.1.90",
      false
    },
    {
      "10.10.1.32/27",
      "10.10.1.90",
      false
    },

    // IPv6 prefix with IPv6 inputs.
    {
      "2001:db8::/32",
      "2001:DB8:3:4::5",
      true
    },
    {
      "2001:db8::/32",
      "2001:c8::",
      false
    },

    // IPv6 prefix with IPv4 inputs.
    {
      "2001:db8::/33",
      "192.168.0.1",
      false
    },
    {
      "::ffff:192.168.0.1/112",
      "192.168.33.77",
      true
    },

    // IPv4 prefix with IPv6 inputs.
    {
      "10.11.33.44/16",
      "::ffff:0a0b:89",
      true
    },
    {
      "10.11.33.44/16",
      "::ffff:10.12.33.44",
      false
    },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s, %s", i,
                                    tests[i].cidr_literal,
                                    tests[i].ip_literal));

    IPAddressNumber ip_number;
    EXPECT_TRUE(ParseIPLiteralToNumber(tests[i].ip_literal, &ip_number));

    IPAddressNumber ip_prefix;
    size_t prefix_length_in_bits;

    EXPECT_TRUE(ParseCIDRBlock(tests[i].cidr_literal,
                               &ip_prefix,
                               &prefix_length_in_bits));

    EXPECT_EQ(tests[i].expected_to_match,
              IPNumberMatchesPrefix(ip_number,
                                    ip_prefix,
                                    prefix_length_in_bits));
  }
}

TEST(NetUtilTest, IsLocalhost) {
  EXPECT_TRUE(net::IsLocalhost("localhost"));
  EXPECT_TRUE(net::IsLocalhost("localhost.localdomain"));
  EXPECT_TRUE(net::IsLocalhost("localhost6"));
  EXPECT_TRUE(net::IsLocalhost("localhost6.localdomain6"));
  EXPECT_TRUE(net::IsLocalhost("127.0.0.1"));
  EXPECT_TRUE(net::IsLocalhost("127.0.1.0"));
  EXPECT_TRUE(net::IsLocalhost("127.1.0.0"));
  EXPECT_TRUE(net::IsLocalhost("127.0.0.255"));
  EXPECT_TRUE(net::IsLocalhost("127.0.255.0"));
  EXPECT_TRUE(net::IsLocalhost("127.255.0.0"));
  EXPECT_TRUE(net::IsLocalhost("::1"));
  EXPECT_TRUE(net::IsLocalhost("0:0:0:0:0:0:0:1"));

  EXPECT_FALSE(net::IsLocalhost("localhostx"));
  EXPECT_FALSE(net::IsLocalhost("foo.localdomain"));
  EXPECT_FALSE(net::IsLocalhost("localhost6x"));
  EXPECT_FALSE(net::IsLocalhost("localhost.localdomain6"));
  EXPECT_FALSE(net::IsLocalhost("localhost6.localdomain"));
  EXPECT_FALSE(net::IsLocalhost("127.0.0.1.1"));
  EXPECT_FALSE(net::IsLocalhost(".127.0.0.255"));
  EXPECT_FALSE(net::IsLocalhost("::2"));
  EXPECT_FALSE(net::IsLocalhost("::1:1"));
  EXPECT_FALSE(net::IsLocalhost("0:0:0:0:1:0:0:1"));
  EXPECT_FALSE(net::IsLocalhost("::1:1"));
  EXPECT_FALSE(net::IsLocalhost("0:0:0:0:0:0:0:0:1"));
}

// Verify GetNetworkList().
TEST(NetUtilTest, GetNetworkList) {
  NetworkInterfaceList list;
  ASSERT_TRUE(GetNetworkList(&list, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES));
  for (NetworkInterfaceList::iterator it = list.begin();
       it != list.end(); ++it) {
    // Verify that the names are not empty.
    EXPECT_FALSE(it->name.empty());
    EXPECT_FALSE(it->friendly_name.empty());

    // Verify that the address is correct.
    EXPECT_TRUE(it->address.size() == kIPv4AddressSize ||
                it->address.size() == kIPv6AddressSize)
        << "Invalid address of size " << it->address.size();
    bool all_zeroes = true;
    for (size_t i = 0; i < it->address.size(); ++i) {
      if (it->address[i] != 0) {
        all_zeroes = false;
        break;
      }
    }
    EXPECT_FALSE(all_zeroes);
    EXPECT_GT(it->network_prefix, 1u);
    EXPECT_LE(it->network_prefix, it->address.size() * 8);

#if defined(OS_WIN)
    // On Windows |name| is NET_LUID.
    base::ScopedNativeLibrary phlpapi_lib(
        base::FilePath(FILE_PATH_LITERAL("iphlpapi.dll")));
    ASSERT_TRUE(phlpapi_lib.is_valid());
    typedef NETIO_STATUS (WINAPI* ConvertInterfaceIndexToLuid)(NET_IFINDEX,
                                                               PNET_LUID);
    ConvertInterfaceIndexToLuid interface_to_luid =
        reinterpret_cast<ConvertInterfaceIndexToLuid>(
            phlpapi_lib.GetFunctionPointer("ConvertInterfaceIndexToLuid"));

    typedef NETIO_STATUS (WINAPI* ConvertInterfaceLuidToGuid)(NET_LUID*,
                                                              GUID*);
    ConvertInterfaceLuidToGuid luid_to_guid =
        reinterpret_cast<ConvertInterfaceLuidToGuid>(
            phlpapi_lib.GetFunctionPointer("ConvertInterfaceLuidToGuid"));

    if (interface_to_luid && luid_to_guid) {
      NET_LUID luid;
      EXPECT_EQ(interface_to_luid(it->interface_index, &luid), NO_ERROR);
      GUID guid;
      EXPECT_EQ(luid_to_guid(&luid, &guid), NO_ERROR);
      LPOLESTR name;
      StringFromCLSID(guid, &name);
      EXPECT_STREQ(base::UTF8ToWide(it->name).c_str(), name);
      CoTaskMemFree(name);
      continue;
    } else {
      EXPECT_LT(base::win::GetVersion(), base::win::VERSION_VISTA);
      EXPECT_LT(it->interface_index, 1u << 24u);  // Must fit 0.x.x.x.
      EXPECT_NE(it->interface_index, 0u);  // 0 means to use default.
    }
    if (it->type == NetworkChangeNotifier::CONNECTION_WIFI) {
      EXPECT_NE(WIFI_PHY_LAYER_PROTOCOL_NONE, GetWifiPHYLayerProtocol());
    }
#elif !defined(OS_ANDROID)
    char name[IF_NAMESIZE];
    EXPECT_TRUE(if_indextoname(it->interface_index, name));
    EXPECT_STREQ(it->name.c_str(), name);
#endif
  }
}

struct NonUniqueNameTestData {
  bool is_unique;
  const char* hostname;
};

// Google Test pretty-printer.
void PrintTo(const NonUniqueNameTestData& data, std::ostream* os) {
  ASSERT_TRUE(data.hostname);
  *os << " hostname: " << testing::PrintToString(data.hostname)
      << "; is_unique: " << testing::PrintToString(data.is_unique);
}

const NonUniqueNameTestData kNonUniqueNameTestData[] = {
    // Domains under ICANN-assigned domains.
    { true, "google.com" },
    { true, "google.co.uk" },
    // Domains under private registries.
    { true, "appspot.com" },
    { true, "test.appspot.com" },
    // Unreserved IPv4 addresses (in various forms).
    { true, "8.8.8.8" },
    { true, "99.64.0.0" },
    { true, "212.15.0.0" },
    { true, "212.15" },
    { true, "212.15.0" },
    { true, "3557752832" },
    // Reserved IPv4 addresses (in various forms).
    { false, "192.168.0.0" },
    { false, "192.168.0.6" },
    { false, "10.0.0.5" },
    { false, "10.0" },
    { false, "10.0.0" },
    { false, "3232235526" },
    // Unreserved IPv6 addresses.
    { true, "FFC0:ba98:7654:3210:FEDC:BA98:7654:3210" },
    { true, "2000:ba98:7654:2301:EFCD:BA98:7654:3210" },
    // Reserved IPv6 addresses.
    { false, "::192.9.5.5" },
    { false, "FEED::BEEF" },
    { false, "FEC0:ba98:7654:3210:FEDC:BA98:7654:3210" },
    // 'internal'/non-IANA assigned domains.
    { false, "intranet" },
    { false, "intranet." },
    { false, "intranet.example" },
    { false, "host.intranet.example" },
    // gTLDs under discussion, but not yet assigned.
    { false, "intranet.corp" },
    { false, "example.tech" },
    { false, "intranet.internal" },
    // Invalid host names are treated as unique - but expected to be
    // filtered out before then.
    { true, "junk)(£)$*!@~#" },
    { true, "w$w.example.com" },
    { true, "nocolonsallowed:example" },
    { true, "[::4.5.6.9]" },
};

class NetUtilNonUniqueNameTest
    : public testing::TestWithParam<NonUniqueNameTestData> {
 public:
  virtual ~NetUtilNonUniqueNameTest() {}

 protected:
  bool IsUnique(const std::string& hostname) {
    return !IsHostnameNonUnique(hostname);
  }
};

// Test that internal/non-unique names are properly identified as such, but
// that IP addresses and hosts beneath registry-controlled domains are flagged
// as unique names.
TEST_P(NetUtilNonUniqueNameTest, IsHostnameNonUnique) {
  const NonUniqueNameTestData& test_data = GetParam();

  EXPECT_EQ(test_data.is_unique, IsUnique(test_data.hostname));
}

INSTANTIATE_TEST_CASE_P(, NetUtilNonUniqueNameTest,
                        testing::ValuesIn(kNonUniqueNameTestData));

}  // namespace net
