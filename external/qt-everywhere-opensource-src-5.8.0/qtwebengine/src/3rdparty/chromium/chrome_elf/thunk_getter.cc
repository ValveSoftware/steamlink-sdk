// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <windows.h>

#include "base/macros.h"
#include "sandbox/win/src/interception_internal.h"
#include "sandbox/win/src/internal_types.h"
#include "sandbox/win/src/sandbox_utils.h"
#include "sandbox/win/src/service_resolver.h"

namespace {
enum Version {
  VERSION_PRE_XP_SP2 = 0,  // Not supported.
  VERSION_XP_SP2,
  VERSION_SERVER_2003,  // Also includes XP Pro x64 and Server 2003 R2.
  VERSION_VISTA,        // Also includes Windows Server 2008.
  VERSION_WIN7,         // Also includes Windows Server 2008 R2.
  VERSION_WIN8,         // Also includes Windows Server 2012.
  VERSION_WIN8_1,
  VERSION_WIN10,
  VERSION_WIN_LAST,  // Indicates error condition.
};

#if !defined(_WIN64)
// Whether a process is running under WOW64 (the wrapper that allows 32-bit
// processes to run on 64-bit versions of Windows).  This will return
// WOW64_DISABLED for both "32-bit Chrome on 32-bit Windows" and "64-bit
// Chrome on 64-bit Windows".  WOW64_UNKNOWN means "an error occurred", e.g.
// the process does not have sufficient access rights to determine this.
enum WOW64Status { WOW64_DISABLED, WOW64_ENABLED, WOW64_UNKNOWN, };

WOW64Status GetWOW64StatusForCurrentProcess() {
  typedef BOOL(WINAPI * IsWow64ProcessFunc)(HANDLE, PBOOL);
  IsWow64ProcessFunc is_wow64_process = reinterpret_cast<IsWow64ProcessFunc>(
      GetProcAddress(GetModuleHandle(L"kernel32.dll"), "IsWow64Process"));
  if (!is_wow64_process)
    return WOW64_DISABLED;
  BOOL is_wow64 = FALSE;
  if (!is_wow64_process(GetCurrentProcess(), &is_wow64))
    return WOW64_UNKNOWN;
  return is_wow64 ? WOW64_ENABLED : WOW64_DISABLED;
}
#endif  // !defined(_WIN64)

class OSInfo {
 public:
  struct VersionNumber {
    int major;
    int minor;
    int build;
  };

  struct ServicePack {
    int major;
    int minor;
  };

  OSInfo() {
    OSVERSIONINFOEX version_info = {sizeof(version_info)};
    GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&version_info));
    version_number_.major = version_info.dwMajorVersion;
    version_number_.minor = version_info.dwMinorVersion;
    version_number_.build = version_info.dwBuildNumber;
    if ((version_number_.major == 5) && (version_number_.minor > 0)) {
      // Treat XP Pro x64, Home Server, and Server 2003 R2 as Server 2003.
      version_ =
          (version_number_.minor == 1) ? VERSION_XP_SP2 : VERSION_SERVER_2003;
      if (version_ == VERSION_XP_SP2 && version_info.wServicePackMajor < 2)
        version_ = VERSION_PRE_XP_SP2;
    } else if (version_number_.major == 6) {
      switch (version_number_.minor) {
        case 0:
          // Treat Windows Server 2008 the same as Windows Vista.
          version_ = VERSION_VISTA;
          break;
        case 1:
          // Treat Windows Server 2008 R2 the same as Windows 7.
          version_ = VERSION_WIN7;
          break;
        case 2:
          // Treat Windows Server 2012 the same as Windows 8.
          version_ = VERSION_WIN8;
          break;
        default:
          version_ = VERSION_WIN8_1;
          break;
      }
    } else if (version_number_.major == 10) {
      version_ = VERSION_WIN10;
    } else if (version_number_.major > 6) {
      version_ = VERSION_WIN_LAST;
    } else {
      version_ = VERSION_PRE_XP_SP2;
    }

    service_pack_.major = version_info.wServicePackMajor;
    service_pack_.minor = version_info.wServicePackMinor;
  }

  Version version() const { return version_; }
  VersionNumber version_number() const { return version_number_; }
  ServicePack service_pack() const { return service_pack_; }

 private:
  Version version_;
  VersionNumber version_number_;
  ServicePack service_pack_;

  DISALLOW_COPY_AND_ASSIGN(OSInfo);
};

}  // namespace

sandbox::ServiceResolverThunk* GetThunk(bool relaxed) {
  // Create a thunk via the appropriate ServiceResolver instance.
  sandbox::ServiceResolverThunk* thunk = NULL;

  // No thunks for unsupported OS versions.
  OSInfo os_info;
  if (os_info.version() <= VERSION_PRE_XP_SP2)
    return thunk;

  // Pseudo-handle, no need to close.
  HANDLE current_process = ::GetCurrentProcess();

#if defined(_WIN64)
  // ServiceResolverThunk can handle all the formats in 64-bit (instead only
  // handling one like it does in 32-bit versions).
  thunk = new sandbox::ServiceResolverThunk(current_process, relaxed);
#else
  if (GetWOW64StatusForCurrentProcess() == WOW64_ENABLED) {
    if (os_info.version() >= VERSION_WIN10)
      thunk = new sandbox::Wow64W10ResolverThunk(current_process, relaxed);
    else if (os_info.version() >= VERSION_WIN8)
      thunk = new sandbox::Wow64W8ResolverThunk(current_process, relaxed);
    else
      thunk = new sandbox::Wow64ResolverThunk(current_process, relaxed);
  } else if (os_info.version() >= VERSION_WIN8) {
    thunk = new sandbox::Win8ResolverThunk(current_process, relaxed);
  } else {
    thunk = new sandbox::ServiceResolverThunk(current_process, relaxed);
  }
#endif

  return thunk;
}
