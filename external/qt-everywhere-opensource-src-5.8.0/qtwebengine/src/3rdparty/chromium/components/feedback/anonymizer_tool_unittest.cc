// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/anonymizer_tool.h"

#include <gtest/gtest.h>

#include "base/strings/string_util.h"

namespace feedback {

class AnonymizerToolTest : public testing::Test {
 protected:
  std::string AnonymizeMACAddresses(const std::string& input) {
    return anonymizer_.AnonymizeMACAddresses(input);
  }

  std::string AnonymizeCustomPatterns(const std::string& input) {
    return anonymizer_.AnonymizeCustomPatterns(input);
  }

  std::string AnonymizeCustomPatternWithContext(
      const std::string& input,
      const std::string& pattern,
      std::map<std::string, std::string>* space) {
    return anonymizer_.AnonymizeCustomPatternWithContext(input, pattern, space);
  }

  std::string AnonymizeCustomPatternWithoutContext(
      const std::string& input,
      const CustomPatternWithoutContext& pattern,
      std::map<std::string, std::string>* space) {
    return anonymizer_.AnonymizeCustomPatternWithoutContext(input, pattern,
                                                            space);
  }

  AnonymizerTool anonymizer_;
};

TEST_F(AnonymizerToolTest, Anonymize) {
  EXPECT_EQ("", anonymizer_.Anonymize(""));
  EXPECT_EQ("foo\nbar\n", anonymizer_.Anonymize("foo\nbar\n"));

  // Make sure MAC address anonymization is invoked.
  EXPECT_EQ("02:46:8a:00:00:01", anonymizer_.Anonymize("02:46:8a:ce:13:57"));

  // Make sure custom pattern anonymization is invoked.
  EXPECT_EQ("Cell ID: '1'", AnonymizeCustomPatterns("Cell ID: 'A1B2'"));
}

TEST_F(AnonymizerToolTest, AnonymizeMACAddresses) {
  EXPECT_EQ("", AnonymizeMACAddresses(""));
  EXPECT_EQ("foo\nbar\n", AnonymizeMACAddresses("foo\nbar\n"));
  EXPECT_EQ("11:22:33:44:55", AnonymizeMACAddresses("11:22:33:44:55"));
  EXPECT_EQ("aa:bb:cc:00:00:01", AnonymizeMACAddresses("aa:bb:cc:dd:ee:ff"));
  EXPECT_EQ(
      "BSSID: aa:bb:cc:00:00:01 in the middle\n"
      "bb:cc:dd:00:00:02 start of line\n"
      "end of line aa:bb:cc:00:00:01\n"
      "no match across lines aa:bb:cc:\n"
      "dd:ee:ff two on the same line:\n"
      "x bb:cc:dd:00:00:02 cc:dd:ee:00:00:03 x\n",
      AnonymizeMACAddresses("BSSID: aa:bb:cc:dd:ee:ff in the middle\n"
                            "bb:cc:dd:ee:ff:00 start of line\n"
                            "end of line aa:bb:cc:dd:ee:ff\n"
                            "no match across lines aa:bb:cc:\n"
                            "dd:ee:ff two on the same line:\n"
                            "x bb:cc:dd:ee:ff:00 cc:dd:ee:ff:00:11 x\n"));
  EXPECT_EQ("Remember bb:cc:dd:00:00:02?",
            AnonymizeMACAddresses("Remember bB:Cc:DD:ee:ff:00?"));
}

TEST_F(AnonymizerToolTest, AnonymizeCustomPatterns) {
  EXPECT_EQ("", AnonymizeCustomPatterns(""));

  EXPECT_EQ("Cell ID: '1'", AnonymizeCustomPatterns("Cell ID: 'A1B2'"));
  EXPECT_EQ("Cell ID: '2'", AnonymizeCustomPatterns("Cell ID: 'C1D2'"));
  EXPECT_EQ("foo Cell ID: '1' bar",
            AnonymizeCustomPatterns("foo Cell ID: 'A1B2' bar"));

  EXPECT_EQ("foo Location area code: '1' bar",
            AnonymizeCustomPatterns("foo Location area code: 'A1B2' bar"));

  EXPECT_EQ("foo\na SSID='1' b\n'",
            AnonymizeCustomPatterns("foo\na SSID='Joe's' b\n'"));
  EXPECT_EQ("ssid '2'", AnonymizeCustomPatterns("ssid 'My AP'"));
  EXPECT_EQ("bssid 'aa:bb'", AnonymizeCustomPatterns("bssid 'aa:bb'"));

  EXPECT_EQ("Scan SSID - hexdump(len=6): 1\nfoo",
            AnonymizeCustomPatterns(
                "Scan SSID - hexdump(len=6): 47 6f 6f 67 6c 65\nfoo"));

  EXPECT_EQ(
      "a\nb [SSID=1] [SSID=2] [SSID=foo\nbar] b",
      AnonymizeCustomPatterns("a\nb [SSID=foo] [SSID=bar] [SSID=foo\nbar] b"));

  EXPECT_EQ("<email: 1>",
            AnonymizeCustomPatterns("foo@bar.com"));
  EXPECT_EQ("Email: <email: 1>.",
            AnonymizeCustomPatterns("Email: foo@bar.com."));
  EXPECT_EQ("Email:\n<email: 2>\n",
            AnonymizeCustomPatterns("Email:\nfooooo@bar.com\n"));

  EXPECT_EQ("[<IPv6: 1>]", AnonymizeCustomPatterns(
                               "[2001:0db8:0000:0000:0000:ff00:0042:8329]"));
  EXPECT_EQ("[<IPv6: 2>]",
            AnonymizeCustomPatterns("[2001:db8:0:0:0:ff00:42:8329]"));
  EXPECT_EQ("[<IPv6: 3>]", AnonymizeCustomPatterns("[2001:db8::ff00:42:8329]"));
  EXPECT_EQ("[<IPv6: 4>]", AnonymizeCustomPatterns("[::1]"));
  EXPECT_EQ("<IPv4: 1>", AnonymizeCustomPatterns("192.168.0.1"));

  EXPECT_EQ("<URL: 1>",
            AnonymizeCustomPatterns("http://example.com/foo?test=1"));
  EXPECT_EQ("Foo <URL: 2> Bar",
            AnonymizeCustomPatterns("Foo http://192.168.0.1/foo?test=1#123 Bar"));
  const char* kURLs[] = {
    "http://example.com/foo?test=1",
    "http://userid:password@example.com:8080",
    "http://userid:password@example.com:8080/",
    "http://@example.com",
    "http://192.168.0.1",
    "http://192.168.0.1/",
    "http://اختبار.com",
    "http://test.com/foo(bar)baz.html",
    "http://test.com/foo%20bar",
    "ftp://test:tester@test.com",
    "chrome://extensions/",
    "chrome-extension://aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/options.html",
    "http://example.com/foo?email=foo@bar.com",
    "rtsp://root@example.com/",
    "https://aaaaaaaaaaaaaaaa.com",
  };
  for (size_t i = 0; i < arraysize(kURLs); ++i) {
    SCOPED_TRACE(kURLs[i]);
    std::string got = AnonymizeCustomPatterns(kURLs[i]);
    EXPECT_TRUE(
        base::StartsWith(got, "<URL: ", base::CompareCase::INSENSITIVE_ASCII));
    EXPECT_TRUE(base::EndsWith(got, ">", base::CompareCase::INSENSITIVE_ASCII));
  }
  // Test that "Android:" is not considered a schema with empty hier part.
  EXPECT_EQ("The following applies to Android:",
            AnonymizeCustomPatterns("The following applies to Android:"));
}

TEST_F(AnonymizerToolTest, AnonymizeCustomPatternWithContext) {
  const char kPattern[] = "(\\b(?i)id:? ')(\\d+)(')";
  std::map<std::string, std::string> space;
  EXPECT_EQ("", AnonymizeCustomPatternWithContext("", kPattern, &space));
  EXPECT_EQ("foo\nbar\n",
            AnonymizeCustomPatternWithContext("foo\nbar\n", kPattern, &space));
  EXPECT_EQ("id '1'",
            AnonymizeCustomPatternWithContext("id '2345'", kPattern, &space));
  EXPECT_EQ("id '2'",
            AnonymizeCustomPatternWithContext("id '1234'", kPattern, &space));
  EXPECT_EQ("id: '2'",
            AnonymizeCustomPatternWithContext("id: '1234'", kPattern, &space));
  EXPECT_EQ("ID: '1'",
            AnonymizeCustomPatternWithContext("ID: '2345'", kPattern, &space));
  EXPECT_EQ("x1 id '1' 1x id '2'\nid '1'\n",
            AnonymizeCustomPatternWithContext(
                "x1 id '2345' 1x id '1234'\nid '2345'\n", kPattern, &space));
  space.clear();
  EXPECT_EQ("id '1'",
            AnonymizeCustomPatternWithContext("id '1234'", kPattern, &space));

  space.clear();
  EXPECT_EQ("x1z",
            AnonymizeCustomPatternWithContext("xyz", "()(y+)()", &space));
}

TEST_F(AnonymizerToolTest, AnonymizeCustomPatternWithoutContext) {
  CustomPatternWithoutContext kPattern = {"pattern", "(o+)"};
  std::map<std::string, std::string> space;
  EXPECT_EQ("", AnonymizeCustomPatternWithoutContext("", kPattern, &space));
  EXPECT_EQ("f<pattern: 1>\nf<pattern: 2>z\nf<pattern: 1>l\n",
            AnonymizeCustomPatternWithoutContext("fo\nfooz\nfol\n", kPattern,
                                                 &space));
}

}  // namespace feedback
