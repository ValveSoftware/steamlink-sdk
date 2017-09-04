// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_TEST_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_TEST_RESOURCE_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/loader/resource_handler.h"
#include "net/base/io_buffer.h"

class GURL;

namespace net {
class URLRequest;
class URLRequestStatus;
}

namespace content {

class ResourceHandler;
struct ResourceResponse;

// A test version of a ResourceHandler. It returns a configurable buffer in
// response to OnWillStart. It records what ResourceHandler methods are called,
// and verifies that they are called in the correct order. It can optionally
// defer or fail the request at any stage, and record the response body and
// final status it sees. Redirects currently not supported.
class TestResourceHandler : public ResourceHandler {
 public:
  // If non-null, |request_status| will be updated when the response is complete
  // with the final status of the request received by the handler and |body|
  // will be updated on each OnReadCompleted call.
  TestResourceHandler(net::URLRequestStatus* request_status, std::string* body);
  TestResourceHandler();
  ~TestResourceHandler() override;

  // ResourceHandler implementation:
  void SetController(ResourceController* controller) override;
  bool OnRequestRedirected(const net::RedirectInfo& redirect_info,
                           ResourceResponse* response,
                           bool* defer) override;
  bool OnResponseStarted(ResourceResponse* response, bool* defer) override;
  bool OnWillStart(const GURL& url, bool* defer) override;
  bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size,
                  int min_size) override;
  bool OnReadCompleted(int bytes_read, bool* defer) override;
  void OnResponseCompleted(const net::URLRequestStatus& status,
                           bool* defer) override;
  void OnDataDownloaded(int bytes_downloaded) override;

  // Sets the size of the read buffer returned by OnWillRead. Releases reference
  // to previous read buffer. Default size is 2048 bytes.
  void SetBufferSize(int buffer_size);

  scoped_refptr<net::IOBuffer> buffer() const { return buffer_; }

  // Sets the result returned by each method. All default to returning true.
  void set_on_will_start_result(bool on_will_start_result) {
    on_will_start_result_ = on_will_start_result;
  }
  void set_on_response_started_result(bool on_response_started_result) {
    on_response_started_result_ = on_response_started_result;
  }
  void set_on_will_read_result(bool on_will_read_result) {
    on_will_read_result_ = on_will_read_result;
  }
  void set_on_read_completed_result(bool on_read_completed_result) {
    on_read_completed_result_ = on_read_completed_result;
  }

  // Cause |defer| to be set to true when the specified method is invoked. The
  // test itself is responsible for resuming the request after deferral.

  void set_defer_on_will_start(bool defer_on_will_start) {
    defer_on_will_start_ = defer_on_will_start;
  }
  void set_defer_on_response_started(bool defer_on_response_started) {
    defer_on_response_started_ = defer_on_response_started;
  }
  // Only the next OnReadCompleted call will set |defer| to true.
  void set_defer_on_read_completed(bool defer_on_read_completed) {
    defer_on_read_completed_ = defer_on_read_completed;
  }
  void set_defer_on_response_completed(bool defer_on_response_completed) {
    defer_on_response_completed_ = defer_on_response_completed;
  }

  // Return the number of times the corresponding method was invoked.

  int on_will_start_called() const { return on_will_start_called_; }
  // Redirection currently not supported.
  int on_request_redirected_called() const { return 0; }
  int on_response_started_called() const { return on_response_started_called_; }
  int on_will_read_called() const { return on_will_read_called_; }
  int on_read_completed_called() const { return on_read_completed_called_; }
  int on_response_completed_called() const {
    return on_response_completed_called_;
  }

 private:
  net::URLRequestStatus* request_status_;
  std::string* body_;
  scoped_refptr<net::IOBuffer> buffer_;
  size_t buffer_size_;

  bool on_will_start_result_ = true;
  bool on_response_started_result_ = true;
  bool on_will_read_result_ = true;
  bool on_read_completed_result_ = true;

  bool defer_on_will_start_ = false;
  bool defer_on_response_started_ = false;
  bool defer_on_read_completed_ = false;
  bool defer_on_response_completed_ = false;

  int on_will_start_called_ = 0;
  int on_response_started_called_ = 0;
  int on_will_read_called_ = 0;
  int on_read_completed_called_ = 0;
  int on_response_completed_called_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_TEST_RESOURCE_HANDLER_H_
