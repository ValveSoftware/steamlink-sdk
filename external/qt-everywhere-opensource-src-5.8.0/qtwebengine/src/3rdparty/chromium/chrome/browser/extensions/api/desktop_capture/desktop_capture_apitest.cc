// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/desktop_capture/desktop_capture_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/media/fake_desktop_media_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace extensions {

namespace {

struct TestFlags {
  bool expect_screens;
  bool expect_windows;
  bool expect_tabs;
  bool expect_audio;
  content::DesktopMediaID selected_source;
  bool cancelled;

  // Following flags are set by FakeDesktopMediaPicker when it's created and
  // deleted.
  bool picker_created;
  bool picker_deleted;
};

class FakeDesktopMediaPicker : public DesktopMediaPicker {
 public:
  explicit FakeDesktopMediaPicker(TestFlags* expectation)
      : expectation_(expectation),
        weak_factory_(this) {
    expectation_->picker_created = true;
  }
  ~FakeDesktopMediaPicker() override { expectation_->picker_deleted = true; }

  // DesktopMediaPicker interface.
  void Show(content::WebContents* web_contents,
            gfx::NativeWindow context,
            gfx::NativeWindow parent,
            const base::string16& app_name,
            const base::string16& target_name,
            std::unique_ptr<DesktopMediaList> screen_list,
            std::unique_ptr<DesktopMediaList> window_list,
            std::unique_ptr<DesktopMediaList> tab_list,
            bool request_audio,
            const DoneCallback& done_callback) override {
    if (!expectation_->cancelled) {
      // Post a task to call the callback asynchronously.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&FakeDesktopMediaPicker::CallCallback,
                     weak_factory_.GetWeakPtr(), done_callback));
    } else {
      // If we expect the dialog to be cancelled then store the callback to
      // retain reference to the callback handler.
      done_callback_ = done_callback;
    }
  }

 private:
  void CallCallback(DoneCallback done_callback) {
    done_callback.Run(expectation_->selected_source);
  }

  TestFlags* expectation_;
  DoneCallback done_callback_;

  base::WeakPtrFactory<FakeDesktopMediaPicker> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeDesktopMediaPicker);
};

class FakeDesktopMediaPickerFactory :
    public DesktopCaptureChooseDesktopMediaFunction::PickerFactory {
 public:
  FakeDesktopMediaPickerFactory() {}
  ~FakeDesktopMediaPickerFactory() override {}

  void SetTestFlags(TestFlags* test_flags, int tests_count) {
    test_flags_ = test_flags;
    tests_count_ = tests_count;
    current_test_ = 0;
  }

  // DesktopCaptureChooseDesktopMediaFunction::PickerFactory interface.
  MediaListArray CreateModel(
      bool show_screens,
      bool show_windows,
      bool show_tabs,
      bool show_audio) override {
    EXPECT_LE(current_test_, tests_count_);
    MediaListArray media_lists;
    if (current_test_ >= tests_count_) {
      return media_lists;
    }
    EXPECT_EQ(test_flags_[current_test_].expect_screens, show_screens);
    EXPECT_EQ(test_flags_[current_test_].expect_windows, show_windows);
    EXPECT_EQ(test_flags_[current_test_].expect_tabs, show_tabs);
    EXPECT_EQ(test_flags_[current_test_].expect_audio, show_audio);

    media_lists[0] = std::unique_ptr<DesktopMediaList>(
        show_screens ? new FakeDesktopMediaList() : nullptr);
    media_lists[1] = std::unique_ptr<DesktopMediaList>(
        show_windows ? new FakeDesktopMediaList() : nullptr);
    media_lists[2] = std::unique_ptr<DesktopMediaList>(
        show_tabs ? new FakeDesktopMediaList() : nullptr);
    return media_lists;
  }

  std::unique_ptr<DesktopMediaPicker> CreatePicker() override {
    EXPECT_LE(current_test_, tests_count_);
    if (current_test_ >= tests_count_)
      return std::unique_ptr<DesktopMediaPicker>();
    ++current_test_;
    return std::unique_ptr<DesktopMediaPicker>(
        new FakeDesktopMediaPicker(test_flags_ + current_test_ - 1));
  }

 private:
  TestFlags* test_flags_;
  int tests_count_;
  int current_test_;

  DISALLOW_COPY_AND_ASSIGN(FakeDesktopMediaPickerFactory);
};

class DesktopCaptureApiTest : public ExtensionApiTest {
 public:
  DesktopCaptureApiTest() {
    DesktopCaptureChooseDesktopMediaFunction::
        SetPickerFactoryForTests(&picker_factory_);
  }
  ~DesktopCaptureApiTest() override {
    DesktopCaptureChooseDesktopMediaFunction::
        SetPickerFactoryForTests(NULL);
  }

 protected:
  GURL GetURLForPath(const std::string& host, const std::string& path) {
    std::string port = base::UintToString(embedded_test_server()->port());
    GURL::Replacements replacements;
    replacements.SetHostStr(host);
    replacements.SetPortStr(port);
    return embedded_test_server()->GetURL(path).ReplaceComponents(replacements);
  }

  FakeDesktopMediaPickerFactory picker_factory_;
};

}  // namespace

// Flaky on Windows: http://crbug.com/301887
#if defined(OS_WIN)
#define MAYBE_ChooseDesktopMedia DISABLED_ChooseDesktopMedia
#else
#define MAYBE_ChooseDesktopMedia ChooseDesktopMedia
#endif
IN_PROC_BROWSER_TEST_F(DesktopCaptureApiTest, MAYBE_ChooseDesktopMedia) {
  // For tabshare, we need to turn on the flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      extensions::switches::kEnableTabForDesktopShare);

  // Each element in the following array corresponds to one test in
  // chrome/test/data/extensions/api_test/desktop_capture/test.js .
  TestFlags test_flags[] = {
      // pickerUiCanceled()
      {true, true, false, false, content::DesktopMediaID()},
      // chooseMedia()
      {true, true, false, false,
       content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                               content::DesktopMediaID::kNullId)},
      // screensOnly()
      {true, false, false, false, content::DesktopMediaID()},
      // WindowsOnly()
      {false, true, false, false, content::DesktopMediaID()},
      // tabOnly()
      {false, false, true, false, content::DesktopMediaID()},
      // audioShare()
      {true, true, true, true, content::DesktopMediaID()},
      // chooseMediaAndGetStream()
      {true, true, false, false,
       content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                               webrtc::kFullDesktopScreenId)},
      // chooseMediaAndTryGetStreamWithInvalidId()
      {true, true, false, false,
       content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                               webrtc::kFullDesktopScreenId)},
      // cancelDialog()
      {true, true, false, false, content::DesktopMediaID(), true},
      // tabShareWithAudioGetStream()
      {false, false, true, true,
       content::DesktopMediaID(content::DesktopMediaID::TYPE_WEB_CONTENTS, 0,
                               true)},
      // windowShareWithAudioGetStream()
      {false, true, false, true,
       content::DesktopMediaID(content::DesktopMediaID::TYPE_WINDOW, 0, true)},
      // screenShareWithAudioGetStream()
      {true, false, false, true,
       content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                               webrtc::kFullDesktopScreenId, true)},
      // tabShareWithoutAudioGetStream()
      {false, false, true, true,
       content::DesktopMediaID(content::DesktopMediaID::TYPE_WEB_CONTENTS, 0)},
      // windowShareWithoutAudioGetStream()
      {false, true, false, true,
       content::DesktopMediaID(content::DesktopMediaID::TYPE_WINDOW, 0)},
      // screenShareWithoutAudioGetStream()
      {true, false, false, true,
       content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                               webrtc::kFullDesktopScreenId)},
  };
  picker_factory_.SetTestFlags(test_flags, arraysize(test_flags));
  ASSERT_TRUE(RunExtensionTest("desktop_capture")) << message_;
}

// Test is flaky http://crbug.com/301887.
IN_PROC_BROWSER_TEST_F(DesktopCaptureApiTest, DISABLED_Delegation) {
  // Initialize test server.
  base::FilePath test_data;
  EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data));
  embedded_test_server()->ServeFilesFromDirectory(test_data.AppendASCII(
      "extensions/api_test/desktop_capture_delegate"));
  ASSERT_TRUE(embedded_test_server()->Start());
  host_resolver()->AddRule("*", embedded_test_server()->base_url().host());

  // Load extension.
  base::FilePath extension_path =
      test_data_dir_.AppendASCII("desktop_capture_delegate");
  const Extension* extension = LoadExtensionWithFlags(
      extension_path, ExtensionBrowserTest::kFlagNone);
  ASSERT_TRUE(extension);

  ui_test_utils::NavigateToURL(
      browser(), GetURLForPath("example.com", "/example.com.html"));

  TestFlags test_flags[] = {
      {true, true, false, false,
       content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                               content::DesktopMediaID::kNullId)},
      {true, true, false, false,
       content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                               content::DesktopMediaID::kNullId)},
      {true, true, false, false,
       content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                               content::DesktopMediaID::kNullId),
       true},
  };
  picker_factory_.SetTestFlags(test_flags, arraysize(test_flags));

  bool result;

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents, "getStream()", &result));
  EXPECT_TRUE(result);

  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents, "getStreamWithInvalidId()", &result));
  EXPECT_TRUE(result);

  // Verify that the picker is closed once the tab is closed.
  content::WebContentsDestroyedWatcher destroyed_watcher(web_contents);
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents, "openPickerDialogAndReturn()", &result));
  EXPECT_TRUE(result);
  EXPECT_TRUE(test_flags[2].picker_created);
  EXPECT_FALSE(test_flags[2].picker_deleted);

  web_contents->Close();
  destroyed_watcher.Wait();
  EXPECT_TRUE(test_flags[2].picker_deleted);
}

}  // namespace extensions
