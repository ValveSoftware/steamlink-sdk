// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "components/safe_browsing_db/util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {

namespace {

bool VectorContains(const std::vector<std::string>& data,
                    const std::string& str) {
  return std::find(data.begin(), data.end(), str) != data.end();
}

}  // namespace

// Tests that we generate the required host/path combinations for testing
// according to the Safe Browsing spec.
// See section 6.2 in
// http://code.google.com/p/google-safe-browsing/wiki/Protocolv2Spec.
TEST(SafeBrowsingDbUtilTest, UrlParsing) {
  std::vector<std::string> hosts, paths;

  GURL url("http://a.b.c/1/2.html?param=1");
  GenerateHostsToCheck(url, &hosts);
  GeneratePathsToCheck(url, &paths);
  EXPECT_EQ(hosts.size(), static_cast<size_t>(2));
  EXPECT_EQ(paths.size(), static_cast<size_t>(4));
  EXPECT_EQ(hosts[0], "b.c");
  EXPECT_EQ(hosts[1], "a.b.c");

  EXPECT_TRUE(VectorContains(paths, "/1/2.html?param=1"));
  EXPECT_TRUE(VectorContains(paths, "/1/2.html"));
  EXPECT_TRUE(VectorContains(paths, "/1/"));
  EXPECT_TRUE(VectorContains(paths, "/"));

  url = GURL("http://a.b.c.d.e.f.g/1.html");
  GenerateHostsToCheck(url, &hosts);
  GeneratePathsToCheck(url, &paths);
  EXPECT_EQ(hosts.size(), static_cast<size_t>(5));
  EXPECT_EQ(paths.size(), static_cast<size_t>(2));
  EXPECT_EQ(hosts[0], "f.g");
  EXPECT_EQ(hosts[1], "e.f.g");
  EXPECT_EQ(hosts[2], "d.e.f.g");
  EXPECT_EQ(hosts[3], "c.d.e.f.g");
  EXPECT_EQ(hosts[4], "a.b.c.d.e.f.g");
  EXPECT_TRUE(VectorContains(paths, "/1.html"));
  EXPECT_TRUE(VectorContains(paths, "/"));

  url = GURL("http://a.b/saw-cgi/eBayISAPI.dll/");
  GeneratePathsToCheck(url, &paths);
  EXPECT_EQ(paths.size(), static_cast<size_t>(3));
  EXPECT_TRUE(VectorContains(paths, "/saw-cgi/eBayISAPI.dll/"));
  EXPECT_TRUE(VectorContains(paths, "/saw-cgi/"));
  EXPECT_TRUE(VectorContains(paths, "/"));
}

// Tests the url canonicalization according to the Safe Browsing spec.
// See section 6.1 in
// http://code.google.com/p/google-safe-browsing/wiki/Protocolv2Spec.
TEST(SafeBrowsingDbUtilTest, CanonicalizeUrl) {
  struct {
    const char* input_url;
    const char* expected_canonicalized_hostname;
    const char* expected_canonicalized_path;
    const char* expected_canonicalized_query;
  } tests[] = {
    {
      "http://host/%25%32%35",
      "host",
      "/%25",
      ""
    }, {
      "http://host/%25%32%35%25%32%35",
      "host",
      "/%25%25",
      ""
    }, {
      "http://host/%2525252525252525",
      "host",
      "/%25",
      ""
    }, {
      "http://host/asdf%25%32%35asd",
      "host",
      "/asdf%25asd",
      ""
    }, {
      "http://host/%%%25%32%35asd%%",
      "host",
      "/%25%25%25asd%25%25",
      ""
    }, {
      "http://host/%%%25%32%35asd%%",
      "host",
      "/%25%25%25asd%25%25",
      ""
    }, {
      "http://www.google.com/",
      "www.google.com",
      "/",
      ""
    }, {
      "http://%31%36%38%2e%31%38%38%2e%39%39%2e%32%36/%2E%73%65%63%75%72%65/%77"
          "%77%77%2E%65%62%61%79%2E%63%6F%6D/",
      "168.188.99.26",
      "/.secure/www.ebay.com/",
      ""
    }, {
      "http://195.127.0.11/uploads/%20%20%20%20/.verify/.eBaysecure=updateuserd"
          "ataxplimnbqmn-xplmvalidateinfoswqpcmlx=hgplmcx/",
      "195.127.0.11",
      "/uploads/%20%20%20%20/.verify/.eBaysecure=updateuserdataxplimnbqmn-xplmv"
          "alidateinfoswqpcmlx=hgplmcx/",
      ""
    }, {
      "http://host.com/%257Ea%2521b%2540c%2523d%2524e%25f%255E00%252611%252A"
          "22%252833%252944_55%252B",
      "host.com",
      "/~a!b@c%23d$e%25f^00&11*22(33)44_55+",
      ""
    }, {
      "http://3279880203/blah",
      "195.127.0.11",
      "/blah",
      ""
    }, {
      "http://www.google.com/blah/..",
      "www.google.com",
      "/",
      ""
    }, {
      "http://www.google.com/blah#fraq",
      "www.google.com",
      "/blah",
      ""
    }, {
      "http://www.GOOgle.com/",
      "www.google.com",
      "/",
      ""
    }, {
      "http://www.google.com.../",
      "www.google.com",
      "/",
      ""
    }, {
      "http://www.google.com/q?",
      "www.google.com",
      "/q",
      ""
    }, {
      "http://www.google.com/q?r?",
      "www.google.com",
      "/q",
      "r?"
    }, {
      "http://www.google.com/q?r?s",
      "www.google.com",
      "/q",
      "r?s"
    }, {
      "http://evil.com/foo#bar#baz",
      "evil.com",
      "/foo",
      ""
    }, {
      "http://evil.com/foo;",
      "evil.com",
      "/foo;",
      ""
    }, {
      "http://evil.com/foo?bar;",
      "evil.com",
      "/foo",
      "bar;"
    }, {
      "http://notrailingslash.com",
      "notrailingslash.com",
      "/",
      ""
    }, {
      "http://www.gotaport.com:1234/",
      "www.gotaport.com",
      "/",
      ""
    }, {
      "  http://www.google.com/  ",
      "www.google.com",
      "/",
      ""
    }, {
      "http:// leadingspace.com/",
      "%20leadingspace.com",
      "/",
      ""
    }, {
      "http://%20leadingspace.com/",
      "%20leadingspace.com",
      "/",
      ""
    }, {
      "https://www.securesite.com/",
      "www.securesite.com",
      "/",
      ""
    }, {
      "http://host.com/ab%23cd",
      "host.com",
      "/ab%23cd",
      ""
    }, {
      "http://host%3e.com//twoslashes?more//slashes",
      "host>.com",
      "/twoslashes",
      "more//slashes"
    }, {
      "http://host.com/abc?val=xyz#anything",
      "host.com",
      "/abc",
      "val=xyz"
    }, {
      "http://abc:def@host.com/xyz",
      "host.com",
      "/xyz",
      ""
    }, {
      "http://host%3e.com/abc/%2e%2e%2fdef",
      "host>.com",
      "/def",
      ""
    }, {
      "http://.......host...com.....//abc/////def%2F%2F%2Fxyz",
      "host.com",
      "/abc/def/xyz",
      ""
    }, {
      "ftp://host.com/foo?bar",
      "host.com",
      "/foo",
      "bar"
    }, {
      "data:text/html;charset=utf-8,%0D%0A",
      "",
      "",
      ""
    }, {
      "javascript:alert()",
      "",
      "",
      ""
    }, {
      "mailto:abc@example.com",
      "",
      "",
      ""
    },
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    SCOPED_TRACE(base::StringPrintf("Test: %s", tests[i].input_url));
    GURL url(tests[i].input_url);

    std::string canonicalized_hostname;
    std::string canonicalized_path;
    std::string canonicalized_query;
    CanonicalizeUrl(url, &canonicalized_hostname, &canonicalized_path,
                    &canonicalized_query);

    EXPECT_EQ(tests[i].expected_canonicalized_hostname, canonicalized_hostname);
    EXPECT_EQ(tests[i].expected_canonicalized_path, canonicalized_path);
    EXPECT_EQ(tests[i].expected_canonicalized_query, canonicalized_query);
  }
}

TEST(SafeBrowsingDbUtilTest, UrlToFullHashes) {
  std::vector<SBFullHash> results;
  GURL url("http://www.evil.com/evil1/evilness.html");
  UrlToFullHashes(url, false, &results);

  EXPECT_EQ(6UL, results.size());
  EXPECT_TRUE(SBFullHashEqual(SBFullHashForString("evil.com/"),
                                                  results[0]));
  EXPECT_TRUE(SBFullHashEqual(SBFullHashForString("evil.com/evil1/"),
                                                  results[1]));
  EXPECT_TRUE(SBFullHashEqual(
      SBFullHashForString("evil.com/evil1/evilness.html"), results[2]));
  EXPECT_TRUE(SBFullHashEqual(SBFullHashForString("www.evil.com/"),
                                                  results[3]));
  EXPECT_TRUE(SBFullHashEqual(SBFullHashForString("www.evil.com/evil1/"),
                                                  results[4]));
  EXPECT_TRUE(SBFullHashEqual(
      SBFullHashForString("www.evil.com/evil1/evilness.html"), results[5]));

  results.clear();
  GURL url2("http://www.evil.com/evil1/evilness.html");
  UrlToFullHashes(url2, true, &results);

  EXPECT_EQ(8UL, results.size());
  EXPECT_TRUE(SBFullHashEqual(SBFullHashForString("evil.com/"),
                                                  results[0]));
  EXPECT_TRUE(SBFullHashEqual(SBFullHashForString("evil.com/evil1/"),
                                                  results[1]));
  EXPECT_TRUE(SBFullHashEqual(SBFullHashForString("evil.com/evil1"),
                                                  results[2]));
  EXPECT_TRUE(SBFullHashEqual(
      SBFullHashForString("evil.com/evil1/evilness.html"), results[3]));
  EXPECT_TRUE(SBFullHashEqual(SBFullHashForString("www.evil.com/"),
                                                  results[4]));
  EXPECT_TRUE(SBFullHashEqual(SBFullHashForString("www.evil.com/evil1/"),
                                                  results[5]));
  EXPECT_TRUE(SBFullHashEqual(SBFullHashForString("www.evil.com/evil1"),
                                                  results[6]));
  EXPECT_TRUE(SBFullHashEqual(
      SBFullHashForString("www.evil.com/evil1/evilness.html"), results[7]));
}

TEST(SafeBrowsingDbUtilTest, ListIdListNameConversion) {
  std::string list_name;
  EXPECT_FALSE(GetListName(INVALID, &list_name));
  EXPECT_TRUE(GetListName(MALWARE, &list_name));
  EXPECT_EQ(list_name, std::string(kMalwareList));
  EXPECT_EQ(MALWARE, GetListId(list_name));

  EXPECT_TRUE(GetListName(PHISH, &list_name));
  EXPECT_EQ(list_name, std::string(kPhishingList));
  EXPECT_EQ(PHISH, GetListId(list_name));

  EXPECT_TRUE(GetListName(BINURL, &list_name));
  EXPECT_EQ(list_name, std::string(kBinUrlList));
  EXPECT_EQ(BINURL, GetListId(list_name));
}

// Since the ids are saved in file, we need to make sure they don't change.
// Since only the last bit of each id is saved in file together with
// chunkids, this checks only last bit.
TEST(SafeBrowsingDbUtilTest, ListIdVerification) {
  EXPECT_EQ(0, MALWARE % 2);
  EXPECT_EQ(1, PHISH % 2);
  EXPECT_EQ(0, BINURL % 2);
}

TEST(SafeBrowsingDbUtilTest, StringToSBFullHashAndSBFullHashToString) {
  // 31 chars plus the last \0 as full_hash.
  const std::string hash_in = "12345678902234567890323456789012";
  SBFullHash hash_out = StringToSBFullHash(hash_in);
  EXPECT_EQ(0x34333231U, hash_out.prefix);
  EXPECT_EQ(0, memcmp(hash_in.data(), hash_out.full_hash, sizeof(SBFullHash)));

  std::string hash_final = SBFullHashToString(hash_out);
  EXPECT_EQ(hash_in, hash_final);
}

TEST(SafeBrowsingDbUtilTest, FullHashOperators) {
  const SBFullHash kHash1 = SBFullHashForString("one");
  const SBFullHash kHash2 = SBFullHashForString("two");

  EXPECT_TRUE(SBFullHashEqual(kHash1, kHash1));
  EXPECT_TRUE(SBFullHashEqual(kHash2, kHash2));
  EXPECT_FALSE(SBFullHashEqual(kHash1, kHash2));
  EXPECT_FALSE(SBFullHashEqual(kHash2, kHash1));

  EXPECT_FALSE(SBFullHashLess(kHash1, kHash2));
  EXPECT_TRUE(SBFullHashLess(kHash2, kHash1));

  EXPECT_FALSE(SBFullHashLess(kHash1, kHash1));
  EXPECT_FALSE(SBFullHashLess(kHash2, kHash2));
}

}  // namespace safe_browsing
