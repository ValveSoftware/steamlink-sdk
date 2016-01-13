// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/scoped_mock_log.h"

#include "base/logging.h"

namespace net {
namespace test {

// static
ScopedMockLog* ScopedMockLog::g_instance_ = NULL;

ScopedMockLog::ScopedMockLog() : is_capturing_logs_(false) {}

ScopedMockLog::~ScopedMockLog() {
  if (is_capturing_logs_) {
    StopCapturingLogs();
  }
}

void ScopedMockLog::StartCapturingLogs() {
  // We don't use CHECK(), which can generate a new LOG message, and
  // thus can confuse ScopedMockLog objects or other registered
  // LogSinks.
  RAW_CHECK(!is_capturing_logs_);
  RAW_CHECK(!g_instance_);

  is_capturing_logs_ = true;
  g_instance_ = this;
  previous_handler_ = logging::GetLogMessageHandler();
  logging::SetLogMessageHandler(LogMessageHandler);
}

void ScopedMockLog::StopCapturingLogs() {
  // We don't use CHECK(), which can generate a new LOG message, and
  // thus can confuse ScopedMockLog objects or other registered
  // LogSinks.
  RAW_CHECK(is_capturing_logs_);
  RAW_CHECK(g_instance_ == this);

  is_capturing_logs_ = false;
  logging::SetLogMessageHandler(previous_handler_);
  g_instance_ = NULL;
}

// static
bool ScopedMockLog::LogMessageHandler(int severity,
                                      const char* file,
                                      int line,
                                      size_t message_start,
                                      const std::string& str) {
  return g_instance_->Log(severity, file, line, message_start, str);
}

}  // namespace test
}  // namespace net
