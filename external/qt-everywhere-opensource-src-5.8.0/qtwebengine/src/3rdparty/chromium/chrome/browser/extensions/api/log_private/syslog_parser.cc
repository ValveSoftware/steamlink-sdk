// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/log_private/syslog_parser.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/log_private/filter_handler.h"
#include "chrome/browser/extensions/api/log_private/log_parser.h"
#include "chrome/browser/extensions/api/log_private/log_private_api.h"
#include "chrome/common/extensions/api/log_private.h"

namespace extensions {

namespace {

const char kProcessInfoDelimiters[] = "[]:";

}  // namespace

SyslogParser::SyslogParser() {}

SyslogParser::~SyslogParser() {}

SyslogParser::Error SyslogParser::ParseEntry(
    const std::string& input,
    std::vector<api::log_private::LogEntry>* output,
    FilterHandler* filter_handler) const {
  base::StringTokenizer tokenizer(input, " ");
  if (!tokenizer.GetNext()) {
    LOG(ERROR)
        << "Error when parsing data. Expect: At least 2 tokens. Actual: 0";
    return TOKENIZE_ERROR;
  }
  std::string time = tokenizer.token();
  api::log_private::LogEntry entry;
  if (ParseTime(time, &(entry.timestamp)) != SyslogParser::SUCCESS) {
    return SyslogParser::PARSE_ERROR;
  }
  if (!tokenizer.GetNext()) {
    LOG(ERROR)
        << "Error when parsing data. Expect: At least 2 tokens. Actual: 1";
    return TOKENIZE_ERROR;
  }
  ParseProcess(tokenizer.token(), &entry);
  ParseLevel(input, &entry);
  entry.full_entry = input;

  if (filter_handler->IsValidLogEntry(entry)) {
    output->push_back(std::move(entry));
  }

  return SyslogParser::SUCCESS;
}

SyslogParser::Error SyslogParser::ParseTime(const std::string& input,
                                            double* output) const {
  base::Time parsed_time;
  if (!base::Time::FromString(input.c_str(), &parsed_time)) {
    LOG(ERROR) << "Error when parsing time";
    return SyslogParser::PARSE_ERROR;
  }

  *output = parsed_time.ToJsTime();
  return SyslogParser::SUCCESS;
}

SyslogParser::Error SyslogParser::ParseProcess(
    const std::string& input,
    api::log_private::LogEntry* entry) const {
  base::StringTokenizer tokenizer(input, kProcessInfoDelimiters);
  if (!tokenizer.GetNext()) {
    LOG(ERROR)
        << "Error when parsing data. Expect: At least 1 token. Actual: 0";
    return SyslogParser::PARSE_ERROR;
  }
  entry->process = tokenizer.token();
  entry->process_id = "unknown";
  if (tokenizer.GetNext()) {
    std::string token = tokenizer.token();
    int tmp;
    if (base::StringToInt(token, &tmp)) {
      entry->process_id = token;
    }
  }
  return SyslogParser::SUCCESS;
}

void SyslogParser::ParseLevel(const std::string& input,
                              api::log_private::LogEntry* entry) const {
  if (input.find("ERROR") != std::string::npos) {
    entry->level = "error";
  } else if (input.find("WARN") != std::string::npos) {
    entry->level = "warning";
  } else if (input.find("INFO") != std::string::npos) {
    entry->level = "info";
  } else {
    entry->level = "unknown";
  }
}

}  // namespace extensions
