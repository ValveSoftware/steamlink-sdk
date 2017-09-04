// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/test_resource_handler.h"

#include "base/logging.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TestResourceHandler::TestResourceHandler(net::URLRequestStatus* request_status,
                                         std::string* body)
    : ResourceHandler(nullptr), request_status_(request_status), body_(body) {
  SetBufferSize(2048);
}

TestResourceHandler::TestResourceHandler()
    : TestResourceHandler(nullptr, nullptr) {}

TestResourceHandler::~TestResourceHandler() {}

void TestResourceHandler::SetController(ResourceController* controller) {}

bool TestResourceHandler::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    ResourceResponse* response,
    bool* defer) {
  NOTREACHED() << "Redirects are not supported by the TestResourceHandler.";
  return false;
}

bool TestResourceHandler::OnResponseStarted(ResourceResponse* response,
                                            bool* defer) {
  EXPECT_EQ(1, on_will_start_called_);
  EXPECT_EQ(0, on_response_started_called_);
  EXPECT_EQ(0, on_response_completed_called_);
  ++on_response_started_called_;

  if (!on_response_started_result_)
    return false;
  *defer = defer_on_response_started_;
  defer_on_response_started_ = false;
  return true;
}

bool TestResourceHandler::OnWillStart(const GURL& url, bool* defer) {
  EXPECT_EQ(0, on_response_started_called_);
  EXPECT_EQ(0, on_will_start_called_);
  EXPECT_EQ(0, on_response_completed_called_);
  ++on_will_start_called_;

  if (!on_will_start_result_)
    return false;

  *defer = defer_on_will_start_;
  return true;
}

bool TestResourceHandler::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                                     int* buf_size,
                                     int min_size) {
  EXPECT_EQ(0, on_response_completed_called_);
  ++on_will_read_called_;

  *buf = buffer_;
  *buf_size = buffer_size_;
  memset(buffer_->data(), '\0', buffer_size_);
  return on_will_read_result_;
}

bool TestResourceHandler::OnReadCompleted(int bytes_read, bool* defer) {
  EXPECT_EQ(1, on_will_start_called_);
  EXPECT_EQ(1, on_response_started_called_);
  EXPECT_EQ(0, on_response_completed_called_);
  ++on_read_completed_called_;

  EXPECT_LE(static_cast<size_t>(bytes_read), buffer_size_);
  if (body_)
    body_->append(buffer_->data(), bytes_read);
  if (!on_read_completed_result_)
    return false;
  *defer = defer_on_read_completed_;
  defer_on_read_completed_ = false;
  return true;
}

void TestResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    bool* defer) {
  EXPECT_EQ(0, on_response_completed_called_);
  ++on_response_completed_called_;

  if (request_status_)
    *request_status_ = status;
  *defer = defer_on_response_completed_;
  defer_on_response_completed_ = false;
}

void TestResourceHandler::OnDataDownloaded(int bytes_downloaded) {
  NOTREACHED() << "Saving to file is not supported by the TestResourceHandler.";
}

void TestResourceHandler::SetBufferSize(int buffer_size) {
  buffer_ = new net::IOBuffer(buffer_size);
  buffer_size_ = buffer_size;
  memset(buffer_->data(), '\0', buffer_size);
}

}  // namespace content
