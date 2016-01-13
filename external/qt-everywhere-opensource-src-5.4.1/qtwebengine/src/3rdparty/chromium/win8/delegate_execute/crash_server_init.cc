// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "win8/delegate_execute/crash_server_init.h"

#include <shlobj.h>
#include <windows.h>

#include <cwchar>

#include "base/file_version_info.h"
#include "base/memory/scoped_ptr.h"
#include "base/win/win_util.h"
#include "breakpad/src/client/windows/handler/exception_handler.h"

const wchar_t kGoogleUpdatePipeName[] = L"\\\\.\\pipe\\GoogleCrashServices\\";
const wchar_t kSystemPrincipalSid[] = L"S-1-5-18";

const MINIDUMP_TYPE kLargerDumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithProcessThreadData |  // Get PEB and TEB.
    MiniDumpWithUnloadedModules |  // Get unloaded modules when available.
    MiniDumpWithIndirectlyReferencedMemory);  // Get memory referenced by stack.

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace {

bool IsRunningSystemInstall() {
  wchar_t exe_path[MAX_PATH * 2] = {0};
  GetModuleFileName(reinterpret_cast<HMODULE>(&__ImageBase),
                    exe_path,
                    _countof(exe_path));

  bool is_system = false;

  wchar_t program_files_path[MAX_PATH] = {0};
  if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES, NULL,
                                SHGFP_TYPE_CURRENT, program_files_path))) {
    if (wcsstr(exe_path, program_files_path) == exe_path) {
      is_system = true;
    }
  }

  return is_system;
}

google_breakpad::CustomClientInfo* GetCustomInfo() {
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());

  static google_breakpad::CustomInfoEntry ver_entry(
      L"ver", version_info->file_version().c_str());
  static google_breakpad::CustomInfoEntry prod_entry(L"prod", L"Chrome");
  static google_breakpad::CustomInfoEntry plat_entry(L"plat", L"Win32");
  static google_breakpad::CustomInfoEntry type_entry(L"ptype",
                                                     L"delegate_execute");
  static google_breakpad::CustomInfoEntry entries[] = {
      ver_entry, prod_entry, plat_entry, type_entry };
  static google_breakpad::CustomClientInfo custom_info = {
      entries, ARRAYSIZE(entries) };
  return &custom_info;
}

}  // namespace

namespace delegate_execute {

scoped_ptr<google_breakpad::ExceptionHandler> InitializeCrashReporting() {
  wchar_t temp_path[MAX_PATH + 1] = {0};
  DWORD path_len = ::GetTempPath(MAX_PATH, temp_path);

  base::string16 pipe_name;
  pipe_name = kGoogleUpdatePipeName;
  if (IsRunningSystemInstall()) {
    pipe_name += kSystemPrincipalSid;
  } else {
    base::string16 user_sid;
    if (base::win::GetUserSidString(&user_sid)) {
      pipe_name += user_sid;
    } else {
      // We don't think we're a system install, but we couldn't get the
      // user SID. Try connecting to the system-level crash service as a
      // last ditch effort.
      pipe_name += kSystemPrincipalSid;
    }
  }

  return scoped_ptr<google_breakpad::ExceptionHandler>(
      new google_breakpad::ExceptionHandler(
          temp_path, NULL, NULL, NULL,
          google_breakpad::ExceptionHandler::HANDLER_ALL, kLargerDumpType,
          pipe_name.c_str(), GetCustomInfo()));
}

}  // namespace delegate_execute
