// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILEAPI_UPLOAD_FILE_SYSTEM_FILE_ELEMENT_READER_H_
#define CONTENT_BROWSER_FILEAPI_UPLOAD_FILE_SYSTEM_FILE_ELEMENT_READER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "net/base/upload_element_reader.h"
#include "url/gurl.h"

namespace storage {
class FileStreamReader;
}

namespace storage {
class FileSystemContext;
}

namespace content {

// An UploadElementReader implementation for filesystem file.
class CONTENT_EXPORT UploadFileSystemFileElementReader :
    NON_EXPORTED_BASE(public net::UploadElementReader) {
 public:
  UploadFileSystemFileElementReader(
      storage::FileSystemContext* file_system_context,
      const GURL& url,
      uint64_t range_offset,
      uint64_t range_length,
      const base::Time& expected_modification_time);
  ~UploadFileSystemFileElementReader() override;

  // UploadElementReader overrides:
  int Init(const net::CompletionCallback& callback) override;
  uint64_t GetContentLength() const override;
  uint64_t BytesRemaining() const override;
  int Read(net::IOBuffer* buf,
           int buf_length,
           const net::CompletionCallback& callback) override;

 private:
  void OnGetLength(const net::CompletionCallback& callback, int64_t result);
  void OnRead(const net::CompletionCallback& callback, int result);

  scoped_refptr<storage::FileSystemContext> file_system_context_;
  const GURL url_;
  const uint64_t range_offset_;
  const uint64_t range_length_;
  const base::Time expected_modification_time_;

  std::unique_ptr<storage::FileStreamReader> stream_reader_;

  uint64_t stream_length_;
  uint64_t position_;

  base::WeakPtrFactory<UploadFileSystemFileElementReader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UploadFileSystemFileElementReader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILEAPI_UPLOAD_FILE_SYSTEM_FILE_ELEMENT_READER_H_
