// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILEAPI_MOCK_URL_REQUEST_DELEGATE_H
#define CONTENT_BROWSER_FILEAPI_MOCK_URL_REQUEST_DELEGATE_H

#include "net/url_request/url_request.h"

namespace net {
class IOBuffer;
}

namespace content {

// A URL request delegate that receives the response body.
class MockURLRequestDelegate : public net::URLRequest::Delegate {
 public:
  MockURLRequestDelegate();
  virtual ~MockURLRequestDelegate();

  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE;
  virtual void OnReadCompleted(net::URLRequest* request,
                               int bytes_read) OVERRIDE;
  const std::string& response_data() const { return response_data_; }

 private:
  void ReadSome(net::URLRequest* request);
  void ReceiveData(net::URLRequest* request, int bytes_read);
  void RequestComplete();

  scoped_refptr<net::IOBuffer> io_buffer_;
  std::string response_data_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILEAPI_MOCK_URL_REQUEST_DELEGATE_H
