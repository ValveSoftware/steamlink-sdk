// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_SIMPLE_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_SIMPLE_JOB_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"
#include "net/url_request/url_range_request_job.h"

namespace net {

class URLRequest;

class NET_EXPORT URLRequestSimpleJob : public URLRangeRequestJob {
 public:
  URLRequestSimpleJob(URLRequest* request, NetworkDelegate* network_delegate);

  virtual void Start() OVERRIDE;
  virtual bool ReadRawData(IOBuffer* buf,
                           int buf_size,
                           int *bytes_read) OVERRIDE;
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;
  virtual bool GetCharset(std::string* charset) OVERRIDE;

 protected:
  virtual ~URLRequestSimpleJob();

  // Subclasses must override the way response data is determined.
  // The return value should be:
  //  - OK if data is obtained;
  //  - ERR_IO_PENDING if async processing is needed to finish obtaining data.
  //    This is the only case when |callback| should be called after
  //    completion of the operation. In other situations |callback| should
  //    never be called;
  //  - any other ERR_* code to indicate an error. This code will be used
  //    as the error code in the URLRequestStatus when the URLRequest
  //    is finished.
  virtual int GetData(std::string* mime_type,
                      std::string* charset,
                      std::string* data,
                      const CompletionCallback& callback) const = 0;

 protected:
  void StartAsync();

 private:
  void OnGetDataCompleted(int result);

  HttpByteRange byte_range_;
  std::string mime_type_;
  std::string charset_;
  std::string data_;
  int data_offset_;
  base::WeakPtrFactory<URLRequestSimpleJob> weak_factory_;
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_SIMPLE_JOB_H_
