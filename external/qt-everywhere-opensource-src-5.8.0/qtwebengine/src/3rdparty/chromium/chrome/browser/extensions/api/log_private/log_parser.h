// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_LOG_PRIVATE_LOG_PARSER_H_
#define CHROME_BROWSER_EXTENSIONS_API_LOG_PRIVATE_LOG_PARSER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/extensions/api/log_private/filter_handler.h"
#include "chrome/common/extensions/api/log_private.h"

namespace extensions {

// This is a abstract class of parsers for different logs.
// All the specific parsers inherit from this class.
class LogParser {
 public:
  enum Error {
    SUCCESS = 0,
    PARSE_ERROR = -1,
    SERIALIZE_ERROR = -2,
    TOKENIZE_ERROR = -3
  };

  virtual ~LogParser();
  // Parses log text into multiple LogEntry objects.
  void Parse(const std::string& input,
             std::vector<api::log_private::LogEntry>* output,
             FilterHandler* filter_handler) const;

 protected:
  LogParser();
  // Parses a single line of log text into one LogEntry object.
  virtual Error ParseEntry(const std::string& input,
                           std::vector<api::log_private::LogEntry>* output,
                           FilterHandler* filter_handler) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(LogParser);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_LOG_PRIVATE_LOG_PARSER_H_
