// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "chrome/browser/extensions/api/image_writer_private/write_from_file_operation.h"
#include "chrome/test/base/testing_profile.h"

namespace extensions {
namespace image_writer {

using testing::_;
using testing::Lt;
using testing::AnyNumber;
using testing::AtLeast;

class ImageWriterFromFileTest : public ImageWriterUnitTestBase {
 protected:
  ImageWriterFromFileTest()
      : profile_(new TestingProfile), manager_(profile_.get()) {}
  std::unique_ptr<TestingProfile> profile_;
  MockOperationManager manager_;
};

TEST_F(ImageWriterFromFileTest, InvalidFile) {
  scoped_refptr<WriteFromFileOperation> op =
      new WriteFromFileOperation(manager_.AsWeakPtr(),
                                 kDummyExtensionId,
                                 test_utils_.GetImagePath(),
                                 test_utils_.GetDevicePath().AsUTF8Unsafe());

  base::DeleteFile(test_utils_.GetImagePath(), false);

  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId, _, _)).Times(0);
  EXPECT_CALL(manager_, OnComplete(kDummyExtensionId)).Times(0);
  EXPECT_CALL(manager_,
              OnError(kDummyExtensionId,
                      image_writer_api::STAGE_UNKNOWN,
                      0,
                      error::kImageInvalid)).Times(1);

  op->Start();

  base::RunLoop().RunUntilIdle();
}

// Runs the entire WriteFromFile operation.
TEST_F(ImageWriterFromFileTest, WriteFromFileEndToEnd) {
  scoped_refptr<WriteFromFileOperation> op =
      new WriteFromFileOperation(manager_.AsWeakPtr(),
                                 kDummyExtensionId,
                                 test_utils_.GetImagePath(),
                                 test_utils_.GetDevicePath().AsUTF8Unsafe());
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::STAGE_WRITE, _))
      .Times(AnyNumber());
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::STAGE_WRITE, 0))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::STAGE_WRITE, 100))
      .Times(AtLeast(1));

#if !defined(OS_CHROMEOS)
  // Chrome OS doesn't verify.
  EXPECT_CALL(
      manager_,
      OnProgress(kDummyExtensionId, image_writer_api::STAGE_VERIFYWRITE, _))
      .Times(AnyNumber());
  EXPECT_CALL(
      manager_,
      OnProgress(kDummyExtensionId, image_writer_api::STAGE_VERIFYWRITE, 0))
      .Times(AtLeast(1));
  EXPECT_CALL(
      manager_,
      OnProgress(kDummyExtensionId, image_writer_api::STAGE_VERIFYWRITE, 100))
      .Times(AtLeast(1));
#endif

  EXPECT_CALL(manager_, OnComplete(kDummyExtensionId)).Times(1);
  EXPECT_CALL(manager_, OnError(kDummyExtensionId, _, _, _)).Times(0);

  op->Start();

  base::RunLoop().RunUntilIdle();
#if !defined(OS_CHROMEOS)
  test_utils_.GetUtilityClient()->Progress(0);
  test_utils_.GetUtilityClient()->Progress(50);
  test_utils_.GetUtilityClient()->Progress(100);
  test_utils_.GetUtilityClient()->Success();
  base::RunLoop().RunUntilIdle();
  test_utils_.GetUtilityClient()->Progress(0);
  test_utils_.GetUtilityClient()->Progress(50);
  test_utils_.GetUtilityClient()->Progress(100);
  test_utils_.GetUtilityClient()->Success();
  base::RunLoop().RunUntilIdle();
#endif
}

}  // namespace image_writer
}  // namespace extensions
