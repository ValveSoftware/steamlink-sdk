// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parser_os2.h"

#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "net/ftp/ftp_directory_listing_parser.h"
#include "net/ftp/ftp_util.h"

namespace net {

bool ParseFtpDirectoryListingOS2(
    const std::vector<base::string16>& lines,
    std::vector<FtpDirectoryListingEntry>* entries) {
  for (size_t i = 0; i < lines.size(); i++) {
    if (lines[i].empty())
      continue;

    std::vector<base::string16> columns;
    base::SplitString(base::CollapseWhitespace(lines[i], false), ' ', &columns);

    // Every line of the listing consists of the following:
    //
    //   1. size in bytes (0 for directories)
    //   2. type (A for files, DIR for directories)
    //   3. date
    //   4. time
    //   5. filename (may be empty or contain spaces)
    //
    // For now, make sure we have 1-4, and handle 5 later.
    if (columns.size() < 4)
      return false;

    FtpDirectoryListingEntry entry;
    if (!base::StringToInt64(columns[0], &entry.size))
      return false;
    if (EqualsASCII(columns[1], "DIR")) {
      if (entry.size != 0)
        return false;
      entry.type = FtpDirectoryListingEntry::DIRECTORY;
      entry.size = -1;
    } else if (EqualsASCII(columns[1], "A")) {
      entry.type = FtpDirectoryListingEntry::FILE;
      if (entry.size < 0)
        return false;
    } else {
      return false;
    }

    if (!FtpUtil::WindowsDateListingToTime(columns[2],
                                           columns[3],
                                           &entry.last_modified)) {
      return false;
    }

    entry.name = FtpUtil::GetStringPartAfterColumns(lines[i], 4);
    if (entry.name.empty()) {
      // Some FTP servers send listing entries with empty names.
      // It's not obvious how to display such an entry, so ignore them.
      // We don't want to make the parsing fail at this point though.
      // Other entries can still be useful.
      continue;
    }

    entries->push_back(entry);
  }

  return true;
}

}  // namespace net
