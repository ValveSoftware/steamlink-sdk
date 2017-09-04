// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/offline_pages/offline_event_logger.h"

namespace offline_pages {

OfflineEventLogger::OfflineEventLogger()
    : activities_(kMaxLogCount), is_logging_(false) {}

OfflineEventLogger::~OfflineEventLogger() {}

void OfflineEventLogger::SetIsLogging(bool is_logging) {
  is_logging_ = is_logging;
}

bool OfflineEventLogger::GetIsLogging() {
  return is_logging_;
}

void OfflineEventLogger::Clear() {
  activities_.clear();
}

void OfflineEventLogger::RecordActivity(const std::string& activity) {
  if (!is_logging_) {
    return;
  }
  if (activities_.size() == kMaxLogCount)
    activities_.pop_back();

  base::Time::Exploded current_time;
  base::Time::Now().LocalExplode(&current_time);

  std::string date_string = base::StringPrintf(
      "%d %02d %02d %02d:%02d:%02d",
      current_time.year,
      current_time.month,
      current_time.day_of_month,
      current_time.hour,
      current_time.minute,
      current_time.second);

  activities_.push_front(date_string + ": " + activity);
}

void OfflineEventLogger::GetLogs(std::vector<std::string>* records) {
  DCHECK(records);
  for (std::deque<std::string>::iterator it = activities_.begin();
       it != activities_.end(); it++) {
    if (!it->empty())
      records->push_back(*it);
  }
}

}  // namespace offline_pages
