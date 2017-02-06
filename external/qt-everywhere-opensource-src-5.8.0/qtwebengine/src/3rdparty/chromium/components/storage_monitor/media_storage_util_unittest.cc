// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "components/storage_monitor/media_storage_util.h"
#include "components/storage_monitor/removable_device_constants.h"
#include "components/storage_monitor/storage_monitor.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kImageCaptureDeviceId[] = "ic:xyz";

}  // namespace

using content::BrowserThread;

namespace storage_monitor {

class MediaStorageUtilTest : public testing::Test {
 public:
  MediaStorageUtilTest()
      : thread_bundle_(content::TestBrowserThreadBundle::REAL_FILE_THREAD) {}
  ~MediaStorageUtilTest() override {}

  // Verify mounted device type.
  void CheckDCIMDeviceType(const base::FilePath& mount_point) {
    EXPECT_TRUE(MediaStorageUtil::HasDcim(mount_point));
  }

  void CheckNonDCIMDeviceType(const base::FilePath& mount_point) {
    EXPECT_FALSE(MediaStorageUtil::HasDcim(mount_point));
  }

  void ProcessAttach(const std::string& id,
                     const base::FilePath::StringType& location) {
    StorageInfo info(id, location, base::string16(), base::string16(),
                     base::string16(), 0);
    monitor_->receiver()->ProcessAttach(info);
  }

 protected:
  // Create mount point for the test device.
  base::FilePath CreateMountPoint(bool create_dcim_dir) {
    base::FilePath path(scoped_temp_dir_.path());
    if (create_dcim_dir)
      path = path.Append(kDCIMDirectoryName);
    if (!base::CreateDirectory(path))
      return base::FilePath();
    return scoped_temp_dir_.path();
  }

  void SetUp() override {
    monitor_ = TestStorageMonitor::CreateAndInstall();
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  }

  void TearDown() override {
    WaitForFileThread();
    TestStorageMonitor::Destroy();
  }

  static void PostQuitToUIThread() {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::MessageLoop::QuitWhenIdleClosure());
  }

  static void WaitForFileThread() {
    BrowserThread::PostTask(BrowserThread::FILE,
                            FROM_HERE,
                            base::Bind(&PostQuitToUIThread));
    base::RunLoop().Run();
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestStorageMonitor* monitor_;
  base::ScopedTempDir scoped_temp_dir_;
};

// Test to verify that HasDcim() function returns true for the given media
// device mount point.
TEST_F(MediaStorageUtilTest, MediaDeviceAttached) {
  // Create a dummy mount point with DCIM Directory.
  base::FilePath mount_point(CreateMountPoint(true));
  ASSERT_FALSE(mount_point.empty());
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&MediaStorageUtilTest::CheckDCIMDeviceType,
                 base::Unretained(this), mount_point));
  base::RunLoop().RunUntilIdle();
}

// Test to verify that HasDcim() function returns false for a given non-media
// device mount point.
TEST_F(MediaStorageUtilTest, NonMediaDeviceAttached) {
  // Create a dummy mount point without DCIM Directory.
  base::FilePath mount_point(CreateMountPoint(false));
  ASSERT_FALSE(mount_point.empty());
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&MediaStorageUtilTest::CheckNonDCIMDeviceType,
                 base::Unretained(this), mount_point));
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaStorageUtilTest, CanCreateFileSystemForImageCapture) {
  EXPECT_TRUE(MediaStorageUtil::CanCreateFileSystem(kImageCaptureDeviceId,
                                                    base::FilePath()));
  EXPECT_FALSE(MediaStorageUtil::CanCreateFileSystem(
      "dcim:xyz", base::FilePath()));
  EXPECT_FALSE(MediaStorageUtil::CanCreateFileSystem(
      "dcim:xyz", base::FilePath(FILE_PATH_LITERAL("relative"))));
  EXPECT_FALSE(MediaStorageUtil::CanCreateFileSystem(
      "dcim:xyz", base::FilePath(FILE_PATH_LITERAL("../refparent"))));
}

TEST_F(MediaStorageUtilTest, DetectDeviceFiltered) {
  MediaStorageUtil::DeviceIdSet devices;
  devices.insert(kImageCaptureDeviceId);

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::Closure signal_event =
      base::Bind(&base::WaitableEvent::Signal, base::Unretained(&event));

  // We need signal_event to be executed on the FILE thread, as the test thread
  // is blocked. Therefore, we invoke FilterAttachedDevices on the FILE thread.
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&MediaStorageUtil::FilterAttachedDevices,
                                     base::Unretained(&devices), signal_event));
  event.Wait();
  EXPECT_FALSE(devices.find(kImageCaptureDeviceId) != devices.end());

  ProcessAttach(kImageCaptureDeviceId, FILE_PATH_LITERAL("/location"));
  devices.insert(kImageCaptureDeviceId);
  event.Reset();
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&MediaStorageUtil::FilterAttachedDevices,
                                     base::Unretained(&devices), signal_event));
  event.Wait();

  EXPECT_TRUE(devices.find(kImageCaptureDeviceId) != devices.end());
}

}  // namespace storage_monitor
