// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file's dependencies should be kept to a minimum so that it can be
// included in WebKit code that doesn't rely on much of common.

#ifndef NET_URL_REQUEST_URL_REQUEST_STATUS_H_
#define NET_URL_REQUEST_URL_REQUEST_STATUS_H_

namespace net {

// Represents the result of a URL request. It encodes errors and various
// types of success.
class URLRequestStatus {
 public:
  enum Status {
    // Request succeeded, |error_| will be 0.
    SUCCESS = 0,

    // An IO request is pending, and the caller will be informed when it is
    // completed.
    IO_PENDING,

    // Request was cancelled programatically.
    CANCELED,

    // The request failed for some reason. |error_| may have more information.
    FAILED,
  };

  URLRequestStatus() : status_(SUCCESS), error_(0) {}
  URLRequestStatus(Status s, int e) : status_(s), error_(e) {}

  Status status() const { return status_; }
  void set_status(Status s) { status_ = s; }

  int error() const { return error_; }
  void set_error(int e) { error_ = e; }

  // Returns true if the status is success, which makes some calling code more
  // convenient because this is the most common test.
  bool is_success() const {
    return status_ == SUCCESS || status_ == IO_PENDING;
  }

  // Returns true if the request is waiting for IO.
  bool is_io_pending() const {
    return status_ == IO_PENDING;
  }

 private:
  // Application level status.
  Status status_;

  // Error code from the network layer if an error was encountered.
  int error_;
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_STATUS_H_
