// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_test_job.h"

#include <algorithm>
#include <list>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"

namespace net {

namespace {

typedef std::list<URLRequestTestJob*> URLRequestJobList;
base::LazyInstance<URLRequestJobList>::Leaky
    g_pending_jobs = LAZY_INSTANCE_INITIALIZER;

class TestJobProtocolHandler : public URLRequestJobFactory::ProtocolHandler {
 public:
  // URLRequestJobFactory::ProtocolHandler implementation:
  virtual URLRequestJob* MaybeCreateJob(
      URLRequest* request, NetworkDelegate* network_delegate) const OVERRIDE {
    return new URLRequestTestJob(request, network_delegate);
  }
};

}  // namespace

// static getters for known URLs
GURL URLRequestTestJob::test_url_1() {
  return GURL("test:url1");
}
GURL URLRequestTestJob::test_url_2() {
  return GURL("test:url2");
}
GURL URLRequestTestJob::test_url_3() {
  return GURL("test:url3");
}
GURL URLRequestTestJob::test_url_4() {
  return GURL("test:url4");
}
GURL URLRequestTestJob::test_url_error() {
  return GURL("test:error");
}
GURL URLRequestTestJob::test_url_redirect_to_url_2() {
  return GURL("test:redirect_to_2");
}

// static getters for known URL responses
std::string URLRequestTestJob::test_data_1() {
  return std::string("<html><title>Test One</title></html>");
}
std::string URLRequestTestJob::test_data_2() {
  return std::string("<html><title>Test Two Two</title></html>");
}
std::string URLRequestTestJob::test_data_3() {
  return std::string("<html><title>Test Three Three Three</title></html>");
}
std::string URLRequestTestJob::test_data_4() {
  return std::string("<html><title>Test Four Four Four Four</title></html>");
}

// static getter for simple response headers
std::string URLRequestTestJob::test_headers() {
  static const char kHeaders[] =
      "HTTP/1.1 200 OK\0"
      "Content-type: text/html\0"
      "\0";
  return std::string(kHeaders, arraysize(kHeaders));
}

// static getter for redirect response headers
std::string URLRequestTestJob::test_redirect_headers() {
  static const char kHeaders[] =
      "HTTP/1.1 302 MOVED\0"
      "Location: somewhere\0"
      "\0";
  return std::string(kHeaders, arraysize(kHeaders));
}

// static getter for redirect response headers
std::string URLRequestTestJob::test_redirect_to_url_2_headers() {
  std::string headers = "HTTP/1.1 302 MOVED";
  headers.push_back('\0');
  headers += "Location: ";
  headers += test_url_2().spec();
  headers.push_back('\0');
  headers.push_back('\0');
  return headers;
}

// static getter for error response headers
std::string URLRequestTestJob::test_error_headers() {
  static const char kHeaders[] =
      "HTTP/1.1 500 BOO HOO\0"
      "\0";
  return std::string(kHeaders, arraysize(kHeaders));
}

// static
URLRequestJobFactory::ProtocolHandler*
URLRequestTestJob::CreateProtocolHandler() {
  return new TestJobProtocolHandler();
}

URLRequestTestJob::URLRequestTestJob(URLRequest* request,
                                     NetworkDelegate* network_delegate)
    : URLRequestJob(request, network_delegate),
      auto_advance_(false),
      stage_(WAITING),
      priority_(DEFAULT_PRIORITY),
      offset_(0),
      async_buf_(NULL),
      async_buf_size_(0),
      weak_factory_(this) {
}

URLRequestTestJob::URLRequestTestJob(URLRequest* request,
                                     NetworkDelegate* network_delegate,
                                     bool auto_advance)
    : URLRequestJob(request, network_delegate),
      auto_advance_(auto_advance),
      stage_(WAITING),
      priority_(DEFAULT_PRIORITY),
      offset_(0),
      async_buf_(NULL),
      async_buf_size_(0),
      weak_factory_(this) {
}

URLRequestTestJob::URLRequestTestJob(URLRequest* request,
                                     NetworkDelegate* network_delegate,
                                     const std::string& response_headers,
                                     const std::string& response_data,
                                     bool auto_advance)
    : URLRequestJob(request, network_delegate),
      auto_advance_(auto_advance),
      stage_(WAITING),
      priority_(DEFAULT_PRIORITY),
      response_headers_(new HttpResponseHeaders(response_headers)),
      response_data_(response_data),
      offset_(0),
      async_buf_(NULL),
      async_buf_size_(0),
      weak_factory_(this) {
}

URLRequestTestJob::~URLRequestTestJob() {
  g_pending_jobs.Get().erase(
      std::remove(
          g_pending_jobs.Get().begin(), g_pending_jobs.Get().end(), this),
      g_pending_jobs.Get().end());
}

bool URLRequestTestJob::GetMimeType(std::string* mime_type) const {
  DCHECK(mime_type);
  if (!response_headers_.get())
    return false;
  return response_headers_->GetMimeType(mime_type);
}

void URLRequestTestJob::SetPriority(RequestPriority priority) {
  priority_ = priority;
}

void URLRequestTestJob::Start() {
  // Start reading asynchronously so that all error reporting and data
  // callbacks happen as they would for network requests.
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&URLRequestTestJob::StartAsync,
                            weak_factory_.GetWeakPtr()));
}

void URLRequestTestJob::StartAsync() {
  if (!response_headers_.get()) {
    response_headers_ = new HttpResponseHeaders(test_headers());
    if (request_->url().spec() == test_url_1().spec()) {
      response_data_ = test_data_1();
      stage_ = DATA_AVAILABLE;  // Simulate a synchronous response for this one.
    } else if (request_->url().spec() == test_url_2().spec()) {
      response_data_ = test_data_2();
    } else if (request_->url().spec() == test_url_3().spec()) {
      response_data_ = test_data_3();
    } else if (request_->url().spec() == test_url_4().spec()) {
      response_data_ = test_data_4();
    } else if (request_->url().spec() == test_url_redirect_to_url_2().spec()) {
      response_headers_ =
          new HttpResponseHeaders(test_redirect_to_url_2_headers());
    } else {
      AdvanceJob();

      // unexpected url, return error
      // FIXME(brettw) we may want to use WININET errors or have some more types
      // of errors
      NotifyDone(URLRequestStatus(URLRequestStatus::FAILED,
                                  ERR_INVALID_URL));
      // FIXME(brettw): this should emulate a network error, and not just fail
      // initiating a connection
      return;
    }
  }

  AdvanceJob();

  this->NotifyHeadersComplete();
}

bool URLRequestTestJob::ReadRawData(IOBuffer* buf, int buf_size,
                                    int *bytes_read) {
  if (stage_ == WAITING) {
    async_buf_ = buf;
    async_buf_size_ = buf_size;
    SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
    return false;
  }

  DCHECK(bytes_read);
  *bytes_read = 0;

  if (offset_ >= static_cast<int>(response_data_.length())) {
    return true;  // done reading
  }

  int to_read = buf_size;
  if (to_read + offset_ > static_cast<int>(response_data_.length()))
    to_read = static_cast<int>(response_data_.length()) - offset_;

  memcpy(buf->data(), &response_data_.c_str()[offset_], to_read);
  offset_ += to_read;

  *bytes_read = to_read;
  return true;
}

void URLRequestTestJob::GetResponseInfo(HttpResponseInfo* info) {
  if (response_headers_.get())
    info->headers = response_headers_;
}

void URLRequestTestJob::GetLoadTimingInfo(
    LoadTimingInfo* load_timing_info) const {
  // Preserve the times the URLRequest is responsible for, but overwrite all
  // the others.
  base::TimeTicks request_start = load_timing_info->request_start;
  base::Time request_start_time = load_timing_info->request_start_time;
  *load_timing_info = load_timing_info_;
  load_timing_info->request_start = request_start;
  load_timing_info->request_start_time = request_start_time;
}

int URLRequestTestJob::GetResponseCode() const {
  if (response_headers_.get())
    return response_headers_->response_code();
  return -1;
}

bool URLRequestTestJob::IsRedirectResponse(GURL* location,
                                           int* http_status_code) {
  if (!response_headers_.get())
    return false;

  std::string value;
  if (!response_headers_->IsRedirect(&value))
    return false;

  *location = request_->url().Resolve(value);
  *http_status_code = response_headers_->response_code();
  return true;
}

void URLRequestTestJob::Kill() {
  stage_ = DONE;
  URLRequestJob::Kill();
  weak_factory_.InvalidateWeakPtrs();
  g_pending_jobs.Get().erase(
      std::remove(
          g_pending_jobs.Get().begin(), g_pending_jobs.Get().end(), this),
      g_pending_jobs.Get().end());
}

void URLRequestTestJob::ProcessNextOperation() {
  switch (stage_) {
    case WAITING:
      // Must call AdvanceJob() prior to NotifyReadComplete() since that may
      // delete |this|.
      AdvanceJob();
      stage_ = DATA_AVAILABLE;
      // OK if ReadRawData wasn't called yet.
      if (async_buf_) {
        int bytes_read;
        if (!ReadRawData(async_buf_, async_buf_size_, &bytes_read))
          NOTREACHED() << "This should not return false in DATA_AVAILABLE.";
        SetStatus(URLRequestStatus());  // clear the io pending flag
        if (NextReadAsync()) {
          // Make all future reads return io pending until the next
          // ProcessNextOperation().
          stage_ = WAITING;
        }
        NotifyReadComplete(bytes_read);
      }
      break;
    case DATA_AVAILABLE:
      AdvanceJob();
      stage_ = ALL_DATA;  // done sending data
      break;
    case ALL_DATA:
      stage_ = DONE;
      return;
    case DONE:
      return;
    default:
      NOTREACHED() << "Invalid stage";
      return;
  }
}

bool URLRequestTestJob::NextReadAsync() {
  return false;
}

void URLRequestTestJob::AdvanceJob() {
  if (auto_advance_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&URLRequestTestJob::ProcessNextOperation,
                              weak_factory_.GetWeakPtr()));
    return;
  }
  g_pending_jobs.Get().push_back(this);
}

// static
bool URLRequestTestJob::ProcessOnePendingMessage() {
  if (g_pending_jobs.Get().empty())
    return false;

  URLRequestTestJob* next_job(g_pending_jobs.Get().front());
  g_pending_jobs.Get().pop_front();

  DCHECK(!next_job->auto_advance());  // auto_advance jobs should be in this q
  next_job->ProcessNextOperation();
  return true;
}

}  // namespace net
