// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MOCK_MEDIA_LOG_H_
#define MEDIA_BASE_MOCK_MEDIA_LOG_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "media/base/media_log.h"
#include "testing/gmock/include/gmock/gmock.h"

// Helper macros to reduce boilerplate when verifying media log entries.
// |outer| is the std::string searched for substring |sub|.
#define CONTAINS_STRING(outer, sub) (std::string::npos != (outer).find(sub))

// "media_log_" is expected to be a scoped_refptr<MockMediaLog>, optionally a
// StrictMock, in scope of the usage of this macro.
#define EXPECT_MEDIA_LOG(x) EXPECT_CALL(*media_log_, DoAddEventLogString((x)))

namespace media {

class MockMediaLog : public MediaLog {
 public:
  MockMediaLog();

  MOCK_METHOD1(DoAddEventLogString, void(const std::string& event));

  // Trampoline method to workaround GMOCK problems with std::unique_ptr<>.
  // Also simplifies tests to be able to string match on the log string
  // representation on the added event.
  void AddEvent(std::unique_ptr<MediaLogEvent> event) override {
    DoAddEventLogString(MediaEventToLogString(*event));
  }

 protected:
  virtual ~MockMediaLog();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMediaLog);
};

}  // namespace media

#endif  // MEDIA_BASE_MOCK_MEDIA_LOG_H_
