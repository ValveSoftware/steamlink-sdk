// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_LOG_PRIVATE_FILTER_HANDLER_H_
#define CHROME_BROWSER_EXTENSIONS_API_LOG_PRIVATE_FILTER_HANDLER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/common/extensions/api/log_private.h"

namespace extensions {
// This class contains multiple filtering methods to filter log entries
// by multiple fields.
class FilterHandler {
 public:
  explicit FilterHandler(const api::log_private::Filter& filter);
  ~FilterHandler();

  // This function decides if a log entry should be returned to user.
  // Returns true if the log entry meets the filtering conditions.
  bool IsValidLogEntry(const api::log_private::LogEntry& entry) const;
  // Filters log by timestamp.
  // Returns true if the timestamp is within the time range of the filter.
  bool IsValidTime(double time) const;
  // Filters log by source (syslog, network_event_log, etc).
  // Returns true if the log is from specified source in the filter.
  bool IsValidSource(const std::string& source) const;
  // Filters log by level (DEBUG, ERROR, WARNING).
  // Returns true if the log level is specified in the filter.
  bool IsValidLevel(const std::string& level) const;
  // Filters log by its process name.
  // Returns true if the process name is specified in the filter.
  bool IsValidProcess(const std::string& process) const;

  const api::log_private::Filter* GetFilter() const { return &filter_; }

 private:
  api::log_private::Filter filter_;

  DISALLOW_COPY_AND_ASSIGN(FilterHandler);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_LOG_PRIVATE_FILTER_HANDLER_H_
