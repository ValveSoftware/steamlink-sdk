// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_UPLOAD_DATA_STREAM_BUILDER_H_
#define CONTENT_BROWSER_LOADER_UPLOAD_DATA_STREAM_BUILDER_H_

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"

namespace base {
class TaskRunner;
}

namespace fileapi {
class FileSystemContext;
}

namespace net {
class UploadDataStream;
}

namespace webkit_blob {
class BlobStorageContext;
}

namespace content {

class ResourceRequestBody;

class CONTENT_EXPORT UploadDataStreamBuilder {
 public:
  // Creates a new UploadDataStream from this request body.
  //
  // This also resolves any blob references using the given |blob_context|
  // and binds those blob references to the ResourceRequestBody ensuring that
  // the blob data remains valid for the lifetime of the ResourceRequestBody
  // object.
  //
  // |file_system_context| is used to create a FileStreamReader for files with
  // filesystem URLs.  |file_task_runner| is used to perform file operations
  // when the data gets uploaded.
  static scoped_ptr<net::UploadDataStream> Build(
      ResourceRequestBody* body,
      webkit_blob::BlobStorageContext* blob_context,
      fileapi::FileSystemContext* file_system_context,
      base::TaskRunner* file_task_runner);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_UPLOAD_DATA_STREAM_BUILDER_H_
