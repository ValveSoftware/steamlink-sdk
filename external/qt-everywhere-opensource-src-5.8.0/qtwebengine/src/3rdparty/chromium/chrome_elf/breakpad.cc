// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module contains the necessary code to register the Breakpad exception
// handler. This implementation is based on Chrome's crash reporting code.

#include "chrome_elf/breakpad.h"

#include <sddl.h>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "breakpad/src/client/windows/handler/exception_handler.h"
#include "chrome/common/chrome_version.h"
#include "chrome/install_static/install_util.h"

google_breakpad::ExceptionHandler* g_elf_breakpad = NULL;

namespace {

const wchar_t kBreakpadProductName[] = L"Chrome";
const wchar_t kBreakpadVersionEntry[] = L"ver";
const wchar_t kBreakpadProdEntry[] = L"prod";
const wchar_t kBreakpadPlatformEntry[] = L"plat";
const wchar_t kBreakpadPlatformWin32[] = L"Win32";
const wchar_t kBreakpadProcessEntry[] = L"ptype";
const wchar_t kBreakpadChannelEntry[] = L"channel";

// The protocol for connecting to the out-of-process Breakpad crash
// reporter is different for x86-32 and x86-64: the message sizes
// are different because the message struct contains a pointer.  As
// a result, there are two different named pipes to connect to.  The
// 64-bit one is distinguished with an "-x64" suffix.
const wchar_t kChromePipeName[] = L"\\\\.\\pipe\\ChromeCrashServices\\";
const wchar_t kGoogleUpdatePipeName[] = L"\\\\.\\pipe\\GoogleCrashServices\\";
const wchar_t kSystemPrincipalSid[] = L"S-1-5-18";

const wchar_t kNoErrorDialogs[] = L"noerrdialogs";

google_breakpad::CustomClientInfo* GetCustomInfo() {
  base::string16 process =
      install_static::IsNonBrowserProcess() ? L"renderer" : L"browser";

  wchar_t exe_path[MAX_PATH] = {};
  base::string16 channel;
  if (GetModuleFileName(NULL, exe_path, arraysize(exe_path)) &&
      install_static::IsSxSChrome(exe_path)) {
    channel = L"canary";
  }

  static google_breakpad::CustomInfoEntry ver_entry(
      kBreakpadVersionEntry, TEXT(CHROME_VERSION_STRING));
  static google_breakpad::CustomInfoEntry prod_entry(
      kBreakpadProdEntry, kBreakpadProductName);
  static google_breakpad::CustomInfoEntry plat_entry(
      kBreakpadPlatformEntry, kBreakpadPlatformWin32);
  static google_breakpad::CustomInfoEntry proc_entry(
      kBreakpadProcessEntry, process.c_str());
  static google_breakpad::CustomInfoEntry channel_entry(
      kBreakpadChannelEntry, channel.c_str());
  static google_breakpad::CustomInfoEntry entries[] = {
      ver_entry, prod_entry, plat_entry, proc_entry, channel_entry};
  static google_breakpad::CustomClientInfo custom_info = {
      entries, arraysize(entries) };
  return &custom_info;
}

base::string16 GetUserSidString() {
  // Get the current token.
  HANDLE token = NULL;
  base::string16 user_sid;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token))
    return user_sid;

  DWORD size = sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE;
  BYTE user_bytes[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE] = {};
  TOKEN_USER* user = reinterpret_cast<TOKEN_USER*>(user_bytes);

  wchar_t* sid_string = NULL;
  if (::GetTokenInformation(token, TokenUser, user, size, &size) &&
      user->User.Sid &&
      ::ConvertSidToStringSid(user->User.Sid, &sid_string)) {
    user_sid = sid_string;
    ::LocalFree(sid_string);
  }

  CloseHandle(token);
  return user_sid;
}

bool IsHeadless() {
  DWORD ret = ::GetEnvironmentVariable(L"CHROME_HEADLESS", NULL, 0);
  if (ret != 0)
    return true;

  wchar_t* command_line = ::GetCommandLine();

  // Note: Since this is a pure substring search rather than a check for a
  // switch, there is a small chance that this code will match things that the
  // Chrome code (which executes a similar check) does not. However, as long as
  // no other switches contain the string "noerrdialogs", it should not be an
  // issue.
  return (command_line && wcsstr(command_line, kNoErrorDialogs));
}

}  // namespace

int GenerateCrashDump(EXCEPTION_POINTERS* exinfo) {
  DWORD code = exinfo->ExceptionRecord->ExceptionCode;
  if (code == EXCEPTION_BREAKPOINT || code == EXCEPTION_SINGLE_STEP)
    return EXCEPTION_CONTINUE_SEARCH;

  if (g_elf_breakpad != NULL)
    g_elf_breakpad->WriteMinidumpForException(exinfo);
  return EXCEPTION_CONTINUE_SEARCH;
}

void InitializeCrashReporting() {
  wchar_t exe_path[MAX_PATH] = {};
  if (!::GetModuleFileName(NULL, exe_path, arraysize(exe_path)))
    return;

  // Disable the message box for assertions.
  _CrtSetReportMode(_CRT_ASSERT, 0);

  // Get the alternate dump directory. We use the temp path.
  // N.B. We don't use base::GetTempDir() here to avoid running more code then
  //      necessary before crashes can be properly reported.
  wchar_t temp_directory[MAX_PATH + 1] = {};
  DWORD length = GetTempPath(MAX_PATH, temp_directory);
  if (length == 0)
    return;

  // Minidump with stacks, PEB, TEBs and unloaded module list.
  MINIDUMP_TYPE dump_type = static_cast<MINIDUMP_TYPE>(
      MiniDumpWithProcessThreadData |  // Get PEB and TEB.
      MiniDumpWithUnloadedModules |  // Get unloaded modules when available.
      MiniDumpWithIndirectlyReferencedMemory);  // Get memory referenced by
                                                // stack.

#if defined(GOOGLE_CHROME_BUILD) && defined(OFFICIAL_BUILD)
  bool is_official_chrome_build = true;
#else
  bool is_official_chrome_build = false;
#endif

  base::string16 pipe_name;

  bool enabled_by_policy = false;
  bool use_policy =
      install_static::ReportingIsEnforcedByPolicy(&enabled_by_policy);

  if (!use_policy && IsHeadless()) {
    pipe_name = kChromePipeName;
  } else if (use_policy ? enabled_by_policy
                        : (is_official_chrome_build &&
                           install_static::GetCollectStatsConsent())) {
    // Build the pipe name. It can be one of:
    // 32-bit system: \\.\pipe\GoogleCrashServices\S-1-5-18
    // 32-bit user: \\.\pipe\GoogleCrashServices\<user SID>
    // 64-bit system: \\.\pipe\GoogleCrashServices\S-1-5-18-x64
    // 64-bit user: \\.\pipe\GoogleCrashServices\<user SID>-x64
    base::string16 user_sid = install_static::IsSystemInstall(exe_path)
                                  ? kSystemPrincipalSid
                                  : GetUserSidString();
    if (user_sid.empty())
      return;

    pipe_name = kGoogleUpdatePipeName;
    pipe_name += user_sid;

#if defined(_WIN64)
    pipe_name += L"-x64";
#endif
  } else {
    // Either this is a Chromium build, reporting is disabled by policy or the
    // user has not given consent.
    return;
  }

  g_elf_breakpad = new google_breakpad::ExceptionHandler(
      temp_directory,
      NULL,
      NULL,
      NULL,
      google_breakpad::ExceptionHandler::HANDLER_ALL,
      dump_type,
      pipe_name.c_str(),
      GetCustomInfo());

  if (g_elf_breakpad->IsOutOfProcess()) {
    // Tells breakpad to handle breakpoint and single step exceptions.
    g_elf_breakpad->set_handle_debug_exceptions(true);
  }
}
