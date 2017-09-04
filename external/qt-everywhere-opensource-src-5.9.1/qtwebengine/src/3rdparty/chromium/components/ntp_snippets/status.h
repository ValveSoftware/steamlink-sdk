// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_STATUS_H_
#define COMPONENTS_NTP_SNIPPETS_STATUS_H_

#include <string>

namespace ntp_snippets {

// This enum indicates how an operation was completed.
enum class StatusCode {
  SUCCESS,          // The operation has been completed successfully.
  TEMPORARY_ERROR,  // The operation failed but retrying might solve the error.
  PERMANENT_ERROR,  // The operation failed and would fail again if retried.
};

// This struct provides the status code of a request and an optional message
// describing the status (esp. failures) in detail.
struct Status {
  Status(StatusCode status, const std::string& message);
  // TODO(tschumann): Change this into a a Success() factory method. Error
  // states should always have a message.
  explicit Status(StatusCode status);
  ~Status();

  StatusCode status;
  // The message is not meant to be displayed to the user.
  std::string message;

  // Copy and assignment allowed.
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_STATUS_H_
