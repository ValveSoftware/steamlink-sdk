// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parser.h"

#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/net_errors.h"
#include "net/ftp/ftp_directory_listing_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class FtpDirectoryListingParserTest
    : public testing::TestWithParam<const char*> {
};

TEST_P(FtpDirectoryListingParserTest, Parse) {
  base::FilePath test_dir;
  PathService::Get(base::DIR_SOURCE_ROOT, &test_dir);
  test_dir = test_dir.AppendASCII("net");
  test_dir = test_dir.AppendASCII("data");
  test_dir = test_dir.AppendASCII("ftp");

  base::Time::Exploded mock_current_time_exploded = { 0 };
  mock_current_time_exploded.year = 1994;
  mock_current_time_exploded.month = 11;
  mock_current_time_exploded.day_of_month = 15;
  mock_current_time_exploded.hour = 12;
  mock_current_time_exploded.minute = 45;
  base::Time mock_current_time(
      base::Time::FromLocalExploded(mock_current_time_exploded));

  SCOPED_TRACE(base::StringPrintf("Test case: %s", GetParam()));

  std::string test_listing;
  EXPECT_TRUE(base::ReadFileToString(test_dir.AppendASCII(GetParam()),
                                     &test_listing));

  std::vector<FtpDirectoryListingEntry> entries;
  EXPECT_EQ(OK, ParseFtpDirectoryListing(test_listing,
                                         mock_current_time,
                                         &entries));

  std::string expected_listing;
  ASSERT_TRUE(base::ReadFileToString(
                  test_dir.AppendASCII(std::string(GetParam()) + ".expected"),
                  &expected_listing));

  std::vector<std::string> lines;
  base::SplitStringUsingSubstr(expected_listing, "\r\n", &lines);

  // Special case for empty listings.
  if (lines.size() == 1 && lines[0].empty())
    lines.clear();

  ASSERT_EQ(9 * entries.size(), lines.size());

  for (size_t i = 0; i < lines.size() / 9; i++) {
    std::string type(lines[9 * i]);
    std::string name(lines[9 * i + 1]);
    int64 size;
    base::StringToInt64(lines[9 * i + 2], &size);

    SCOPED_TRACE(base::StringPrintf("Filename: %s", name.c_str()));

    int year, month, day_of_month, hour, minute;
    base::StringToInt(lines[9 * i + 3], &year);
    base::StringToInt(lines[9 * i + 4], &month);
    base::StringToInt(lines[9 * i + 5], &day_of_month);
    base::StringToInt(lines[9 * i + 6], &hour);
    base::StringToInt(lines[9 * i + 7], &minute);

    const FtpDirectoryListingEntry& entry = entries[i];

    if (type == "d") {
      EXPECT_EQ(FtpDirectoryListingEntry::DIRECTORY, entry.type);
    } else if (type == "-") {
      EXPECT_EQ(FtpDirectoryListingEntry::FILE, entry.type);
    } else if (type == "l") {
      EXPECT_EQ(FtpDirectoryListingEntry::SYMLINK, entry.type);
    } else {
      ADD_FAILURE() << "invalid gold test data: " << type;
    }

    EXPECT_EQ(base::UTF8ToUTF16(name), entry.name);
    EXPECT_EQ(size, entry.size);

    base::Time::Exploded time_exploded;
    entry.last_modified.LocalExplode(&time_exploded);
    EXPECT_EQ(year, time_exploded.year);
    EXPECT_EQ(month, time_exploded.month);
    EXPECT_EQ(day_of_month, time_exploded.day_of_month);
    EXPECT_EQ(hour, time_exploded.hour);
    EXPECT_EQ(minute, time_exploded.minute);
  }
}

const char* kTestFiles[] = {
  "dir-listing-ls-1",
  "dir-listing-ls-1-utf8",
  "dir-listing-ls-2",
  "dir-listing-ls-3",
  "dir-listing-ls-4",
  "dir-listing-ls-5",
  "dir-listing-ls-6",
  "dir-listing-ls-7",
  "dir-listing-ls-8",
  "dir-listing-ls-9",
  "dir-listing-ls-10",
  "dir-listing-ls-11",
  "dir-listing-ls-12",
  "dir-listing-ls-13",
  "dir-listing-ls-14",
  "dir-listing-ls-15",
  "dir-listing-ls-16",
  "dir-listing-ls-17",
  "dir-listing-ls-18",
  "dir-listing-ls-19",
  "dir-listing-ls-20",  // TODO(phajdan.jr): should use windows-1251 encoding.
  "dir-listing-ls-21",  // TODO(phajdan.jr): should use windows-1251 encoding.
  "dir-listing-ls-22",  // TODO(phajdan.jr): should use windows-1251 encoding.
  "dir-listing-ls-23",
  "dir-listing-ls-24",

  // Tests for Russian listings. The only difference between those
  // files is character encoding:
  "dir-listing-ls-25",  // UTF-8
  "dir-listing-ls-26",  // KOI8-R
  "dir-listing-ls-27",  // windows-1251

  "dir-listing-ls-28",  // Hylafax FTP server
  "dir-listing-ls-29",
  "dir-listing-ls-30",
  "dir-listing-ls-31",
  "dir-listing-ls-32",  // busybox

  "dir-listing-netware-1",
  "dir-listing-netware-2",
  "dir-listing-netware-3",  // Spaces in file names.
  "dir-listing-os2-1",
  "dir-listing-vms-1",
  "dir-listing-vms-2",
  "dir-listing-vms-3",
  "dir-listing-vms-4",
  "dir-listing-vms-5",
  "dir-listing-vms-6",
  "dir-listing-vms-7",
  "dir-listing-vms-8",
  "dir-listing-windows-1",
  "dir-listing-windows-2",
};

INSTANTIATE_TEST_CASE_P(, FtpDirectoryListingParserTest,
                        testing::ValuesIn(kTestFiles));

}  // namespace

}  // namespace net
