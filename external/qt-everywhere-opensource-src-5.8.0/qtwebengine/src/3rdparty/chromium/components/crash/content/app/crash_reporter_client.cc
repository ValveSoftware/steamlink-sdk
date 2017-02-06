// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/app/crash_reporter_client.h"

#include "build/build_config.h"

// On Windows don't use FilePath and logging.h.
// http://crbug.com/604923
#if !defined(OS_WIN)
#include "base/files/file_path.h"
#include "base/logging.h"
#else
#include <assert.h>
#define DCHECK assert
#endif

namespace crash_reporter {

namespace {

CrashReporterClient* g_client = NULL;

}  // namespace

void SetCrashReporterClient(CrashReporterClient* client) {
  g_client = client;
}

CrashReporterClient* GetCrashReporterClient() {
  DCHECK(g_client);
  return g_client;
}

CrashReporterClient::CrashReporterClient() {}
CrashReporterClient::~CrashReporterClient() {}

#if !defined(OS_MACOSX) && !defined(OS_WIN)
void CrashReporterClient::SetCrashReporterClientIdFromGUID(
    const std::string& client_guid) {}
#endif

#if defined(OS_WIN)
bool CrashReporterClient::ShouldCreatePipeName(
    const base::string16& process_type) {
  return process_type == L"browser";
}

bool CrashReporterClient::GetAlternativeCrashDumpLocation(
    base::string16* crash_dir) {
  return false;
}

void CrashReporterClient::GetProductNameAndVersion(
    const base::string16& exe_path,
    base::string16* product_name,
    base::string16* version,
    base::string16* special_build,
    base::string16* channel_name) {
}

bool CrashReporterClient::ShouldShowRestartDialog(base::string16* title,
                                                  base::string16* message,
                                                  bool* is_rtl_locale) {
  return false;
}

bool CrashReporterClient::AboutToRestart() {
  return false;
}

bool CrashReporterClient::GetDeferredUploadsSupported(
    bool is_per_usr_install) {
  return false;
}

bool CrashReporterClient::GetIsPerUserInstall(const base::string16& exe_path) {
  return true;
}

bool CrashReporterClient::GetShouldDumpLargerDumps(bool is_per_user_install) {
  return false;
}

int CrashReporterClient::GetResultCodeRespawnFailed() {
  return 0;
}
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_IOS)
void CrashReporterClient::GetProductNameAndVersion(const char** product_name,
                                                   const char** version) {
}

base::FilePath CrashReporterClient::GetReporterLogFilename() {
  return base::FilePath();
}

bool CrashReporterClient::HandleCrashDump(const char* crashdump_filename) {
  return false;
}
#endif

#if defined(OS_WIN)
bool CrashReporterClient::GetCrashDumpLocation(base::string16* crash_dir) {
#else
bool CrashReporterClient::GetCrashDumpLocation(base::FilePath* crash_dir) {
#endif
  return false;
}

size_t CrashReporterClient::RegisterCrashKeys() {
  return 0;
}

bool CrashReporterClient::IsRunningUnattended() {
  return true;
}

bool CrashReporterClient::GetCollectStatsConsent() {
  return false;
}

#if defined(OS_WIN) || defined(OS_MACOSX)
bool CrashReporterClient::ReportingIsEnforcedByPolicy(bool* breakpad_enabled) {
  return false;
}
#endif

#if defined(OS_ANDROID)
int CrashReporterClient::GetAndroidMinidumpDescriptor() {
  return 0;
}

bool CrashReporterClient::ShouldEnableBreakpadMicrodumps() {
// Always enable microdumps on Android when stripping unwind tables. Rationale:
// when unwind tables are stripped out (to save binary size) the stack traces
// produced locally in the case of a crash / CHECK are meaningless. In order to
// provide meaningful development diagnostics (and keep the binary size savings)
// on Android we attach a secondary crash handler which serializes a reduced
// form of logcat on the console.
#if defined(NO_UNWIND_TABLES)
  return true;
#else
  return false;
#endif
}
#endif

bool CrashReporterClient::EnableBreakpadForProcess(
    const std::string& process_type) {
  return false;
}

}  // namespace crash_reporter
