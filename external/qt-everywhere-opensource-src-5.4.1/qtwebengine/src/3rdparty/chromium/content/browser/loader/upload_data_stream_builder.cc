// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/upload_data_stream_builder.h"

#include "base/logging.h"
#include "content/browser/fileapi/upload_file_system_file_element_reader.h"
#include "content/common/resource_request_body.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_file_element_reader.h"
#include "webkit/browser/blob/blob_data_handle.h"
#include "webkit/browser/blob/blob_storage_context.h"

using webkit_blob::BlobData;
using webkit_blob::BlobDataHandle;
using webkit_blob::BlobStorageContext;

namespace content {
namespace {

// A subclass of net::UploadBytesElementReader which owns ResourceRequestBody.
class BytesElementReader : public net::UploadBytesElementReader {
 public:
  BytesElementReader(ResourceRequestBody* resource_request_body,
                     const ResourceRequestBody::Element& element)
      : net::UploadBytesElementReader(element.bytes(), element.length()),
        resource_request_body_(resource_request_body) {
    DCHECK_EQ(ResourceRequestBody::Element::TYPE_BYTES, element.type());
  }

  virtual ~BytesElementReader() {}

 private:
  scoped_refptr<ResourceRequestBody> resource_request_body_;

  DISALLOW_COPY_AND_ASSIGN(BytesElementReader);
};

// A subclass of net::UploadFileElementReader which owns ResourceRequestBody.
// This class is necessary to ensure the BlobData and any attached shareable
// files survive until upload completion.
class FileElementReader : public net::UploadFileElementReader {
 public:
  FileElementReader(ResourceRequestBody* resource_request_body,
                    base::TaskRunner* task_runner,
                    const ResourceRequestBody::Element& element)
      : net::UploadFileElementReader(task_runner,
                                     element.path(),
                                     element.offset(),
                                     element.length(),
                                     element.expected_modification_time()),
        resource_request_body_(resource_request_body) {
    DCHECK_EQ(ResourceRequestBody::Element::TYPE_FILE, element.type());
  }

  virtual ~FileElementReader() {}

 private:
  scoped_refptr<ResourceRequestBody> resource_request_body_;

  DISALLOW_COPY_AND_ASSIGN(FileElementReader);
};

void ResolveBlobReference(
    ResourceRequestBody* body,
    webkit_blob::BlobStorageContext* blob_context,
    const ResourceRequestBody::Element& element,
    std::vector<const ResourceRequestBody::Element*>* resolved_elements) {
  DCHECK(blob_context);
  scoped_ptr<webkit_blob::BlobDataHandle> handle =
      blob_context->GetBlobDataFromUUID(element.blob_uuid());
  DCHECK(handle);
  if (!handle)
    return;

  // If there is no element in the referred blob data, just return.
  if (handle->data()->items().empty())
    return;

  // Append the elements in the referenced blob data.
  for (size_t i = 0; i < handle->data()->items().size(); ++i) {
    const BlobData::Item& item = handle->data()->items().at(i);
    DCHECK_NE(BlobData::Item::TYPE_BLOB, item.type());
    resolved_elements->push_back(&item);
  }

  // Ensure the blob and any attached shareable files survive until
  // upload completion. The |body| takes ownership of |handle|.
  const void* key = handle.get();
  body->SetUserData(key, handle.release());
}

}  // namespace

scoped_ptr<net::UploadDataStream> UploadDataStreamBuilder::Build(
    ResourceRequestBody* body,
    BlobStorageContext* blob_context,
    fileapi::FileSystemContext* file_system_context,
    base::TaskRunner* file_task_runner) {
  // Resolve all blob elements.
  std::vector<const ResourceRequestBody::Element*> resolved_elements;
  for (size_t i = 0; i < body->elements()->size(); ++i) {
    const ResourceRequestBody::Element& element = (*body->elements())[i];
    if (element.type() == ResourceRequestBody::Element::TYPE_BLOB)
      ResolveBlobReference(body, blob_context, element, &resolved_elements);
    else
      resolved_elements.push_back(&element);
  }

  ScopedVector<net::UploadElementReader> element_readers;
  for (size_t i = 0; i < resolved_elements.size(); ++i) {
    const ResourceRequestBody::Element& element = *resolved_elements[i];
    switch (element.type()) {
      case ResourceRequestBody::Element::TYPE_BYTES:
        element_readers.push_back(new BytesElementReader(body, element));
        break;
      case ResourceRequestBody::Element::TYPE_FILE:
        element_readers.push_back(
            new FileElementReader(body, file_task_runner, element));
        break;
      case ResourceRequestBody::Element::TYPE_FILE_FILESYSTEM:
        element_readers.push_back(
            new content::UploadFileSystemFileElementReader(
                file_system_context,
                element.filesystem_url(),
                element.offset(),
                element.length(),
                element.expected_modification_time()));
        break;
      case ResourceRequestBody::Element::TYPE_BLOB:
        // Blob elements should be resolved beforehand.
        NOTREACHED();
        break;
      case ResourceRequestBody::Element::TYPE_UNKNOWN:
        NOTREACHED();
        break;
    }
  }

  return make_scoped_ptr(
      new net::UploadDataStream(element_readers.Pass(), body->identifier()));
}

}  // namespace content
