// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/cast_sys_info_android.h"

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "chromecast/base/cast_sys_info_util.h"
#include "chromecast/base/version.h"
#include "jni/CastSysInfoAndroid_jni.h"

namespace chromecast {

namespace {
const char kBuildTypeUser[] = "user";
}  // namespace

// static
bool CastSysInfoAndroid::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
std::unique_ptr<CastSysInfo> CreateSysInfo() {
  return base::WrapUnique(new CastSysInfoAndroid());
}

CastSysInfoAndroid::CastSysInfoAndroid()
    : build_info_(base::android::BuildInfo::GetInstance()) {
}

CastSysInfoAndroid::~CastSysInfoAndroid() {
}

CastSysInfo::BuildType CastSysInfoAndroid::GetBuildType() {
  if (CAST_IS_DEBUG_BUILD())
    return BUILD_ENG;

  int build_number;
  if (!base::StringToInt(CAST_BUILD_INCREMENTAL, &build_number))
    build_number = 0;

  // Note: no way to determine which channel was used on play store.
  if (strcmp(build_info_->build_type(), kBuildTypeUser) == 0 &&
      build_number > 0) {
    return BUILD_PRODUCTION;
  }

  // Dogfooders without a user system build should all still have non-Debug
  // builds of the cast receiver APK, but with valid build numbers.
  if (build_number > 0)
    return BUILD_BETA;

  // Default to ENG build.
  return BUILD_ENG;
}

std::string CastSysInfoAndroid::GetSerialNumber() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::android::ConvertJavaStringToUTF8(
      Java_CastSysInfoAndroid_getSerialNumber(env));
}

std::string CastSysInfoAndroid::GetProductName() {
  return build_info_->device();
}

std::string CastSysInfoAndroid::GetDeviceModel() {
  return build_info_->model();
}

std::string CastSysInfoAndroid::GetManufacturer() {
  return build_info_->manufacturer();
}

std::string CastSysInfoAndroid::GetSystemBuildNumber() {
  return base::SysInfo::GetAndroidBuildID();
}

std::string CastSysInfoAndroid::GetSystemReleaseChannel() {
  return "";
}

std::string CastSysInfoAndroid::GetBoardName() {
  return "";
}

std::string CastSysInfoAndroid::GetBoardRevision() {
  return "";
}

std::string CastSysInfoAndroid::GetFactoryCountry() {
  return "";
}

std::string CastSysInfoAndroid::GetFactoryLocale(std::string* second_locale) {
  return "";
}

std::string CastSysInfoAndroid::GetWifiInterface() {
  return "";
}

std::string CastSysInfoAndroid::GetApInterface() {
  return "";
}

std::string CastSysInfoAndroid::GetGlVendor() {
  NOTREACHED() << "GL information shouldn't be requested on Android.";
  return "";
}

std::string CastSysInfoAndroid::GetGlRenderer() {
  NOTREACHED() << "GL information shouldn't be requested on Android.";
  return "";
}

std::string CastSysInfoAndroid::GetGlVersion() {
  NOTREACHED() << "GL information shouldn't be requested on Android.";
  return "";
}

}  // namespace chromecast
