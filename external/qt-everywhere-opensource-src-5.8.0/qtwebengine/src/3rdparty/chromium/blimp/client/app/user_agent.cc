// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/app/user_agent.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"

namespace blimp {

/**
 * Returns a string for building user agent such as :
 * Linux; Android 5.1.1; Nexus 6 Build/LMY49C
 */
std::string GetOSVersionInfoForUserAgent() {
  std::string os_cpu;

#if defined(OS_ANDROID)
  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(
      &os_major_version, &os_minor_version, &os_bugfix_version);
#endif

#if defined(OS_POSIX) && !defined(OS_ANDROID)
  // Should work on any Posix system.
  struct utsname unixinfo;
  uname(&unixinfo);

  std::string cputype;
  // special case for biarch systems
  if (strcmp(unixinfo.machine, "x86_64") == 0 &&
      sizeof(void*) == sizeof(int32_t)) {  // NOLINT
    cputype.assign("i686 (x86_64)");
  } else {
    cputype.assign(unixinfo.machine);
  }
#endif

#if defined(OS_ANDROID)
  std::string android_version_str;
  base::StringAppendF(&android_version_str, "%d.%d", os_major_version,
                      os_minor_version);
  if (os_bugfix_version != 0)
    base::StringAppendF(&android_version_str, ".%d", os_bugfix_version);

  std::string android_info_str;

  // Send information about the device.
  bool semicolon_inserted = false;
  std::string android_build_codename = base::SysInfo::GetAndroidBuildCodename();
  std::string android_device_name = base::SysInfo::HardwareModelName();
  if ("REL" == android_build_codename && android_device_name.size() > 0) {
    android_info_str += "; " + android_device_name;
    semicolon_inserted = true;
  }

  // Append the build ID.
  std::string android_build_id = base::SysInfo::GetAndroidBuildID();
  if (android_build_id.size() > 0) {
    if (!semicolon_inserted) {
      android_info_str += ";";
    }
    android_info_str += " Build/" + android_build_id;
  }
#endif

  base::StringAppendF(&os_cpu,
#if defined(OS_ANDROID)
                      "Android %s%s", android_version_str.c_str(),
                      android_info_str.c_str()
#else
                      "%s %s",
                      unixinfo.sysname,  // e.g. Linux
                      cputype.c_str()    // e.g. i686
#endif
                          );  // NOLINT

  os_cpu = "Linux; " + os_cpu;

  return os_cpu;
}

}  // namespace blimp
