// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "content/browser/fileapi/blob_storage_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/blob/blob_data_handle.h"
#include "webkit/browser/blob/blob_storage_context.h"

using webkit_blob::BlobDataHandle;

namespace content {

namespace {
void SetupBasicBlob(BlobStorageHost* host, const std::string& id) {
  EXPECT_TRUE(host->StartBuildingBlob(id));
  BlobData::Item item;
  item.SetToBytes("1", 1);
  EXPECT_TRUE(host->AppendBlobDataItem(id, item));
  EXPECT_TRUE(host->FinishBuildingBlob(id, "text/plain"));
  EXPECT_FALSE(host->StartBuildingBlob(id));
}
}  // namespace

TEST(BlobStorageContextTest, IncrementDecrementRef) {
  BlobStorageContext context;
  BlobStorageHost host(&context);
  base::MessageLoop fake_io_message_loop;

  // Build up a basic blob.
  const std::string kId("id");
  SetupBasicBlob(&host, kId);

  // Make sure it's there, finish building implies a ref of one.
  scoped_ptr<BlobDataHandle> blob_data_handle;
  blob_data_handle = context.GetBlobDataFromUUID(kId);
  EXPECT_TRUE(blob_data_handle);
  blob_data_handle.reset();
  {  // Clean up for ASAN
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  // Make sure its still there after inc/dec.
  EXPECT_TRUE(host.IncrementBlobRefCount(kId));
  EXPECT_TRUE(host.DecrementBlobRefCount(kId));
  blob_data_handle = context.GetBlobDataFromUUID(kId);
  EXPECT_TRUE(blob_data_handle);
  blob_data_handle.reset();
  {  // Clean up for ASAN
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  // Make sure it goes away in the end.
  EXPECT_TRUE(host.DecrementBlobRefCount(kId));
  blob_data_handle = context.GetBlobDataFromUUID(kId);
  EXPECT_FALSE(blob_data_handle);
  EXPECT_FALSE(host.DecrementBlobRefCount(kId));
  EXPECT_FALSE(host.IncrementBlobRefCount(kId));
}

TEST(BlobStorageContextTest, BlobDataHandle) {
  BlobStorageContext context;
  BlobStorageHost host(&context);
  base::MessageLoop fake_io_message_loop;

  // Build up a basic blob.
  const std::string kId("id");
  SetupBasicBlob(&host, kId);

  // Get a handle to it.
  scoped_ptr<BlobDataHandle> blob_data_handle =
      context.GetBlobDataFromUUID(kId);
  EXPECT_TRUE(blob_data_handle);

  // Drop the host's ref to it.
  EXPECT_TRUE(host.DecrementBlobRefCount(kId));

  // Should still be there due to the handle.
  scoped_ptr<BlobDataHandle> another_handle =
      context.GetBlobDataFromUUID(kId);
  EXPECT_TRUE(another_handle);

  // Should disappear after dropping both handles.
  blob_data_handle.reset();
  another_handle.reset();
  {  // Clean up for ASAN
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  blob_data_handle = context.GetBlobDataFromUUID(kId);
  EXPECT_FALSE(blob_data_handle);
}

TEST(BlobStorageContextTest, CompoundBlobs) {
  const std::string kId1("id1");
  const std::string kId2("id2");
  const std::string kId2Prime("id2.prime");

  base::MessageLoop fake_io_message_loop;

  // Setup a set of blob data for testing.
  base::Time time1, time2;
  base::Time::FromString("Tue, 15 Nov 1994, 12:45:26 GMT", &time1);
  base::Time::FromString("Mon, 14 Nov 1994, 11:30:49 GMT", &time2);

  scoped_refptr<BlobData> blob_data1(new BlobData(kId1));
  blob_data1->AppendData("Data1");
  blob_data1->AppendData("Data2");
  blob_data1->AppendFile(base::FilePath(FILE_PATH_LITERAL("File1.txt")),
    10, 1024, time1);

  scoped_refptr<BlobData> blob_data2(new BlobData(kId2));
  blob_data2->AppendData("Data3");
  blob_data2->AppendBlob(kId1, 8, 100);
  blob_data2->AppendFile(base::FilePath(FILE_PATH_LITERAL("File2.txt")),
      0, 20, time2);

  scoped_refptr<BlobData> canonicalized_blob_data2(new BlobData(kId2Prime));
  canonicalized_blob_data2->AppendData("Data3");
  canonicalized_blob_data2->AppendData("a2___", 2);
  canonicalized_blob_data2->AppendFile(
      base::FilePath(FILE_PATH_LITERAL("File1.txt")),
      10, 98, time1);
  canonicalized_blob_data2->AppendFile(
      base::FilePath(FILE_PATH_LITERAL("File2.txt")), 0, 20, time2);

  BlobStorageContext context;
  scoped_ptr<BlobDataHandle> blob_data_handle;

  // Test a blob referring to only data and a file.
  blob_data_handle = context.AddFinishedBlob(blob_data1.get());
  ASSERT_TRUE(blob_data_handle.get());
  EXPECT_TRUE(*(blob_data_handle->data()) == *blob_data1.get());

  // Test a blob composed in part with another blob.
  blob_data_handle = context.AddFinishedBlob(blob_data2.get());
  ASSERT_TRUE(blob_data_handle.get());
  EXPECT_TRUE(*(blob_data_handle->data()) == *canonicalized_blob_data2.get());

  blob_data_handle.reset();
  {  // Clean up for ASAN
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
}

TEST(BlobStorageContextTest, PublicBlobUrls) {
  BlobStorageContext context;
  BlobStorageHost host(&context);
  base::MessageLoop fake_io_message_loop;

  // Build up a basic blob.
  const std::string kId("id");
  SetupBasicBlob(&host, kId);

  // Now register a url for that blob.
  GURL kUrl("blob:id");
  EXPECT_TRUE(host.RegisterPublicBlobURL(kUrl, kId));
  scoped_ptr<BlobDataHandle> blob_data_handle =
      context.GetBlobDataFromPublicURL(kUrl);
  ASSERT_TRUE(blob_data_handle.get());
  EXPECT_EQ(kId, blob_data_handle->data()->uuid());
  blob_data_handle.reset();
  {  // Clean up for ASAN
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  // The url registration should keep the blob alive even after
  // explicit references are dropped.
  EXPECT_TRUE(host.DecrementBlobRefCount(kId));
  blob_data_handle = context.GetBlobDataFromPublicURL(kUrl);
  EXPECT_TRUE(blob_data_handle);
  blob_data_handle.reset();
  {  // Clean up for ASAN
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  // Finally get rid of the url registration and the blob.
  EXPECT_TRUE(host.RevokePublicBlobURL(kUrl));
  blob_data_handle = context.GetBlobDataFromPublicURL(kUrl);
  EXPECT_TRUE(!blob_data_handle.get());
  EXPECT_FALSE(host.RevokePublicBlobURL(kUrl));
}

TEST(BlobStorageContextTest, HostCleanup) {
  BlobStorageContext context;
  scoped_ptr<BlobStorageHost> host(new BlobStorageHost(&context));
  base::MessageLoop fake_io_message_loop;

  // Build up a basic blob and register a url
  const std::string kId("id");
  GURL kUrl("blob:id");
  SetupBasicBlob(host.get(), kId);
  EXPECT_TRUE(host->RegisterPublicBlobURL(kUrl, kId));

  // All should disappear upon host deletion.
  host.reset();
  scoped_ptr<BlobDataHandle> handle = context.GetBlobDataFromPublicURL(kUrl);
  EXPECT_TRUE(!handle.get());
  handle = context.GetBlobDataFromUUID(kId);
  EXPECT_TRUE(!handle.get());
}

TEST(BlobStorageContextTest, EarlyContextDeletion) {
  scoped_ptr<BlobStorageContext> context(new BlobStorageContext);
  BlobStorageHost host(context.get());
  base::MessageLoop fake_io_message_loop;

  // Deleting the context should not induce crashes.
  context.reset();

  const std::string kId("id");
  GURL kUrl("blob:id");
  EXPECT_FALSE(host.StartBuildingBlob(kId));
  BlobData::Item item;
  item.SetToBytes("1", 1);
  EXPECT_FALSE(host.AppendBlobDataItem(kId, item));
  EXPECT_FALSE(host.FinishBuildingBlob(kId, "text/plain"));
  EXPECT_FALSE(host.RegisterPublicBlobURL(kUrl, kId));
  EXPECT_FALSE(host.IncrementBlobRefCount(kId));
  EXPECT_FALSE(host.DecrementBlobRefCount(kId));
  EXPECT_FALSE(host.RevokePublicBlobURL(kUrl));
}

// TODO(michaeln): tests for the depcrecated url stuff

}  // namespace content
