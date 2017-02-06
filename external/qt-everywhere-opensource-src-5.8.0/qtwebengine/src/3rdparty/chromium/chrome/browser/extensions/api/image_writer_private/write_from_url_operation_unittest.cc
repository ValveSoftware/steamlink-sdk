// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "chrome/browser/extensions/api/image_writer_private/write_from_url_operation.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/test_url_request_interceptor.h"

namespace extensions {
namespace image_writer {

namespace {

using content::BrowserThread;
using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::Gt;
using testing::Lt;

const char kTestImageUrl[] = "http://localhost/test/image.zip";

typedef net::LocalHostTestURLRequestInterceptor GetInterceptor;

// This class gives us a generic Operation with the ability to set or inspect
// the current path to the image file.
class OperationForTest : public WriteFromUrlOperation {
 public:
  OperationForTest(base::WeakPtr<OperationManager> manager_,
                   const ExtensionId& extension_id,
                   net::URLRequestContextGetter* request_context,
                   GURL url,
                   const std::string& hash,
                   const std::string& storage_unit_id)
      : WriteFromUrlOperation(manager_,
                              extension_id,
                              request_context,
                              url,
                              hash,
                              storage_unit_id) {}

  void StartImpl() override {}

  // Expose stages for testing.
  void GetDownloadTarget(const base::Closure& continuation) {
    WriteFromUrlOperation::GetDownloadTarget(continuation);
  }

  void Download(const base::Closure& continuation) {
    WriteFromUrlOperation::Download(continuation);
  }

  void VerifyDownload(const base::Closure& continuation) {
    WriteFromUrlOperation::VerifyDownload(continuation);
  }

  // Helpers to set-up state for intermediate stages.
  void SetImagePath(const base::FilePath image_path) {
    image_path_ = image_path;
  }

  base::FilePath GetImagePath() { return image_path_; }

 private:
  ~OperationForTest() override{};
};

class ImageWriterWriteFromUrlOperationTest : public ImageWriterUnitTestBase {
 protected:
  ImageWriterWriteFromUrlOperationTest() : manager_(&test_profile_) {}

  void SetUp() override {
    ImageWriterUnitTestBase::SetUp();

    // Turn on interception and set up our dummy file.
    get_interceptor_.reset(new GetInterceptor(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
        BrowserThread::GetBlockingPool()->GetTaskRunnerWithShutdownBehavior(
            base::SequencedWorkerPool::SKIP_ON_SHUTDOWN)));
    get_interceptor_->SetResponse(GURL(kTestImageUrl),
                                  test_utils_.GetImagePath());
  }

  void TearDown() override {
    ImageWriterUnitTestBase::TearDown();
  }

  scoped_refptr<OperationForTest> CreateOperation(const GURL& url,
                                                  const std::string& hash) {
    scoped_refptr<OperationForTest> operation(
        new OperationForTest(manager_.AsWeakPtr(),
                             kDummyExtensionId,
                             test_profile_.GetRequestContext(),
                             url,
                             hash,
                             test_utils_.GetDevicePath().AsUTF8Unsafe()));
    operation->Start();
    return operation;
  }

  TestingProfile test_profile_;
  std::unique_ptr<GetInterceptor> get_interceptor_;

  MockOperationManager manager_;
};

TEST_F(ImageWriterWriteFromUrlOperationTest, SelectTargetWithoutExtension) {
  scoped_refptr<OperationForTest> operation =
      CreateOperation(GURL("http://localhost/foo/bar"), "");

  operation->GetDownloadTarget(base::Bind(&base::DoNothing));

  EXPECT_EQ(FILE_PATH_LITERAL("bar"),
            operation->GetImagePath().BaseName().value());

  operation->Cancel();
}

TEST_F(ImageWriterWriteFromUrlOperationTest, SelectTargetWithExtension) {
  scoped_refptr<OperationForTest> operation =
      CreateOperation(GURL("http://localhost/foo/bar.zip"), "");

  operation->GetDownloadTarget(base::Bind(&base::DoNothing));

  EXPECT_EQ(FILE_PATH_LITERAL("bar.zip"),
            operation->GetImagePath().BaseName().value());

  operation->Cancel();
}

TEST_F(ImageWriterWriteFromUrlOperationTest, DownloadFile) {
  // This test actually triggers the URL fetch code, which will drain the
  // message queues while waiting for IO, thus we have to run until the
  // operation completes.
  base::RunLoop runloop;
  base::Closure quit_closure = runloop.QuitClosure();
  base::FilePath download_target_path;
  scoped_refptr<OperationForTest> operation =
      CreateOperation(GURL(kTestImageUrl), "");

  EXPECT_TRUE(base::CreateTemporaryFileInDir(test_utils_.GetTempDir(),
                                             &download_target_path));
  operation->SetImagePath(download_target_path);

  EXPECT_CALL(
      manager_,
      OnProgress(kDummyExtensionId, image_writer_api::STAGE_DOWNLOAD, _))
      .Times(AtLeast(1));
  EXPECT_CALL(
      manager_,
      OnProgress(kDummyExtensionId, image_writer_api::STAGE_DOWNLOAD, 0))
      .Times(AnyNumber());
  EXPECT_CALL(
      manager_,
      OnProgress(kDummyExtensionId, image_writer_api::STAGE_DOWNLOAD, 100))
      .Times(AnyNumber());

  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&OperationForTest::Download, operation, quit_closure));

  runloop.Run();

  EXPECT_TRUE(base::ContentsEqual(test_utils_.GetImagePath(),
                                  operation->GetImagePath()));

  EXPECT_EQ(1, get_interceptor_->GetHitCount());

  operation->Cancel();
}

TEST_F(ImageWriterWriteFromUrlOperationTest, VerifyFile) {
  std::unique_ptr<char[]> data_buffer(new char[kTestFileSize]);
  base::ReadFile(test_utils_.GetImagePath(), data_buffer.get(), kTestFileSize);
  base::MD5Digest expected_digest;
  base::MD5Sum(data_buffer.get(), kTestFileSize, &expected_digest);
  std::string expected_hash = base::MD5DigestToBase16(expected_digest);

  scoped_refptr<OperationForTest> operation =
      CreateOperation(GURL(""), expected_hash);

  EXPECT_CALL(
      manager_,
      OnProgress(kDummyExtensionId, image_writer_api::STAGE_VERIFYDOWNLOAD, _))
      .Times(AtLeast(1));
  EXPECT_CALL(
      manager_,
      OnProgress(kDummyExtensionId, image_writer_api::STAGE_VERIFYDOWNLOAD, 0))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId,
                         image_writer_api::STAGE_VERIFYDOWNLOAD,
                         100)).Times(AtLeast(1));

  operation->SetImagePath(test_utils_.GetImagePath());
  content::BrowserThread::PostTask(content::BrowserThread::FILE,
                                   FROM_HERE,
                                   base::Bind(&OperationForTest::VerifyDownload,
                                              operation,
                                              base::Bind(&base::DoNothing)));

  base::RunLoop().RunUntilIdle();

  operation->Cancel();
}

}  // namespace

}  // namespace image_writer
}  // namespace extensions
