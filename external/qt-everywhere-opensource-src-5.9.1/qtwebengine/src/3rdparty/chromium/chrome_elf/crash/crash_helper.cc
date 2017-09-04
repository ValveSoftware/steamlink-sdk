// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_elf/crash/crash_helper.h"

#include <assert.h>
#include <windows.h>

#include <algorithm>
#include <string>
#include <vector>

#include "chrome/app/chrome_crash_reporter_client_win.h"
#include "chrome_elf/hook_util/hook_util.h"
#include "components/crash/content/app/crashpad.h"
#include "components/crash/core/common/crash_keys.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"

namespace {

// Crash handling from elf is only enabled for the chrome.exe process.
// Use this global to safely handle the rare case where elf may not be in that
// process (e.g. tests).
bool g_crash_helper_enabled = false;

// Global pointer to a vector of crash reports.
// This structure will be initialized in InitializeCrashReportingForProcess()
// and cleaned up in DllDetachCrashReportingCleanup().
std::vector<crash_reporter::Report>* g_crash_reports = nullptr;

// chrome_elf loads early in the process and initializes Crashpad. That in turn
// uses the SetUnhandledExceptionFilter API to set a top level exception
// handler for the process. When the process eventually initializes, CRT sets
// an exception handler which calls TerminateProcess which effectively bypasses
// us. Ideally we want to be at the top of the unhandled exception filter
// chain. However we don't have a good way of intercepting the
// SetUnhandledExceptionFilter API in the sandbox. EAT patching kernel32 or
// kernelbase should ideally work. However the kernel32 kernelbase dlls are
// prebound which causes EAT patching to not work. Sidestep works. However it
// is only supported for 32 bit. For now we use IAT patching for the
// executable.
// TODO(ananta).
// Check if it is possible to fix EAT patching or use sidestep patching for
// 32 bit and 64 bit for this purpose.
elf_hook::IATHook* g_set_unhandled_exception_filter = nullptr;

// Hook function, which ignores the request to set an unhandled-exception
// filter.
LPTOP_LEVEL_EXCEPTION_FILTER WINAPI
SetUnhandledExceptionFilterPatch(LPTOP_LEVEL_EXCEPTION_FILTER filter) {
  // Don't set the exception filter. Please see above for comments.
  return nullptr;
}

}  // namespace

//------------------------------------------------------------------------------
// Public chrome_elf crash APIs
//------------------------------------------------------------------------------

namespace elf_crash {

// NOTE: This function will be called from DllMain during DLL_PROCESS_ATTACH
// (while we have the loader lock), so do not misbehave.
bool InitializeCrashReporting() {
#ifdef _DEBUG
  assert(g_crash_reports == nullptr);
  assert(g_set_unhandled_exception_filter == nullptr);
#endif  // _DEBUG

  // No global objects with destructors, so using global pointers.
  // DllMain on detach will clean these up.
  g_crash_reports = new std::vector<crash_reporter::Report>;
  g_set_unhandled_exception_filter = new elf_hook::IATHook();

  ChromeCrashReporterClient::InitializeCrashReportingForProcess();

  g_crash_helper_enabled = true;
  return true;
}

// NOTE: This function will be called from DllMain during DLL_PROCESS_DETACH
// (while we have the loader lock), so do not misbehave.
void ShutdownCrashReporting() {
  if (g_crash_reports != nullptr) {
    g_crash_reports->clear();
    delete g_crash_reports;
  }
  if (g_set_unhandled_exception_filter != nullptr) {
    delete g_set_unhandled_exception_filter;
  }
}

// Please refer to the comment on g_set_unhandled_exception_filter for more
// information about why we intercept the SetUnhandledExceptionFilter API.
void DisableSetUnhandledExceptionFilter() {
  if (!g_crash_helper_enabled)
    return;
  if (g_set_unhandled_exception_filter->Hook(
          ::GetModuleHandle(nullptr), "kernel32.dll",
          "SetUnhandledExceptionFilter",
          SetUnhandledExceptionFilterPatch) != NO_ERROR) {
#ifdef _DEBUG
    assert(false);
#endif  //_DEBUG
  }
}

int GenerateCrashDump(EXCEPTION_POINTERS* exception_pointers) {
  if (g_crash_helper_enabled)
    crashpad::CrashpadClient::DumpWithoutCrash(
        *(exception_pointers->ContextRecord));
  return EXCEPTION_CONTINUE_SEARCH;
}

}  // namespace elf_crash

//------------------------------------------------------------------------------
// Exported crash APIs for the rest of the process.
//------------------------------------------------------------------------------

// This helper is invoked by code in chrome.dll to retrieve the crash reports.
// See CrashUploadListCrashpad. Note that we do not pass a std::vector here,
// because we do not want to allocate/free in different modules. The returned
// pointer is read-only.
//
// NOTE: Since the returned pointer references read-only memory that will be
// cleaned up when this DLL unloads, be careful not to reference the memory
// beyond that point (e.g. during tests).
extern "C" __declspec(dllexport) void GetCrashReportsImpl(
    const crash_reporter::Report** reports,
    size_t* report_count) {
  if (!g_crash_helper_enabled)
    return;
  crash_reporter::GetReports(g_crash_reports);
  *reports = g_crash_reports->data();
  *report_count = g_crash_reports->size();
}

// This helper is invoked by debugging code in chrome to register the client
// id.
extern "C" __declspec(dllexport) void SetMetricsClientId(
    const char* client_id) {
  if (!g_crash_helper_enabled)
    return;
  if (client_id)
    crash_keys::SetMetricsClientIdFromGUID(client_id);
}
