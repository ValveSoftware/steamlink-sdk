// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle_android.h"

#include "base/android/apk_assets.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "jni/ResourceBundle_jni.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/data_pack.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

namespace ui {

namespace {

bool g_locale_paks_in_apk = false;
// It is okay to cache and share these file descriptors since the
// ResourceBundle singleton never closes the handles.
int g_chrome_100_percent_fd = -1;
int g_resources_pack_fd = -1;
int g_locale_pack_fd = -1;
base::MemoryMappedFile::Region g_chrome_100_percent_region;
base::MemoryMappedFile::Region g_resources_pack_region;
base::MemoryMappedFile::Region g_locale_pack_region;

bool LoadFromApkOrFile(const char* apk_path,
                       const base::FilePath* disk_path,
                       int* fd_out,
                       base::MemoryMappedFile::Region* region_out) {
  DCHECK_EQ(*fd_out, -1) << "Attempt to load " << apk_path << " twice.";
  if (apk_path != nullptr) {
    *fd_out = base::android::OpenApkAsset(apk_path, region_out);
  }
  // For unit tests, the file exists on disk.
  if (*fd_out < 0 && disk_path != nullptr) {
    int flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
    *fd_out = base::File(*disk_path, flags).TakePlatformFile();
    *region_out = base::MemoryMappedFile::Region::kWholeFile;
  }
  bool success = *fd_out >= 0;
  if (!success) {
    LOG(ERROR) << "Failed to open pak file: " << apk_path;
  }
  return success;
}

}  // namespace

void ResourceBundle::LoadCommonResources() {
  base::FilePath disk_path;
  PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &disk_path);
  disk_path = disk_path.AppendASCII("chrome_100_percent.pak");
  if (LoadFromApkOrFile("assets/chrome_100_percent.pak",
                        &disk_path,
                        &g_chrome_100_percent_fd,
                        &g_chrome_100_percent_region)) {
    AddDataPackFromFileRegion(base::File(g_chrome_100_percent_fd),
                              g_chrome_100_percent_region, SCALE_FACTOR_100P);
  }
}

bool ResourceBundle::LocaleDataPakExists(const std::string& locale) {
  if (g_locale_paks_in_apk) {
    return !GetPathForAndroidLocalePakWithinApk(locale).empty();
  }
  return !GetLocaleFilePath(locale, true).empty();
}

std::string ResourceBundle::LoadLocaleResources(
    const std::string& pref_locale) {
  DCHECK(!locale_resources_data_.get()) << "locale.pak already loaded";
  if (g_locale_pack_fd != -1) {
    LOG(WARNING)
        << "Unexpected (outside of tests): Loading a second locale pak file.";
  }
  std::string app_locale = l10n_util::GetApplicationLocale(pref_locale);
  if (g_locale_paks_in_apk) {
    std::string locale_path_within_apk =
        GetPathForAndroidLocalePakWithinApk(app_locale);
    if (locale_path_within_apk.empty()) {
      LOG(WARNING) << "locale_path_within_apk.empty() for locale "
                   << app_locale;
      return std::string();
    }
    g_locale_pack_fd = base::android::OpenApkAsset(locale_path_within_apk,
                                                   &g_locale_pack_region);
  } else {
    base::FilePath locale_file_path = GetOverriddenPakPath();
    if (locale_file_path.empty())
      locale_file_path = GetLocaleFilePath(app_locale, true);

    if (locale_file_path.empty()) {
      // It's possible that there is no locale.pak.
      LOG(WARNING) << "locale_file_path.empty() for locale " << app_locale;
      return std::string();
    }
    int flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
    g_locale_pack_fd = base::File(locale_file_path, flags).TakePlatformFile();
    g_locale_pack_region = base::MemoryMappedFile::Region::kWholeFile;
  }

  std::unique_ptr<DataPack> data_pack(new DataPack(SCALE_FACTOR_100P));
  if (!data_pack->LoadFromFileRegion(base::File(g_locale_pack_fd),
                                     g_locale_pack_region)) {
    UMA_HISTOGRAM_ENUMERATION("ResourceBundle.LoadLocaleResourcesError",
                              logging::GetLastSystemErrorCode(), 16000);
    LOG(ERROR) << "failed to load locale.pak";
    NOTREACHED();
    return std::string();
  }

  locale_resources_data_.reset(data_pack.release());
  return app_locale;
}

gfx::Image& ResourceBundle::GetNativeImageNamed(int resource_id) {
  return GetImageNamed(resource_id);
}

void SetLocalePaksStoredInApk(bool value) {
  g_locale_paks_in_apk = value;
}

void LoadMainAndroidPackFile(const char* path_within_apk,
                             const base::FilePath& disk_file_path) {
  if (LoadFromApkOrFile(path_within_apk,
                        &disk_file_path,
                        &g_resources_pack_fd,
                        &g_resources_pack_region)) {
    ResourceBundle::GetSharedInstance().AddDataPackFromFileRegion(
        base::File(g_resources_pack_fd), g_resources_pack_region,
        SCALE_FACTOR_NONE);
  }
}

int GetMainAndroidPackFd(base::MemoryMappedFile::Region* out_region) {
  DCHECK_GE(g_resources_pack_fd, 0);
  *out_region = g_resources_pack_region;
  return g_resources_pack_fd;
}

int GetCommonResourcesPackFd(base::MemoryMappedFile::Region* out_region) {
  DCHECK_GE(g_chrome_100_percent_fd, 0);
  *out_region = g_chrome_100_percent_region;
  return g_chrome_100_percent_fd;
}

int GetLocalePackFd(base::MemoryMappedFile::Region* out_region) {
  DCHECK_GE(g_locale_pack_fd, 0);
  *out_region = g_locale_pack_region;
  return g_locale_pack_fd;
}

std::string GetPathForAndroidLocalePakWithinApk(const std::string& locale) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> ret =
      Java_ResourceBundle_getLocalePakResourcePath(
          env, base::android::ConvertUTF8ToJavaString(env, locale).obj());
  if (ret.obj() == nullptr) {
    return std::string();
  }
  return base::android::ConvertJavaStringToUTF8(env, ret.obj());
}

bool RegisterResourceBundleAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace ui
