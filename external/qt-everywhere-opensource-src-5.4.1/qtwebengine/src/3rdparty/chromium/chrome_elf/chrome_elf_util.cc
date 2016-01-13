// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_elf/chrome_elf_util.h"

#include <windows.h>

#include "base/macros.h"
#include "base/strings/string16.h"

namespace {

const wchar_t kRegPathClientState[] = L"Software\\Google\\Update\\ClientState";
const wchar_t kRegPathClientStateMedium[] =
    L"Software\\Google\\Update\\ClientStateMedium";
#if defined(GOOGLE_CHROME_BUILD)
const wchar_t kRegPathChromePolicy[] = L"SOFTWARE\\Policies\\Google\\Chrome";
#else
const wchar_t kRegPathChromePolicy[] = L"SOFTWARE\\Policies\\Chromium";
#endif  // defined(GOOGLE_CHROME_BUILD)

const wchar_t kRegValueUsageStats[] = L"usagestats";
const wchar_t kUninstallArgumentsField[] = L"UninstallArguments";
const wchar_t kMetricsReportingEnabled[] =L"MetricsReportingEnabled";

const wchar_t kAppGuidCanary[] =
    L"{4ea16ac7-fd5a-47c3-875b-dbf4a2008c20}";
const wchar_t kAppGuidGoogleChrome[] =
    L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";
const wchar_t kAppGuidGoogleBinaries[] =
    L"{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}";

bool ReadKeyValueString(bool system_install, const wchar_t* key_path,
                        const wchar_t* guid, const wchar_t* value_to_read,
                        base::string16* value_out) {
  HKEY key = NULL;
  value_out->clear();

  base::string16 full_key_path(key_path);
  full_key_path.append(1, L'\\');
  full_key_path.append(guid);

  if (::RegOpenKeyEx(system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
                     full_key_path.c_str(), 0,
                     KEY_QUERY_VALUE | KEY_WOW64_32KEY, &key) !=
                     ERROR_SUCCESS) {
    return false;
  }

  const size_t kMaxStringLength = 1024;
  wchar_t raw_value[kMaxStringLength] = {};
  DWORD size = sizeof(raw_value);
  DWORD type = REG_SZ;
  LONG result = ::RegQueryValueEx(key, value_to_read, 0, &type,
                                  reinterpret_cast<LPBYTE>(raw_value), &size);

  if (result == ERROR_SUCCESS) {
    if (type != REG_SZ || (size & 1) != 0) {
      result = ERROR_NOT_SUPPORTED;
    } else if (size == 0) {
      *raw_value = L'\0';
    } else if (raw_value[size / sizeof(wchar_t) - 1] != L'\0') {
      if ((size / sizeof(wchar_t)) < kMaxStringLength)
        raw_value[size / sizeof(wchar_t)] = L'\0';
      else
        result = ERROR_MORE_DATA;
    }
  }

  if (result == ERROR_SUCCESS)
    *value_out = raw_value;

  ::RegCloseKey(key);

  return result == ERROR_SUCCESS;
}

bool ReadKeyValueDW(bool system_install, const wchar_t* key_path,
                    base::string16 guid, const wchar_t* value_to_read,
                    DWORD* value_out) {
  HKEY key = NULL;
  *value_out = 0;

  base::string16 full_key_path(key_path);
  full_key_path.append(1, L'\\');
  full_key_path.append(guid);

  if (::RegOpenKeyEx(system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
                     full_key_path.c_str(), 0,
                     KEY_QUERY_VALUE | KEY_WOW64_32KEY, &key) !=
                     ERROR_SUCCESS) {
    return false;
  }

  DWORD size = sizeof(*value_out);
  DWORD type = REG_DWORD;
  LONG result = ::RegQueryValueEx(key, value_to_read, 0, &type,
                                  reinterpret_cast<BYTE*>(value_out), &size);

  ::RegCloseKey(key);

  return result == ERROR_SUCCESS && size == sizeof(*value_out);
}

}  // namespace

bool IsCanary(const wchar_t* exe_path) {
  return wcsstr(exe_path, L"Chrome SxS\\Application") != NULL;
}

bool IsSystemInstall(const wchar_t* exe_path) {
  wchar_t program_dir[MAX_PATH] = {};
  DWORD ret = ::GetEnvironmentVariable(L"PROGRAMFILES", program_dir,
                                       arraysize(program_dir));
  if (ret && ret < MAX_PATH && !wcsncmp(exe_path, program_dir, ret))
    return true;

  ret = ::GetEnvironmentVariable(L"PROGRAMFILES(X86)", program_dir,
                                 arraysize(program_dir));
  if (ret && ret < MAX_PATH && !wcsncmp(exe_path, program_dir, ret))
    return true;

  return false;
}

bool IsMultiInstall(bool is_system_install) {
  base::string16 args;
  if (!ReadKeyValueString(is_system_install, kRegPathClientState,
                          kAppGuidGoogleChrome, kUninstallArgumentsField,
                          &args)) {
    return false;
  }
  return args.find(L"--multi-install") != base::string16::npos;
}

bool AreUsageStatsEnabled(const wchar_t* exe_path) {
  bool enabled = true;
  bool controlled_by_policy = ReportingIsEnforcedByPolicy(&enabled);

  if (controlled_by_policy && !enabled)
    return false;

  bool system_install = IsSystemInstall(exe_path);
  base::string16 app_guid;

  if (IsCanary(exe_path)) {
    app_guid = kAppGuidCanary;
  } else {
    app_guid = IsMultiInstall(system_install) ? kAppGuidGoogleBinaries :
                                                kAppGuidGoogleChrome;
  }

  DWORD out_value = 0;
  if (system_install &&
      ReadKeyValueDW(system_install, kRegPathClientStateMedium, app_guid,
                     kRegValueUsageStats, &out_value)) {
    return out_value == 1;
  }

  return ReadKeyValueDW(system_install, kRegPathClientState, app_guid,
                        kRegValueUsageStats, &out_value) && out_value == 1;
}

bool ReportingIsEnforcedByPolicy(bool* breakpad_enabled) {
  HKEY key = NULL;
  DWORD value = 0;
  BYTE* value_bytes = reinterpret_cast<BYTE*>(&value);
  DWORD size = sizeof(value);
  DWORD type = REG_DWORD;

  if (::RegOpenKeyEx(HKEY_LOCAL_MACHINE, kRegPathChromePolicy, 0,
                     KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
    if (::RegQueryValueEx(key, kMetricsReportingEnabled, 0, &type,
                          value_bytes, &size) == ERROR_SUCCESS) {
      *breakpad_enabled = value != 0;
    }
    ::RegCloseKey(key);
    return size == sizeof(value);
  }

  if (::RegOpenKeyEx(HKEY_CURRENT_USER, kRegPathChromePolicy, 0,
                     KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
    if (::RegQueryValueEx(key, kMetricsReportingEnabled, 0, &type,
                          value_bytes, &size) == ERROR_SUCCESS) {
      *breakpad_enabled = value != 0;
    }
    ::RegCloseKey(key);
    return size == sizeof(value);
  }

  return false;
}

bool IsNonBrowserProcess() {
  typedef bool (*IsSandboxedProcessFunc)();
  IsSandboxedProcessFunc is_sandboxed_process_func =
      reinterpret_cast<IsSandboxedProcessFunc>(
          GetProcAddress(GetModuleHandle(NULL), "IsSandboxedProcess"));
  bool is_sandboxed_process =
      is_sandboxed_process_func && is_sandboxed_process_func();

  // TODO(robertshield): Drop the command line check when we drop support for
  // enabling chrome_elf in unsandboxed processes.
  wchar_t* command_line = GetCommandLine();
  bool has_process_type_flag = command_line && wcsstr(command_line, L"--type");

  return (has_process_type_flag || is_sandboxed_process);
}
