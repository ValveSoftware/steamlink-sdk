// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/upload_data_stream_builder.h"

#include <stdint.h>

#include <limits>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "content/browser/fileapi/upload_file_system_file_element_reader.h"
#include "content/common/resource_request_body_impl.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_file_element_reader.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_reader.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/upload_blob_element_reader.h"

namespace disk_cache {
class Entry;
}

namespace content {
namespace {

// A subclass of net::UploadBytesElementReader which owns
// ResourceRequestBodyImpl.
class BytesElementReader : public net::UploadBytesElementReader {
 public:
  BytesElementReader(ResourceRequestBodyImpl* resource_request_body,
                     const ResourceRequestBodyImpl::Element& element)
      : net::UploadBytesElementReader(element.bytes(), element.length()),
        resource_request_body_(resource_request_body) {
    DCHECK_EQ(ResourceRequestBodyImpl::Element::TYPE_BYTES, element.type());
  }

  ~BytesElementReader() override {}

 private:
  scoped_refptr<ResourceRequestBodyImpl> resource_request_body_;

  DISALLOW_COPY_AND_ASSIGN(BytesElementReader);
};

// A subclass of net::UploadFileElementReader which owns
// ResourceRequestBodyImpl.
// This class is necessary to ensure the BlobData and any attached shareable
// files survive until upload completion.
class FileElementReader : public net::UploadFileElementReader {
 public:
  FileElementReader(ResourceRequestBodyImpl* resource_request_body,
                    base::TaskRunner* task_runner,
                    const ResourceRequestBodyImpl::Element& element)
      : net::UploadFileElementReader(task_runner,
                                     element.path(),
                                     element.offset(),
                                     element.length(),
                                     element.expected_modification_time()),
        resource_request_body_(resource_request_body) {
    DCHECK_EQ(ResourceRequestBodyImpl::Element::TYPE_FILE, element.type());
  }

  ~FileElementReader() override {}

 private:
  scoped_refptr<ResourceRequestBodyImpl> resource_request_body_;

  DISALLOW_COPY_AND_ASSIGN(FileElementReader);
};

}  // namespace

std::unique_ptr<net::UploadDataStream> UploadDataStreamBuilder::Build(
    ResourceRequestBodyImpl* body,
    storage::BlobStorageContext* blob_context,
    storage::FileSystemContext* file_system_context,
    base::SingleThreadTaskRunner* file_task_runner) {
  std::vector<std::unique_ptr<net::UploadElementReader>> element_readers;
  for (const auto& element : *body->elements()) {
    switch (element.type()) {
      case ResourceRequestBodyImpl::Element::TYPE_BYTES:
        element_readers.push_back(
            base::WrapUnique(new BytesElementReader(body, element)));
        break;
      case ResourceRequestBodyImpl::Element::TYPE_FILE:
        element_readers.push_back(base::WrapUnique(
            new FileElementReader(body, file_task_runner, element)));
        break;
      case ResourceRequestBodyImpl::Element::TYPE_FILE_FILESYSTEM:
        // If |body| contains any filesystem URLs, the caller should have
        // supplied a FileSystemContext.
        DCHECK(file_system_context);
        element_readers.push_back(
            base::WrapUnique(new content::UploadFileSystemFileElementReader(
                file_system_context, element.filesystem_url(), element.offset(),
                element.length(), element.expected_modification_time())));
        break;
      case ResourceRequestBodyImpl::Element::TYPE_BLOB: {
        DCHECK_EQ(std::numeric_limits<uint64_t>::max(), element.length());
        DCHECK_EQ(0ul, element.offset());
        std::unique_ptr<storage::BlobDataHandle> handle =
            blob_context->GetBlobDataFromUUID(element.blob_uuid());
        element_readers.push_back(
            base::WrapUnique(new storage::UploadBlobElementReader(
                std::move(handle), file_system_context, file_task_runner)));
        break;
      }
      case ResourceRequestBodyImpl::Element::TYPE_DISK_CACHE_ENTRY:
      case ResourceRequestBodyImpl::Element::TYPE_BYTES_DESCRIPTION:
      case ResourceRequestBodyImpl::Element::TYPE_UNKNOWN:
        NOTREACHED();
        break;
    }
  }

  return base::WrapUnique(new net::ElementsUploadDataStream(
      std::move(element_readers), body->identifier()));
}

}  // namespace content
