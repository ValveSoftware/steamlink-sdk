// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_BLOB_BLOB_URL_REQUEST_JOB_H_
#define WEBKIT_BROWSER_BLOB_BLOB_URL_REQUEST_JOB_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_job.h"
#include "webkit/browser/webkit_storage_browser_export.h"
#include "webkit/common/blob/blob_data.h"

namespace base {
class MessageLoopProxy;
}

namespace fileapi {
class FileSystemContext;
}

namespace net {
class DrainableIOBuffer;
class IOBuffer;
}

namespace webkit_blob {

class FileStreamReader;

// A request job that handles reading blob URLs.
class WEBKIT_STORAGE_BROWSER_EXPORT BlobURLRequestJob
    : public net::URLRequestJob {
 public:
  BlobURLRequestJob(net::URLRequest* request,
                    net::NetworkDelegate* network_delegate,
                    BlobData* blob_data,
                    fileapi::FileSystemContext* file_system_context,
                    base::MessageLoopProxy* resolving_message_loop_proxy);

  // net::URLRequestJob methods.
  virtual void Start() OVERRIDE;
  virtual void Kill() OVERRIDE;
  virtual bool ReadRawData(net::IOBuffer* buf,
                           int buf_size,
                           int* bytes_read) OVERRIDE;
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;
  virtual void GetResponseInfo(net::HttpResponseInfo* info) OVERRIDE;
  virtual int GetResponseCode() const OVERRIDE;
  virtual void SetExtraRequestHeaders(
      const net::HttpRequestHeaders& headers) OVERRIDE;

 protected:
  virtual ~BlobURLRequestJob();

 private:
  typedef std::map<size_t, FileStreamReader*> IndexToReaderMap;

  // For preparing for read: get the size, apply the range and perform seek.
  void DidStart();
  bool AddItemLength(size_t index, int64 item_length);
  void CountSize();
  void DidCountSize(int error);
  void DidGetFileItemLength(size_t index, int64 result);
  void Seek(int64 offset);

  // For reading the blob.
  bool ReadLoop(int* bytes_read);
  bool ReadItem();
  void AdvanceItem();
  void AdvanceBytesRead(int result);
  bool ReadBytesItem(const BlobData::Item& item, int bytes_to_read);
  bool ReadFileItem(FileStreamReader* reader, int bytes_to_read);

  void DidReadFile(int result);
  void DeleteCurrentFileReader();

  int ComputeBytesToRead() const;
  int BytesReadCompleted();

  // These methods convert the result of blob data reading into response headers
  // and pass it to URLRequestJob's NotifyDone() or NotifyHeadersComplete().
  void NotifySuccess();
  void NotifyFailure(int);
  void HeadersCompleted(net::HttpStatusCode status_code);

  // Returns a FileStreamReader for a blob item at |index|.
  // If the item at |index| is not of file this returns NULL.
  FileStreamReader* GetFileStreamReader(size_t index);

  // Creates a FileStreamReader for the item at |index| with additional_offset.
  void CreateFileStreamReader(size_t index, int64 additional_offset);

  scoped_refptr<BlobData> blob_data_;

  // Variables for controlling read from |blob_data_|.
  scoped_refptr<fileapi::FileSystemContext> file_system_context_;
  scoped_refptr<base::MessageLoopProxy> file_thread_proxy_;
  std::vector<int64> item_length_list_;
  int64 total_size_;
  int64 remaining_bytes_;
  int pending_get_file_info_count_;
  IndexToReaderMap index_to_reader_;
  size_t current_item_index_;
  int64 current_item_offset_;

  // Holds the buffer for read data with the IOBuffer interface.
  scoped_refptr<net::DrainableIOBuffer> read_buf_;

  // Is set when NotifyFailure() is called and reset when DidStart is called.
  bool error_;

  bool byte_range_set_;
  net::HttpByteRange byte_range_;

  scoped_ptr<net::HttpResponseInfo> response_info_;

  base::WeakPtrFactory<BlobURLRequestJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlobURLRequestJob);
};

}  // namespace webkit_blob

#endif  // WEBKIT_BROWSER_BLOB_BLOB_URL_REQUEST_JOB_H_
