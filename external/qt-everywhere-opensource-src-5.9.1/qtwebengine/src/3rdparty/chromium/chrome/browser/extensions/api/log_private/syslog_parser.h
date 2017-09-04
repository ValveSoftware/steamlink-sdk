// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_LOG_PRIVATE_SYSLOG_PARSER_H_
#define CHROME_BROWSER_EXTENSIONS_API_LOG_PRIVATE_SYSLOG_PARSER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/extensions/api/log_private/log_parser.h"
#include "chrome/common/extensions/api/log_private.h"

namespace extensions {

// A parser that parses syslog into LogEntry objects.
class SyslogParser : public LogParser {
 public:
  SyslogParser();
  ~SyslogParser() override;

 protected:
  // Parses one line log text into a LogEntry object.
  Error ParseEntry(const std::string& input,
                   std::vector<api::log_private::LogEntry>* output,
                   FilterHandler* filter_handler) const override;

 private:
  // Parses time token and get time in milliseconds.
  Error ParseTime(const std::string& input, double* output) const;
  // Parses process token and get process name and ID.
  Error ParseProcess(const std::string& input,
                     api::log_private::LogEntry* entry) const;
  // Parses level token and get log level.
  void ParseLevel(const std::string& input,
                  api::log_private::LogEntry* entry) const;

  DISALLOW_COPY_AND_ASSIGN(SyslogParser);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_LOG_PRIVATE_SYSLOG_PARSER_H_
