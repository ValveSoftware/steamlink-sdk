// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parser_unittest.h"

#include "base/format_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "net/ftp/ftp_directory_listing_parser_netware.h"

namespace net {

namespace {

typedef FtpDirectoryListingParserTest FtpDirectoryListingParserNetwareTest;

TEST_F(FtpDirectoryListingParserNetwareTest, Good) {
  const struct SingleLineTestData good_cases[] = {
    { "d [RWCEAFMS] ftpadmin 512 Jan 29  2004 pub",
      FtpDirectoryListingEntry::DIRECTORY, "pub", -1,
      2004, 1, 29, 0, 0 },
    { "- [RW------] ftpadmin 123 Nov 11  18:25 afile",
      FtpDirectoryListingEntry::FILE, "afile", 123,
      1994, 11, 11, 18, 25 },
    { "d [RWCEAFMS] daniel 512 May 17  2010 NVP anyagok",
      FtpDirectoryListingEntry::DIRECTORY, "NVP anyagok", -1,
      2010, 5, 17, 0, 0 },
  };
  for (size_t i = 0; i < arraysize(good_cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i,
                                    good_cases[i].input));

    std::vector<base::string16> lines(
        GetSingleLineTestCase(good_cases[i].input));

    // The parser requires a "total n" line before accepting regular input.
    lines.insert(lines.begin(), base::ASCIIToUTF16("total 1"));

    std::vector<FtpDirectoryListingEntry> entries;
    EXPECT_TRUE(ParseFtpDirectoryListingNetware(lines,
                                                GetMockCurrentTime(),
                                                &entries));
    VerifySingleLineTestCase(good_cases[i], entries);
  }
}

TEST_F(FtpDirectoryListingParserNetwareTest, Bad) {
  const char* bad_cases[] = {
    " foo",
    "garbage",
    "d [] ftpadmin 512 Jan 29  2004 pub",
    "d [XGARBAGE] ftpadmin 512 Jan 29  2004 pub",
    "d [RWCEAFMS] 512 Jan 29  2004 pub",
    "d [RWCEAFMS] ftpadmin -1 Jan 29  2004 pub",
    "l [RW------] ftpadmin 512 Jan 29  2004 pub",
  };
  for (size_t i = 0; i < arraysize(bad_cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i,
                                    bad_cases[i]));

    std::vector<base::string16> lines(GetSingleLineTestCase(bad_cases[i]));

    // The parser requires a "total n" line before accepting regular input.
    lines.insert(lines.begin(), base::ASCIIToUTF16("total 1"));

    std::vector<FtpDirectoryListingEntry> entries;
    EXPECT_FALSE(ParseFtpDirectoryListingNetware(lines,
                                                 GetMockCurrentTime(),
                                                 &entries));
  }
}

}  // namespace

}  // namespace net
