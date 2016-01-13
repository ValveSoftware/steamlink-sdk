// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/run_loop.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"

namespace content {

class TracingControllerTest : public ContentBrowserTest {
 public:
  TracingControllerTest() {}

  virtual void SetUp() OVERRIDE {
    get_categories_done_callback_count_ = 0;
    enable_recording_done_callback_count_ = 0;
    disable_recording_done_callback_count_ = 0;
    enable_monitoring_done_callback_count_ = 0;
    disable_monitoring_done_callback_count_ = 0;
    capture_monitoring_snapshot_done_callback_count_ = 0;
    ContentBrowserTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    ContentBrowserTest::TearDown();
  }

  void Navigate(Shell* shell) {
    NavigateToURL(shell, GetTestUrl("", "title.html"));
  }

  void GetCategoriesDoneCallbackTest(base::Closure quit_callback,
                                     const std::set<std::string>& categories) {
    get_categories_done_callback_count_++;
    EXPECT_TRUE(categories.size() > 0);
    quit_callback.Run();
  }

  void EnableRecordingDoneCallbackTest(base::Closure quit_callback) {
    enable_recording_done_callback_count_++;
    quit_callback.Run();
  }

  void DisableRecordingDoneCallbackTest(base::Closure quit_callback,
                                        const base::FilePath& file_path) {
    disable_recording_done_callback_count_++;
    EXPECT_TRUE(PathExists(file_path));
    int64 file_size;
    base::GetFileSize(file_path, &file_size);
    EXPECT_TRUE(file_size > 0);
    quit_callback.Run();
    last_actual_recording_file_path_ = file_path;
  }

  void EnableMonitoringDoneCallbackTest(base::Closure quit_callback) {
    enable_monitoring_done_callback_count_++;
    quit_callback.Run();
  }

  void DisableMonitoringDoneCallbackTest(base::Closure quit_callback) {
    disable_monitoring_done_callback_count_++;
    quit_callback.Run();
  }

  void CaptureMonitoringSnapshotDoneCallbackTest(
      base::Closure quit_callback, const base::FilePath& file_path) {
    capture_monitoring_snapshot_done_callback_count_++;
    EXPECT_TRUE(PathExists(file_path));
    int64 file_size;
    base::GetFileSize(file_path, &file_size);
    EXPECT_TRUE(file_size > 0);
    quit_callback.Run();
    last_actual_monitoring_file_path_ = file_path;
  }

  int get_categories_done_callback_count() const {
    return get_categories_done_callback_count_;
  }

  int enable_recording_done_callback_count() const {
    return enable_recording_done_callback_count_;
  }

  int disable_recording_done_callback_count() const {
    return disable_recording_done_callback_count_;
  }

  int enable_monitoring_done_callback_count() const {
    return enable_monitoring_done_callback_count_;
  }

  int disable_monitoring_done_callback_count() const {
    return disable_monitoring_done_callback_count_;
  }

  int capture_monitoring_snapshot_done_callback_count() const {
    return capture_monitoring_snapshot_done_callback_count_;
  }

  base::FilePath last_actual_recording_file_path() const {
    return last_actual_recording_file_path_;
  }

  base::FilePath last_actual_monitoring_file_path() const {
    return last_actual_monitoring_file_path_;
  }

  void TestEnableAndDisableRecording(const base::FilePath& result_file_path) {
    Navigate(shell());

    TracingController* controller = TracingController::GetInstance();

    {
      base::RunLoop run_loop;
      TracingController::EnableRecordingDoneCallback callback =
          base::Bind(&TracingControllerTest::EnableRecordingDoneCallbackTest,
                     base::Unretained(this),
                     run_loop.QuitClosure());
      bool result = controller->EnableRecording(
          "", TracingController::DEFAULT_OPTIONS, callback);
      ASSERT_TRUE(result);
      run_loop.Run();
      EXPECT_EQ(enable_recording_done_callback_count(), 1);
    }

    {
      base::RunLoop run_loop;
      TracingController::TracingFileResultCallback callback =
          base::Bind(&TracingControllerTest::DisableRecordingDoneCallbackTest,
                     base::Unretained(this),
                     run_loop.QuitClosure());
      bool result = controller->DisableRecording(result_file_path, callback);
      ASSERT_TRUE(result);
      run_loop.Run();
      EXPECT_EQ(disable_recording_done_callback_count(), 1);
    }
  }

  void TestEnableCaptureAndDisableMonitoring(
      const base::FilePath& result_file_path) {
    Navigate(shell());

    TracingController* controller = TracingController::GetInstance();

    {
      bool is_monitoring;
      std::string category_filter;
      TracingController::Options options;
      controller->GetMonitoringStatus(&is_monitoring,
                                      &category_filter,
                                      &options);
      EXPECT_FALSE(is_monitoring);
      EXPECT_EQ("-*Debug,-*Test", category_filter);
      EXPECT_FALSE(options & TracingController::ENABLE_SYSTRACE);
      EXPECT_FALSE(options & TracingController::RECORD_CONTINUOUSLY);
      EXPECT_FALSE(options & TracingController::ENABLE_SAMPLING);
    }

    {
      base::RunLoop run_loop;
      TracingController::EnableMonitoringDoneCallback callback =
          base::Bind(&TracingControllerTest::EnableMonitoringDoneCallbackTest,
                     base::Unretained(this),
                     run_loop.QuitClosure());
      bool result = controller->EnableMonitoring(
          "*", TracingController::ENABLE_SAMPLING, callback);
      ASSERT_TRUE(result);
      run_loop.Run();
      EXPECT_EQ(enable_monitoring_done_callback_count(), 1);
    }

    {
      bool is_monitoring;
      std::string category_filter;
      TracingController::Options options;
      controller->GetMonitoringStatus(&is_monitoring,
                                      &category_filter,
                                      &options);
      EXPECT_TRUE(is_monitoring);
      EXPECT_EQ("*", category_filter);
      EXPECT_FALSE(options & TracingController::ENABLE_SYSTRACE);
      EXPECT_FALSE(options & TracingController::RECORD_CONTINUOUSLY);
      EXPECT_TRUE(options & TracingController::ENABLE_SAMPLING);
    }

    {
      base::RunLoop run_loop;
      TracingController::TracingFileResultCallback callback =
          base::Bind(&TracingControllerTest::
                         CaptureMonitoringSnapshotDoneCallbackTest,
                     base::Unretained(this),
                     run_loop.QuitClosure());
      ASSERT_TRUE(controller->CaptureMonitoringSnapshot(result_file_path,
                                                        callback));
      run_loop.Run();
      EXPECT_EQ(capture_monitoring_snapshot_done_callback_count(), 1);
    }

    {
      base::RunLoop run_loop;
      TracingController::DisableMonitoringDoneCallback callback =
          base::Bind(&TracingControllerTest::DisableMonitoringDoneCallbackTest,
                     base::Unretained(this),
                     run_loop.QuitClosure());
      bool result = controller->DisableMonitoring(callback);
      ASSERT_TRUE(result);
      run_loop.Run();
      EXPECT_EQ(disable_monitoring_done_callback_count(), 1);
    }

    {
      bool is_monitoring;
      std::string category_filter;
      TracingController::Options options;
      controller->GetMonitoringStatus(&is_monitoring,
                                      &category_filter,
                                      &options);
      EXPECT_FALSE(is_monitoring);
      EXPECT_EQ("", category_filter);
      EXPECT_FALSE(options & TracingController::ENABLE_SYSTRACE);
      EXPECT_FALSE(options & TracingController::RECORD_CONTINUOUSLY);
      EXPECT_FALSE(options & TracingController::ENABLE_SAMPLING);
    }
  }

 private:
  int get_categories_done_callback_count_;
  int enable_recording_done_callback_count_;
  int disable_recording_done_callback_count_;
  int enable_monitoring_done_callback_count_;
  int disable_monitoring_done_callback_count_;
  int capture_monitoring_snapshot_done_callback_count_;
  base::FilePath last_actual_recording_file_path_;
  base::FilePath last_actual_monitoring_file_path_;
};

IN_PROC_BROWSER_TEST_F(TracingControllerTest, GetCategories) {
  Navigate(shell());

  TracingController* controller = TracingController::GetInstance();

  base::RunLoop run_loop;
  TracingController::GetCategoriesDoneCallback callback =
      base::Bind(&TracingControllerTest::GetCategoriesDoneCallbackTest,
                 base::Unretained(this),
                 run_loop.QuitClosure());
  ASSERT_TRUE(controller->GetCategories(callback));
  run_loop.Run();
  EXPECT_EQ(get_categories_done_callback_count(), 1);
}

IN_PROC_BROWSER_TEST_F(TracingControllerTest, EnableAndDisableRecording) {
  TestEnableAndDisableRecording(base::FilePath());
}

IN_PROC_BROWSER_TEST_F(TracingControllerTest,
                       EnableAndDisableRecordingWithFilePath) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);
  TestEnableAndDisableRecording(file_path);
  EXPECT_EQ(file_path.value(), last_actual_recording_file_path().value());
}

IN_PROC_BROWSER_TEST_F(TracingControllerTest,
                       EnableAndDisableRecordingWithEmptyFileAndNullCallback) {
  Navigate(shell());

  TracingController* controller = TracingController::GetInstance();
  EXPECT_TRUE(controller->EnableRecording(
      "", TracingController::DEFAULT_OPTIONS,
      TracingController::EnableRecordingDoneCallback()));
  EXPECT_TRUE(controller->DisableRecording(
      base::FilePath(), TracingController::TracingFileResultCallback()));
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(TracingControllerTest,
                       EnableCaptureAndDisableMonitoring) {
  TestEnableCaptureAndDisableMonitoring(base::FilePath());
}

IN_PROC_BROWSER_TEST_F(TracingControllerTest,
                       EnableCaptureAndDisableMonitoringWithFilePath) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);
  TestEnableCaptureAndDisableMonitoring(file_path);
  EXPECT_EQ(file_path.value(), last_actual_monitoring_file_path().value());
}

IN_PROC_BROWSER_TEST_F(
    TracingControllerTest,
    EnableCaptureAndDisableMonitoringWithEmptyFileAndNullCallback) {
  Navigate(shell());

  TracingController* controller = TracingController::GetInstance();
  EXPECT_TRUE(controller->EnableMonitoring(
      "*", TracingController::ENABLE_SAMPLING,
      TracingController::EnableMonitoringDoneCallback()));
  controller->CaptureMonitoringSnapshot(
      base::FilePath(), TracingController::TracingFileResultCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(controller->DisableMonitoring(
      TracingController::DisableMonitoringDoneCallback()));
  base::RunLoop().RunUntilIdle();
}

}  // namespace content
