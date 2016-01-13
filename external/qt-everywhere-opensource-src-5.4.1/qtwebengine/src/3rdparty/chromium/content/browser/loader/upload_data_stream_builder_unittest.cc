// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/upload_data_stream_builder.h"

#include <algorithm>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "content/common/resource_request_body.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_file_element_reader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "webkit/browser/blob/blob_storage_context.h"

using webkit_blob::BlobData;
using webkit_blob::BlobDataHandle;
using webkit_blob::BlobStorageContext;

namespace content {
namespace {

bool AreElementsEqual(const net::UploadElementReader& reader,
                      const ResourceRequestBody::Element& element) {
  switch(element.type()) {
    case ResourceRequestBody::Element::TYPE_BYTES: {
      const net::UploadBytesElementReader* bytes_reader =
          reader.AsBytesReader();
      return bytes_reader &&
          element.length() == bytes_reader->length() &&
          std::equal(element.bytes(), element.bytes() + element.length(),
                     bytes_reader->bytes());
    }
    case ResourceRequestBody::Element::TYPE_FILE: {
      const net::UploadFileElementReader* file_reader = reader.AsFileReader();
      return file_reader &&
          file_reader->path() == element.path() &&
          file_reader->range_offset() == element.offset() &&
          file_reader->range_length() == element.length() &&
          file_reader->expected_modification_time() ==
          element.expected_modification_time();
      break;
    }
    default:
      NOTREACHED();
  }
  return false;
}

}  // namespace

TEST(UploadDataStreamBuilderTest, CreateUploadDataStreamWithoutBlob) {
  base::MessageLoop message_loop;
  scoped_refptr<ResourceRequestBody> request_body = new ResourceRequestBody;

  const char kData[] = "123";
  const base::FilePath::StringType kFilePath = FILE_PATH_LITERAL("abc");
  const uint64 kFileOffset = 10U;
  const uint64 kFileLength = 100U;
  const base::Time kFileTime = base::Time::FromDoubleT(999);
  const int64 kIdentifier = 12345;

  request_body->AppendBytes(kData, arraysize(kData) - 1);
  request_body->AppendFileRange(base::FilePath(kFilePath),
                                kFileOffset, kFileLength, kFileTime);
  request_body->set_identifier(kIdentifier);

  scoped_ptr<net::UploadDataStream> upload(UploadDataStreamBuilder::Build(
      request_body.get(), NULL, NULL, base::MessageLoopProxy::current().get()));

  EXPECT_EQ(kIdentifier, upload->identifier());
  ASSERT_EQ(request_body->elements()->size(), upload->element_readers().size());

  const net::UploadBytesElementReader* r1 =
      upload->element_readers()[0]->AsBytesReader();
  ASSERT_TRUE(r1);
  EXPECT_EQ(kData, std::string(r1->bytes(), r1->length()));

  const net::UploadFileElementReader* r2 =
      upload->element_readers()[1]->AsFileReader();
  ASSERT_TRUE(r2);
  EXPECT_EQ(kFilePath, r2->path().value());
  EXPECT_EQ(kFileOffset, r2->range_offset());
  EXPECT_EQ(kFileLength, r2->range_length());
  EXPECT_EQ(kFileTime, r2->expected_modification_time());
}

TEST(UploadDataStreamBuilderTest, ResolveBlobAndCreateUploadDataStream) {
  base::MessageLoop message_loop;
  {
    // Setup blob data for testing.
    base::Time time1, time2;
    base::Time::FromString("Tue, 15 Nov 1994, 12:45:26 GMT", &time1);
    base::Time::FromString("Mon, 14 Nov 1994, 11:30:49 GMT", &time2);

    BlobStorageContext blob_storage_context;

    const std::string blob_id0("id-0");
    scoped_refptr<BlobData> blob_data(new BlobData(blob_id0));
    scoped_ptr<BlobDataHandle> handle1 =
        blob_storage_context.AddFinishedBlob(blob_data);

    const std::string blob_id1("id-1");
    blob_data = new BlobData(blob_id1);
    blob_data->AppendData("BlobData");
    blob_data->AppendFile(
        base::FilePath(FILE_PATH_LITERAL("BlobFile.txt")), 0, 20, time1);
    scoped_ptr<BlobDataHandle> handle2 =
        blob_storage_context.AddFinishedBlob(blob_data);

    // Setup upload data elements for comparison.
    ResourceRequestBody::Element blob_element1, blob_element2;
    blob_element1.SetToBytes(
        blob_data->items().at(0).bytes() +
        static_cast<int>(blob_data->items().at(0).offset()),
        static_cast<int>(blob_data->items().at(0).length()));
    blob_element2.SetToFilePathRange(
        blob_data->items().at(1).path(),
        blob_data->items().at(1).offset(),
        blob_data->items().at(1).length(),
        blob_data->items().at(1).expected_modification_time());

    ResourceRequestBody::Element upload_element1, upload_element2;
    upload_element1.SetToBytes("Hello", 5);
    upload_element2.SetToFilePathRange(
        base::FilePath(FILE_PATH_LITERAL("foo1.txt")), 0, 20, time2);

    // Test no blob reference.
    scoped_refptr<ResourceRequestBody> request_body(new ResourceRequestBody());
    request_body->AppendBytes(
        upload_element1.bytes(),
        upload_element1.length());
    request_body->AppendFileRange(
        upload_element2.path(),
        upload_element2.offset(),
        upload_element2.length(),
        upload_element2.expected_modification_time());

    scoped_ptr<net::UploadDataStream> upload(
        UploadDataStreamBuilder::Build(
            request_body.get(),
            &blob_storage_context,
            NULL,
            base::MessageLoopProxy::current().get()));

    ASSERT_EQ(2U, upload->element_readers().size());
    EXPECT_TRUE(AreElementsEqual(
        *upload->element_readers()[0], upload_element1));
    EXPECT_TRUE(AreElementsEqual(
        *upload->element_readers()[1], upload_element2));

    // Test having only one blob reference that refers to empty blob data.
    request_body = new ResourceRequestBody();
    request_body->AppendBlob(blob_id0);

    upload = UploadDataStreamBuilder::Build(
        request_body.get(),
        &blob_storage_context,
        NULL,
        base::MessageLoopProxy::current().get());
    ASSERT_EQ(0U, upload->element_readers().size());

    // Test having only one blob reference.
    request_body = new ResourceRequestBody();
    request_body->AppendBlob(blob_id1);

    upload = UploadDataStreamBuilder::Build(
        request_body.get(),
        &blob_storage_context,
        NULL,
        base::MessageLoopProxy::current().get());
    ASSERT_EQ(2U, upload->element_readers().size());
    EXPECT_TRUE(AreElementsEqual(
        *upload->element_readers()[0], blob_element1));
    EXPECT_TRUE(AreElementsEqual(
        *upload->element_readers()[1], blob_element2));

    // Test having one blob reference at the beginning.
    request_body = new ResourceRequestBody();
    request_body->AppendBlob(blob_id1);
    request_body->AppendBytes(
        upload_element1.bytes(),
        upload_element1.length());
    request_body->AppendFileRange(
        upload_element2.path(),
        upload_element2.offset(),
        upload_element2.length(),
        upload_element2.expected_modification_time());

    upload = UploadDataStreamBuilder::Build(
        request_body.get(),
        &blob_storage_context,
        NULL,
        base::MessageLoopProxy::current().get());
    ASSERT_EQ(4U, upload->element_readers().size());
    EXPECT_TRUE(AreElementsEqual(
        *upload->element_readers()[0], blob_element1));
    EXPECT_TRUE(AreElementsEqual(
        *upload->element_readers()[1], blob_element2));
    EXPECT_TRUE(AreElementsEqual(
        *upload->element_readers()[2], upload_element1));
    EXPECT_TRUE(AreElementsEqual(
        *upload->element_readers()[3], upload_element2));

    // Test having one blob reference at the end.
    request_body = new ResourceRequestBody();
    request_body->AppendBytes(
        upload_element1.bytes(),
        upload_element1.length());
    request_body->AppendFileRange(
        upload_element2.path(),
        upload_element2.offset(),
        upload_element2.length(),
        upload_element2.expected_modification_time());
    request_body->AppendBlob(blob_id1);

    upload =
        UploadDataStreamBuilder::Build(request_body.get(),
                                       &blob_storage_context,
                                       NULL,
                                       base::MessageLoopProxy::current().get());
    ASSERT_EQ(4U, upload->element_readers().size());
    EXPECT_TRUE(AreElementsEqual(
        *upload->element_readers()[0], upload_element1));
    EXPECT_TRUE(AreElementsEqual(
        *upload->element_readers()[1], upload_element2));
    EXPECT_TRUE(AreElementsEqual(
        *upload->element_readers()[2], blob_element1));
    EXPECT_TRUE(AreElementsEqual(
          *upload->element_readers()[3], blob_element2));

    // Test having one blob reference in the middle.
    request_body = new ResourceRequestBody();
    request_body->AppendBytes(
        upload_element1.bytes(),
        upload_element1.length());
    request_body->AppendBlob(blob_id1);
    request_body->AppendFileRange(
        upload_element2.path(),
        upload_element2.offset(),
        upload_element2.length(),
        upload_element2.expected_modification_time());

    upload = UploadDataStreamBuilder::Build(
        request_body.get(),
        &blob_storage_context,
        NULL,
        base::MessageLoopProxy::current().get());
    ASSERT_EQ(4U, upload->element_readers().size());
    EXPECT_TRUE(AreElementsEqual(
        *upload->element_readers()[0], upload_element1));
    EXPECT_TRUE(AreElementsEqual(
        *upload->element_readers()[1], blob_element1));
    EXPECT_TRUE(AreElementsEqual(
        *upload->element_readers()[2], blob_element2));
    EXPECT_TRUE(AreElementsEqual(
        *upload->element_readers()[3], upload_element2));

    // Test having multiple blob references.
    request_body = new ResourceRequestBody();
    request_body->AppendBlob(blob_id1);
    request_body->AppendBytes(
        upload_element1.bytes(),
        upload_element1.length());
    request_body->AppendBlob(blob_id1);
    request_body->AppendBlob(blob_id1);
    request_body->AppendFileRange(
        upload_element2.path(),
        upload_element2.offset(),
        upload_element2.length(),
        upload_element2.expected_modification_time());

    upload = UploadDataStreamBuilder::Build(
        request_body.get(),
        &blob_storage_context,
        NULL,
        base::MessageLoopProxy::current().get());
    ASSERT_EQ(8U, upload->element_readers().size());
    EXPECT_TRUE(AreElementsEqual(
        *upload->element_readers()[0], blob_element1));
    EXPECT_TRUE(AreElementsEqual(
        *upload->element_readers()[1], blob_element2));
    EXPECT_TRUE(AreElementsEqual(
        *upload->element_readers()[2], upload_element1));
    EXPECT_TRUE(AreElementsEqual(
        *upload->element_readers()[3], blob_element1));
    EXPECT_TRUE(AreElementsEqual(
        *upload->element_readers()[4], blob_element2));
    EXPECT_TRUE(AreElementsEqual(
        *upload->element_readers()[5], blob_element1));
    EXPECT_TRUE(AreElementsEqual(
        *upload->element_readers()[6], blob_element2));
    EXPECT_TRUE(AreElementsEqual(
        *upload->element_readers()[7], upload_element2));
  }
  // Clean up for ASAN.
  base::RunLoop().RunUntilIdle();
}

}  // namespace content
