// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"

#include <string.h>
#include <utility>

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_image_burner_client.h"
#endif

namespace extensions {
namespace image_writer {

#if defined(OS_CHROMEOS)
namespace {

class ImageWriterFakeImageBurnerClient
    : public chromeos::FakeImageBurnerClient {
 public:
  ImageWriterFakeImageBurnerClient() {}
  ~ImageWriterFakeImageBurnerClient() override {}

  void SetEventHandlers(
      const BurnFinishedHandler& burn_finished_handler,
      const BurnProgressUpdateHandler& burn_progress_update_handler) override {
    burn_finished_handler_ = burn_finished_handler;
    burn_progress_update_handler_ = burn_progress_update_handler;
  }

  void BurnImage(const std::string& from_path,
                 const std::string& to_path,
                 const ErrorCallback& error_callback) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(burn_progress_update_handler_, to_path, 0, 100));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(burn_progress_update_handler_, to_path, 50, 100));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(burn_progress_update_handler_, to_path, 100, 100));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(burn_finished_handler_, to_path, true, ""));
  }

 private:
  BurnFinishedHandler burn_finished_handler_;
  BurnProgressUpdateHandler burn_progress_update_handler_;
};

} // namespace
#endif

MockOperationManager::MockOperationManager() : OperationManager(NULL) {}
MockOperationManager::MockOperationManager(content::BrowserContext* context)
    : OperationManager(context) {}
MockOperationManager::~MockOperationManager() {}

#if defined(OS_CHROMEOS)
FakeDiskMountManager::FakeDiskMountManager() {}
FakeDiskMountManager::~FakeDiskMountManager() {}

void FakeDiskMountManager::UnmountDeviceRecursively(
    const std::string& device_path,
    const UnmountDeviceRecursivelyCallbackType& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback, true));
}
#endif

FakeImageWriterClient::FakeImageWriterClient() {}
FakeImageWriterClient::~FakeImageWriterClient() {}

void FakeImageWriterClient::Write(const ProgressCallback& progress_callback,
                                  const SuccessCallback& success_callback,
                                  const ErrorCallback& error_callback,
                                  const base::FilePath& source,
                                  const base::FilePath& target) {
  progress_callback_ = progress_callback;
  success_callback_ = success_callback;
  error_callback_ = error_callback;

  if (!write_callback_.is_null())
    write_callback_.Run();
}

void FakeImageWriterClient::Verify(const ProgressCallback& progress_callback,
                                   const SuccessCallback& success_callback,
                                   const ErrorCallback& error_callback,
                                   const base::FilePath& source,
                                   const base::FilePath& target) {
  progress_callback_ = progress_callback;
  success_callback_ = success_callback;
  error_callback_ = error_callback;

  if (!verify_callback_.is_null())
    verify_callback_.Run();
}

void FakeImageWriterClient::Cancel(const CancelCallback& cancel_callback) {
  cancel_callback_ = cancel_callback;
}

void FakeImageWriterClient::Shutdown() {
  // Clear handlers to not hold any reference to the caller.
  success_callback_.Reset();
  progress_callback_.Reset();
  error_callback_.Reset();
  cancel_callback_.Reset();

  write_callback_.Reset();
  verify_callback_.Reset();
}

void FakeImageWriterClient::SetWriteCallback(
    const base::Closure& write_callback) {
  write_callback_ = write_callback;
}

void FakeImageWriterClient::SetVerifyCallback(
    const base::Closure& verify_callback) {
  verify_callback_ = verify_callback;
}

void FakeImageWriterClient::Progress(int64_t progress) {
  if (!progress_callback_.is_null())
    progress_callback_.Run(progress);
}

void FakeImageWriterClient::Success() {
  if (!success_callback_.is_null())
    success_callback_.Run();
}

void FakeImageWriterClient::Error(const std::string& message) {
  if (!error_callback_.is_null())
    error_callback_.Run(message);
}

void FakeImageWriterClient::Cancel() {
  if (!cancel_callback_.is_null())
    cancel_callback_.Run();
}

ImageWriterTestUtils::ImageWriterTestUtils() {
}
ImageWriterTestUtils::~ImageWriterTestUtils() {
}

void ImageWriterTestUtils::SetUp() {
  SetUp(false);
}

void ImageWriterTestUtils::SetUp(bool is_browser_test) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &test_image_path_));
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &test_device_path_));

  ASSERT_TRUE(FillFile(test_image_path_, kImagePattern, kTestFileSize));
  ASSERT_TRUE(FillFile(test_device_path_, kDevicePattern, kTestFileSize));

#if defined(OS_CHROMEOS)
  if (!chromeos::DBusThreadManager::IsInitialized()) {
    std::unique_ptr<chromeos::DBusThreadManagerSetter> dbus_setter =
        chromeos::DBusThreadManager::GetSetterForTesting();
    std::unique_ptr<chromeos::ImageBurnerClient> image_burner_fake(
        new ImageWriterFakeImageBurnerClient());
    dbus_setter->SetImageBurnerClient(std::move(image_burner_fake));
  }

  FakeDiskMountManager* disk_manager = new FakeDiskMountManager();
  chromeos::disks::DiskMountManager::InitializeForTesting(disk_manager);

  // Adds a disk entry for test_device_path_ with the same device and file path.
  disk_manager->CreateDiskEntryForMountDevice(
      chromeos::disks::DiskMountManager::MountPointInfo(
          test_device_path_.value(),
          "/dummy/mount",
          chromeos::MOUNT_TYPE_DEVICE,
          chromeos::disks::MOUNT_CONDITION_NONE),
      "device_id",
      "device_label",
      "Vendor",
      "Product",
      chromeos::DEVICE_TYPE_USB,
      kTestFileSize,
      true,
      true,
      true,
      false);
  disk_manager->SetupDefaultReplies();
#else
  client_ = new FakeImageWriterClient();
  image_writer::Operation::SetUtilityClientForTesting(client_);
#endif
}

void ImageWriterTestUtils::TearDown() {
#if defined(OS_CHROMEOS)
  if (chromeos::DBusThreadManager::IsInitialized()) {
    chromeos::DBusThreadManager::Shutdown();
  }
  chromeos::disks::DiskMountManager::Shutdown();
#else
  image_writer::Operation::SetUtilityClientForTesting(NULL);
  client_->Shutdown();
#endif
}

const base::FilePath& ImageWriterTestUtils::GetTempDir() {
  return temp_dir_.GetPath();
}

const base::FilePath& ImageWriterTestUtils::GetImagePath() {
  return test_image_path_;
}

const base::FilePath& ImageWriterTestUtils::GetDevicePath() {
  return test_device_path_;
}

#if !defined(OS_CHROMEOS)
FakeImageWriterClient* ImageWriterTestUtils::GetUtilityClient() {
  return client_.get();
}
#endif

bool ImageWriterTestUtils::ImageWrittenToDevice() {
  std::unique_ptr<char[]> image_buffer(new char[kTestFileSize]);
  std::unique_ptr<char[]> device_buffer(new char[kTestFileSize]);

  int image_bytes_read =
      ReadFile(test_image_path_, image_buffer.get(), kTestFileSize);

  if (image_bytes_read < 0)
    return false;

  int device_bytes_read =
      ReadFile(test_device_path_, device_buffer.get(), kTestFileSize);

  if (image_bytes_read != device_bytes_read)
    return false;

  return memcmp(image_buffer.get(), device_buffer.get(), image_bytes_read) == 0;
}

bool ImageWriterTestUtils::FillFile(const base::FilePath& file,
                                    const int pattern,
                                    const int length) {
  std::unique_ptr<char[]> buffer(new char[length]);
  memset(buffer.get(), pattern, length);

  return base::WriteFile(file, buffer.get(), length) == length;
}

ImageWriterUnitTestBase::ImageWriterUnitTestBase()
    : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {
}
ImageWriterUnitTestBase::~ImageWriterUnitTestBase() {
}

void ImageWriterUnitTestBase::SetUp() {
  testing::Test::SetUp();
  test_utils_.SetUp();
}

void ImageWriterUnitTestBase::TearDown() {
  testing::Test::TearDown();
  test_utils_.TearDown();
}

}  // namespace image_writer
}  // namespace extensions
