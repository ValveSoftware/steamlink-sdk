// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/log_private/filter_handler.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "chrome/common/extensions/api/log_private.h"

namespace extensions {

namespace {

template <typename T>
bool IsValidField(const std::vector<T>& filter, const T& field) {
  return (filter.empty() ||
          std::find(filter.begin(), filter.end(), field) != filter.end());
}

}  // namespace

FilterHandler::FilterHandler(const api::log_private::Filter& filter) {
  std::unique_ptr<base::DictionaryValue> filter_value = filter.ToValue();
  api::log_private::Filter::Populate(*filter_value, &filter_);
}

FilterHandler::~FilterHandler() {}

bool FilterHandler::IsValidLogEntry(
    const api::log_private::LogEntry& entry) const {
  return (IsValidProcess(entry.process) && IsValidLevel(entry.level) &&
          IsValidTime(entry.timestamp));
}

bool FilterHandler::IsValidTime(double time) const {
  const double kInvalidTime = 0;
  if (filter_.start_timestamp != kInvalidTime &&
      (filter_.start_timestamp > time || filter_.end_timestamp < time)) {
    return false;
  }
  return true;
}

bool FilterHandler::IsValidSource(const std::string& source) const {
  return IsValidField(filter_.sources, source);
}

bool FilterHandler::IsValidLevel(const std::string& level) const {
  return IsValidField(filter_.level, level);
}

bool FilterHandler::IsValidProcess(const std::string& process) const {
  return IsValidField(filter_.process, process);
}

}  // namespace extensions
