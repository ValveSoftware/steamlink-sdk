// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/log_private/log_parser.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_split.h"
#include "chrome/browser/extensions/api/log_private/log_private_api.h"
#include "chrome/common/extensions/api/log_private.h"

using std::string;
using std::vector;

namespace extensions {

LogParser::LogParser() {
}

LogParser::~LogParser() {
}

void LogParser::Parse(const string& input,
                      std::vector<api::log_private::LogEntry>* output,
                      FilterHandler* filter_handler) const {
  // Assume there is no newline in the log entry
  std::vector<string> entries = base::SplitString(
      input, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  for (size_t i = 0; i < entries.size(); i++) {
    ParseEntry(entries[i], output, filter_handler);
  }
}

}  // namespace extensions
