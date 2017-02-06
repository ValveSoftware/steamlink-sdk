// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/auto_reset.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/media_galleries/media_galleries_api.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/media_galleries/media_galleries_test_util.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/chrome_paths.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/test/result_catcher.h"
#include "media/base/test_data_util.h"

#if defined(OS_WIN) || defined(OS_MACOSX)
#include "chrome/browser/media_galleries/fileapi/picasa_finder.h"
#include "chrome/common/media_galleries/picasa_test_util.h"
#include "chrome/common/media_galleries/picasa_types.h"
#include "chrome/common/media_galleries/pmp_test_util.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/media_galleries/fileapi/iapps_finder_impl.h"
#endif  // OS_MACOSX

#if !defined(DISABLE_NACL)
#include "base/command_line.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "ppapi/shared_impl/ppapi_switches.h"
#endif

using extensions::PlatformAppBrowserTest;
using storage_monitor::StorageInfo;
using storage_monitor::StorageMonitor;

namespace {

// Dummy device properties.
const char kDeviceId[] = "testDeviceId";
const char kDeviceName[] = "foobar";
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
base::FilePath::CharType kDevicePath[] = FILE_PATH_LITERAL("C:\\qux");
#else
base::FilePath::CharType kDevicePath[] = FILE_PATH_LITERAL("/qux");
#endif

}  // namespace

class MediaGalleriesPlatformAppBrowserTest : public PlatformAppBrowserTest {
 protected:
  MediaGalleriesPlatformAppBrowserTest() : test_jpg_size_(0) {}
  ~MediaGalleriesPlatformAppBrowserTest() override {}

  void SetUpOnMainThread() override {
    PlatformAppBrowserTest::SetUpOnMainThread();
    ensure_media_directories_exists_.reset(new EnsureMediaDirectoriesExists);

    int64_t file_size;
    ASSERT_TRUE(base::GetFileSize(GetCommonDataDir().AppendASCII("test.jpg"),
                                  &file_size));
    test_jpg_size_ = base::checked_cast<int>(file_size);
  }

  void TearDownOnMainThread() override {
    ensure_media_directories_exists_.reset();
    PlatformAppBrowserTest::TearDownOnMainThread();
  }

  bool RunMediaGalleriesTest(const std::string& extension_name) {
    base::ListValue empty_list_value;
    return RunMediaGalleriesTestWithArg(extension_name, empty_list_value);
  }

  bool RunMediaGalleriesTestWithArg(const std::string& extension_name,
                                    const base::ListValue& custom_arg_value) {
    // Copy the test data for this test into a temporary directory. Then add
    // a common_injected.js to the temporary copy and run it.
    const char kTestDir[] = "api_test/media_galleries/";
    base::FilePath from_dir =
        test_data_dir_.AppendASCII(kTestDir + extension_name);
    from_dir = from_dir.NormalizePathSeparators();

    base::ScopedTempDir temp_dir;
    if (!temp_dir.CreateUniqueTempDir())
      return false;

    if (!base::CopyDirectory(from_dir, temp_dir.path(), true))
      return false;

    base::FilePath common_js_path(
        GetCommonDataDir().AppendASCII("common_injected.js"));
    base::FilePath inject_js_path(
        temp_dir.path().AppendASCII(extension_name)
                       .AppendASCII("common_injected.js"));
    if (!base::CopyFile(common_js_path, inject_js_path))
      return false;

    const char* custom_arg = NULL;
    std::string json_string;
    if (!custom_arg_value.empty()) {
      base::JSONWriter::Write(custom_arg_value, &json_string);
      custom_arg = json_string.c_str();
    }

    base::AutoReset<base::FilePath> reset(&test_data_dir_, temp_dir.path());
    bool result = RunPlatformAppTestWithArg(extension_name, custom_arg);
    content::RunAllPendingInMessageLoop();  // avoid race on exit in registry.
    return result;
  }

  void AttachFakeDevice() {
    device_id_ = StorageInfo::MakeDeviceId(
        StorageInfo::REMOVABLE_MASS_STORAGE_WITH_DCIM, kDeviceId);

    StorageMonitor::GetInstance()->receiver()->ProcessAttach(
        StorageInfo(device_id_, kDevicePath, base::ASCIIToUTF16(kDeviceName),
                    base::string16(), base::string16(), 0));
    content::RunAllPendingInMessageLoop();
  }

  void DetachFakeDevice() {
    StorageMonitor::GetInstance()->receiver()->ProcessDetach(device_id_);
    content::RunAllPendingInMessageLoop();
  }

  // Called if test only wants a single gallery it creates.
  void RemoveAllGalleries() {
    MediaGalleriesPreferences* preferences = GetAndInitializePreferences();

    // Make a copy, as the iterator would be invalidated otherwise.
    const MediaGalleriesPrefInfoMap galleries =
        preferences->known_galleries();
    for (MediaGalleriesPrefInfoMap::const_iterator it = galleries.begin();
         it != galleries.end(); ++it) {
      preferences->ForgetGalleryById(it->first);
    }
  }

  // This function makes a single fake gallery. This is needed to test platforms
  // with no default media galleries, such as CHROMEOS. This fake gallery is
  // pre-populated with a test.jpg and test.txt.
  void MakeSingleFakeGallery(MediaGalleryPrefId* pref_id) {
    ASSERT_FALSE(fake_gallery_temp_dir_.IsValid());
    ASSERT_TRUE(fake_gallery_temp_dir_.CreateUniqueTempDir());

    MediaGalleriesPreferences* preferences = GetAndInitializePreferences();

    MediaGalleryPrefInfo gallery_info;
    ASSERT_FALSE(preferences->LookUpGalleryByPath(fake_gallery_temp_dir_.path(),
                                                  &gallery_info));
    MediaGalleryPrefId id = preferences->AddGallery(
        gallery_info.device_id,
        gallery_info.path,
        MediaGalleryPrefInfo::kAutoDetected,
        gallery_info.volume_label,
        gallery_info.vendor_name,
        gallery_info.model_name,
        gallery_info.total_size_in_bytes,
        gallery_info.last_attach_time,
        0, 0, 0);
    if (pref_id)
      *pref_id = id;

    content::RunAllPendingInMessageLoop();

    // Valid file, should show up in JS as a FileEntry.
    AddFileToSingleFakeGallery(GetCommonDataDir().AppendASCII("test.jpg"));

    // Invalid file, should not show up as a FileEntry in JS at all.
    AddFileToSingleFakeGallery(GetCommonDataDir().AppendASCII("test.txt"));
  }

  void AddFileToSingleFakeGallery(const base::FilePath& source_path) {
    ASSERT_TRUE(fake_gallery_temp_dir_.IsValid());

    ASSERT_TRUE(base::CopyFile(
        source_path,
        fake_gallery_temp_dir_.path().Append(source_path.BaseName())));
  }

#if defined(OS_WIN) || defined(OS_MACOSX)
  void PopulatePicasaTestData(const base::FilePath& picasa_app_data_root) {
    base::FilePath picasa_database_path =
        picasa::MakePicasaDatabasePath(picasa_app_data_root);
    base::FilePath picasa_temp_dir_path =
        picasa_database_path.DirName().AppendASCII(picasa::kPicasaTempDirName);
    ASSERT_TRUE(base::CreateDirectory(picasa_database_path));
    ASSERT_TRUE(base::CreateDirectory(picasa_temp_dir_path));

    // Create fake folder directories.
    base::FilePath folders_root =
        ensure_media_directories_exists_->GetFakePicasaFoldersRootPath();
    base::FilePath fake_folder_1 = folders_root.AppendASCII("folder1");
    base::FilePath fake_folder_2 = folders_root.AppendASCII("folder2");
    ASSERT_TRUE(base::CreateDirectory(fake_folder_1));
    ASSERT_TRUE(base::CreateDirectory(fake_folder_2));

    // Write folder and album contents.
    picasa::WriteTestAlbumTable(
        picasa_database_path, fake_folder_1, fake_folder_2);
    picasa::WriteTestAlbumsImagesIndex(fake_folder_1, fake_folder_2);

    base::FilePath test_jpg_path = GetCommonDataDir().AppendASCII("test.jpg");
    ASSERT_TRUE(base::CopyFile(
        test_jpg_path, fake_folder_1.AppendASCII("InBoth.jpg")));
    ASSERT_TRUE(base::CopyFile(
        test_jpg_path, fake_folder_1.AppendASCII("InSecondAlbumOnly.jpg")));
    ASSERT_TRUE(base::CopyFile(
        test_jpg_path, fake_folder_2.AppendASCII("InFirstAlbumOnly.jpg")));
  }
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

  base::FilePath GetCommonDataDir() const {
    return test_data_dir_.AppendASCII("api_test")
                         .AppendASCII("media_galleries")
                         .AppendASCII("common");
  }

  int num_galleries() const {
    return ensure_media_directories_exists_->num_galleries();
  }

  int test_jpg_size() const { return test_jpg_size_; }

  EnsureMediaDirectoriesExists* ensure_media_directories_exists() const {
    return ensure_media_directories_exists_.get();
  }

 private:
  MediaGalleriesPreferences* GetAndInitializePreferences() {
    MediaGalleriesPreferences* preferences =
        g_browser_process->media_file_system_registry()->GetPreferences(
            browser()->profile());
    base::RunLoop runloop;
    preferences->EnsureInitialized(runloop.QuitClosure());
    runloop.Run();
    return preferences;
  }

  std::string device_id_;
  base::ScopedTempDir fake_gallery_temp_dir_;
  int test_jpg_size_;
  std::unique_ptr<EnsureMediaDirectoriesExists>
      ensure_media_directories_exists_;
};

#if !defined(DISABLE_NACL)
class MediaGalleriesPlatformAppPpapiTest
    : public MediaGalleriesPlatformAppBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MediaGalleriesPlatformAppBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnablePepperTesting);
  }

  void SetUpOnMainThread() override {
    MediaGalleriesPlatformAppBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(PathService::Get(chrome::DIR_GEN_TEST_DATA, &app_dir_));
    app_dir_ = app_dir_.AppendASCII("ppapi")
                       .AppendASCII("tests")
                       .AppendASCII("extensions")
                       .AppendASCII("media_galleries")
                       .AppendASCII("newlib");
  }

  const base::FilePath& app_dir() const {
    return app_dir_;
  }

 private:
  base::FilePath app_dir_;
};

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppPpapiTest, SendFilesystem) {
  RemoveAllGalleries();
  MakeSingleFakeGallery(NULL);

  const extensions::Extension* extension = LoadExtension(app_dir());
  ASSERT_TRUE(extension);

  extensions::ResultCatcher catcher;
  AppLaunchParams params(browser()->profile(), extension,
                         extensions::LAUNCH_CONTAINER_NONE, NEW_WINDOW,
                         extensions::SOURCE_TEST);
  params.command_line = *base::CommandLine::ForCurrentProcess();
  OpenApplication(params);

  bool result = true;
  if (!catcher.GetNextResult()) {
    message_ = catcher.message();
    result = false;
  }
  content::RunAllPendingInMessageLoop();  // avoid race on exit in registry.
  ASSERT_TRUE(result) << message_;
}

#endif  // !defined(DISABLE_NACL)

// Test is flaky, it fails on certain bots, namely WinXP Tests(1) and Linux
// (dbg)(1)(32).  See crbug.com/354425.
#if defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_MediaGalleriesNoAccess DISABLED_MediaGalleriesNoAccess
#else
#define MAYBE_MediaGalleriesNoAccess MediaGalleriesNoAccess
#endif
IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MAYBE_MediaGalleriesNoAccess) {
  MakeSingleFakeGallery(NULL);

  base::ListValue custom_args;
  custom_args.AppendInteger(num_galleries() + 1);

  ASSERT_TRUE(RunMediaGalleriesTestWithArg("no_access", custom_args))
      << message_;
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest, NoGalleriesRead) {
  ASSERT_TRUE(RunMediaGalleriesTest("no_galleries")) << message_;
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       NoGalleriesCopyTo) {
  ASSERT_TRUE(RunMediaGalleriesTest("no_galleries_copy_to")) << message_;
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MediaGalleriesRead) {
  RemoveAllGalleries();
  MakeSingleFakeGallery(NULL);
  base::ListValue custom_args;
  custom_args.AppendInteger(test_jpg_size());

  ASSERT_TRUE(RunMediaGalleriesTestWithArg("read_access", custom_args))
      << message_;
}

// Test is flaky, it fails on certain bots, namely WinXP Tests(1) and Linux
// (dbg)(1)(32).  See crbug.com/354425.
#if defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_MediaGalleriesCopyTo DISABLED_MediaGalleriesCopyTo
#else
#define MAYBE_MediaGalleriesCopyTo MediaGalleriesCopyTo
#endif
IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MAYBE_MediaGalleriesCopyTo) {
  RemoveAllGalleries();
  MakeSingleFakeGallery(NULL);
  ASSERT_TRUE(RunMediaGalleriesTest("copy_to_access")) << message_;
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MediaGalleriesDelete) {
  MakeSingleFakeGallery(NULL);
  base::ListValue custom_args;
  custom_args.AppendInteger(num_galleries() + 1);
  ASSERT_TRUE(RunMediaGalleriesTestWithArg("delete_access", custom_args))
      << message_;
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MediaGalleriesAccessAttached) {
  AttachFakeDevice();

  base::ListValue custom_args;
  custom_args.AppendInteger(num_galleries() + 1);
  custom_args.AppendString(kDeviceName);

  ASSERT_TRUE(RunMediaGalleriesTestWithArg("access_attached", custom_args))
      << message_;

  DetachFakeDevice();
}

// These two tests are flaky, they time out frequently on Win7 bots. See
// crbug.com/567212.
#if defined(OS_WIN)
#define MAYBE_PicasaDefaultLocation DISABLED_PicasaDefaultLocation
#define MAYBE_PicasaCustomLocation DISABLED_PicasaCustomLocation
#else
#define MAYBE_PicasaDefaultLocation PicasaDefaultLocation
#define MAYBE_PicasaCustomLocation PicasaCustomLocation
#endif
#if defined(OS_WIN)|| defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MAYBE_PicasaDefaultLocation) {
#if defined(OS_WIN)
  PopulatePicasaTestData(
      ensure_media_directories_exists()->GetFakeLocalAppDataPath());
#elif defined(OS_MACOSX)
  PopulatePicasaTestData(
      ensure_media_directories_exists()->GetFakeAppDataPath());
#endif

  base::ListValue custom_args;
  custom_args.AppendInteger(test_jpg_size());
  ASSERT_TRUE(RunMediaGalleriesTestWithArg("picasa", custom_args)) << message_;
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest,
                       MAYBE_PicasaCustomLocation) {
  base::ScopedTempDir custom_picasa_app_data_root;
  ASSERT_TRUE(custom_picasa_app_data_root.CreateUniqueTempDir());
  ensure_media_directories_exists()->SetCustomPicasaAppDataPath(
      custom_picasa_app_data_root.path());
  PopulatePicasaTestData(custom_picasa_app_data_root.path());

  base::ListValue custom_args;
  custom_args.AppendInteger(test_jpg_size());
  ASSERT_TRUE(RunMediaGalleriesTestWithArg("picasa", custom_args)) << message_;
}
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest, ToURL) {
  RemoveAllGalleries();
  MediaGalleryPrefId pref_id;
  MakeSingleFakeGallery(&pref_id);

  base::ListValue custom_args;
  custom_args.AppendInteger(base::checked_cast<int>(pref_id));
  custom_args.AppendString(
      browser()->profile()->GetPath().BaseName().MaybeAsASCII());

  ASSERT_TRUE(RunMediaGalleriesTestWithArg("tourl", custom_args)) << message_;
}

IN_PROC_BROWSER_TEST_F(MediaGalleriesPlatformAppBrowserTest, GetMetadata) {
  RemoveAllGalleries();
  MakeSingleFakeGallery(NULL);

  AddFileToSingleFakeGallery(media::GetTestDataFilePath("90rotation.mp4"));
  AddFileToSingleFakeGallery(media::GetTestDataFilePath("id3_png_test.mp3"));

  base::ListValue custom_args;
#if defined(USE_PROPRIETARY_CODECS)
  custom_args.AppendBoolean(true);
#else
  custom_args.AppendBoolean(false);
#endif
  ASSERT_TRUE(RunMediaGalleriesTestWithArg("media_metadata", custom_args))
      << message_;
}
