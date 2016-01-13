// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_FILE_DIR_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_FILE_DIR_JOB_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "net/base/directory_lister.h"
#include "net/url_request/url_request_job.h"

namespace net {

class URLRequestFileDirJob
  : public URLRequestJob,
    public DirectoryLister::DirectoryListerDelegate {
 public:
  URLRequestFileDirJob(URLRequest* request,
                       NetworkDelegate* network_delegate,
                       const base::FilePath& dir_path);

  bool list_complete() const { return list_complete_; }

  virtual void StartAsync();

  // Overridden from URLRequestJob:
  virtual void Start() OVERRIDE;
  virtual void Kill() OVERRIDE;
  virtual bool ReadRawData(IOBuffer* buf,
                           int buf_size,
                           int* bytes_read) OVERRIDE;
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;
  virtual bool GetCharset(std::string* charset) OVERRIDE;

  // Overridden from DirectoryLister::DirectoryListerDelegate:
  virtual void OnListFile(
      const DirectoryLister::DirectoryListerData& data) OVERRIDE;
  virtual void OnListDone(int error) OVERRIDE;

 private:
  virtual ~URLRequestFileDirJob();

  void CloseLister();

  // When we have data and a read has been pending, this function
  // will fill the response buffer and notify the request
  // appropriately.
  void CompleteRead();

  // Fills a buffer with the output.
  bool FillReadBuffer(char *buf, int buf_size, int *bytes_read);

  DirectoryLister lister_;
  base::FilePath dir_path_;
  std::string data_;
  bool canceled_;

  // Indicates whether we have the complete list of the dir
  bool list_complete_;

  // Indicates whether we have written the HTML header
  bool wrote_header_;

  // To simulate Async IO, we hold onto the Reader's buffer while
  // we wait for IO to complete.  When done, we fill the buffer
  // manually.
  bool read_pending_;
  scoped_refptr<IOBuffer> read_buffer_;
  int read_buffer_length_;
  base::WeakPtrFactory<URLRequestFileDirJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestFileDirJob);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_FILE_DIR_JOB_H_
