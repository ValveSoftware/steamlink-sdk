// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_URL_REQUEST_JOB_H_
#define WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_URL_REQUEST_JOB_H_

#include <string>

#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/http/http_byte_range.h"
#include "net/url_request/url_request_job.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/webkit_storage_browser_export.h"

class GURL;

namespace base {
class FilePath;
}

namespace webkit_blob {
class FileStreamReader;
}

namespace fileapi {
class FileSystemContext;

// A request job that handles reading filesystem: URLs
class WEBKIT_STORAGE_BROWSER_EXPORT_PRIVATE FileSystemURLRequestJob
    : public net::URLRequestJob {
 public:
  FileSystemURLRequestJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const std::string& storage_domain,
      FileSystemContext* file_system_context);

  // URLRequestJob methods:
  virtual void Start() OVERRIDE;
  virtual void Kill() OVERRIDE;
  virtual bool ReadRawData(net::IOBuffer* buf,
                           int buf_size,
                           int* bytes_read) OVERRIDE;
  virtual bool IsRedirectResponse(GURL* location,
                                  int* http_status_code) OVERRIDE;
  virtual void SetExtraRequestHeaders(
      const net::HttpRequestHeaders& headers) OVERRIDE;
  virtual void GetResponseInfo(net::HttpResponseInfo* info) OVERRIDE;
  virtual int GetResponseCode() const OVERRIDE;

  // FilterContext methods (via URLRequestJob):
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;

 private:
  class CallbackDispatcher;

  virtual ~FileSystemURLRequestJob();

  void StartAsync();
  void DidAttemptAutoMount(base::File::Error result);
  void DidGetMetadata(base::File::Error error_code,
                      const base::File::Info& file_info);
  void DidRead(int result);
  void NotifyFailed(int rv);

  const std::string storage_domain_;
  FileSystemContext* file_system_context_;
  scoped_ptr<webkit_blob::FileStreamReader> reader_;
  FileSystemURL url_;
  bool is_directory_;
  scoped_ptr<net::HttpResponseInfo> response_info_;
  int64 remaining_bytes_;
  net::HttpByteRange byte_range_;
  base::WeakPtrFactory<FileSystemURLRequestJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemURLRequestJob);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_URL_REQUEST_JOB_H_
