// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parser_unittest.h"

#include "base/format_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/ftp/ftp_directory_listing_parser_os2.h"

namespace net {

namespace {

typedef FtpDirectoryListingParserTest FtpDirectoryListingParserOS2Test;

TEST_F(FtpDirectoryListingParserOS2Test, Good) {
  const struct SingleLineTestData good_cases[] = {
    { "0 DIR 11-02-09  17:32       NT",
      FtpDirectoryListingEntry::DIRECTORY, "NT", -1,
      2009, 11, 2, 17, 32 },
    { "458 A 01-06-09  14:42       Readme.txt",
      FtpDirectoryListingEntry::FILE, "Readme.txt", 458,
      2009, 1, 6, 14, 42 },
    { "1 A 01-06-09  02:42         Readme.txt",
      FtpDirectoryListingEntry::FILE, "Readme.txt", 1,
      2009, 1, 6, 2, 42 },
    { "458 A 01-06-01  02:42       Readme.txt",
      FtpDirectoryListingEntry::FILE, "Readme.txt", 458,
      2001, 1, 6, 2, 42 },
    { "458 A 01-06-00  02:42       Corner1.txt",
      FtpDirectoryListingEntry::FILE, "Corner1.txt", 458,
      2000, 1, 6, 2, 42 },
    { "458 A 01-06-99  02:42       Corner2.txt",
      FtpDirectoryListingEntry::FILE, "Corner2.txt", 458,
      1999, 1, 6, 2, 42 },
    { "458 A 01-06-80  02:42       Corner3.txt",
      FtpDirectoryListingEntry::FILE, "Corner3.txt", 458,
      1980, 1, 6, 2, 42 },
    { "458 A 01-06-1979  02:42     Readme.txt",
      FtpDirectoryListingEntry::FILE, "Readme.txt", 458,
      1979, 1, 6, 2, 42 },
    { "0 DIR 11-02-09  17:32       My Directory",
      FtpDirectoryListingEntry::DIRECTORY, "My Directory", -1,
      2009, 11, 2, 17, 32 },
    { "0 DIR 12-25-10  00:00       Christmas Midnight",
      FtpDirectoryListingEntry::DIRECTORY, "Christmas Midnight", -1,
      2010, 12, 25, 0, 0 },
    { "0 DIR 12-25-10  12:00       Christmas Midday",
      FtpDirectoryListingEntry::DIRECTORY, "Christmas Midday", -1,
      2010, 12, 25, 12, 0 },
  };
  for (size_t i = 0; i < arraysize(good_cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i,
                                    good_cases[i].input));

    std::vector<FtpDirectoryListingEntry> entries;
    EXPECT_TRUE(ParseFtpDirectoryListingOS2(
        GetSingleLineTestCase(good_cases[i].input),
        &entries));
    VerifySingleLineTestCase(good_cases[i], entries);
  }
}

TEST_F(FtpDirectoryListingParserOS2Test, Ignored) {
  const char* ignored_cases[] = {
    "1234 A 12-07-10  12:05",
    "0 DIR 11-02-09  05:32",
  };
  for (size_t i = 0; i < arraysize(ignored_cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i,
                                    ignored_cases[i]));

    std::vector<FtpDirectoryListingEntry> entries;
    EXPECT_TRUE(ParseFtpDirectoryListingOS2(
                    GetSingleLineTestCase(ignored_cases[i]),
                    &entries));
    EXPECT_EQ(0U, entries.size());
  }
}

TEST_F(FtpDirectoryListingParserOS2Test, Bad) {
  const char* bad_cases[] = {
    "garbage",
    "0 GARBAGE 11-02-09  05:32",
    "0 GARBAGE 11-02-09  05:32       NT",
    "0 DIR 11-FEB-09 05:32",
    "0 DIR 11-02     05:32",
    "-1 A 11-02-09  05:32",
    "0 DIR 11-FEB-09 05:32",
    "0 DIR 11-02     05:32       NT",
    "-1 A 11-02-09  05:32        NT",
    "0 A 99-25-10  12:00",
    "0 A 12-99-10  12:00",
    "0 A 12-25-10  99:00",
    "0 A 12-25-10  12:99",
    "0 A 99-25-10  12:00 months out of range",
    "0 A 12-99-10  12:00 days out of range",
    "0 A 12-25-10  99:00 hours out of range",
    "0 A 12-25-10  12:99 minutes out of range",
  };
  for (size_t i = 0; i < arraysize(bad_cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i,
                                    bad_cases[i]));

    std::vector<FtpDirectoryListingEntry> entries;
    EXPECT_FALSE(ParseFtpDirectoryListingOS2(
                     GetSingleLineTestCase(bad_cases[i]),
                     &entries));
  }
}

}  // namespace

}  // namespace net
