// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/app/crashpad.h"

#include <stddef.h>
#include <string.h>

#include <algorithm>
#include <map>
#include <vector>

#include "base/auto_reset.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/crash/content/app/crash_reporter_client.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"
#include "third_party/crashpad/crashpad/client/crashpad_info.h"
#include "third_party/crashpad/crashpad/client/settings.h"
#include "third_party/crashpad/crashpad/client/simple_string_dictionary.h"
#include "third_party/crashpad/crashpad/client/simulate_crash.h"

#if defined(OS_POSIX)
#include <unistd.h>
#endif  // OS_POSIX

namespace crash_reporter {

namespace {

crashpad::SimpleStringDictionary* g_simple_string_dictionary;
crashpad::CrashReportDatabase* g_database;

void SetCrashKeyValue(const base::StringPiece& key,
                      const base::StringPiece& value) {
  g_simple_string_dictionary->SetKeyValue(key.data(), value.data());
}

void ClearCrashKey(const base::StringPiece& key) {
  g_simple_string_dictionary->RemoveKey(key.data());
}

bool LogMessageHandler(int severity,
                       const char* file,
                       int line,
                       size_t message_start,
                       const std::string& string) {
  // Only handle FATAL.
  if (severity != logging::LOG_FATAL) {
    return false;
  }

  // In case of an out-of-memory condition, this code could be reentered when
  // constructing and storing the key. Using a static is not thread-safe, but if
  // multiple threads are in the process of a fatal crash at the same time, this
  // should work.
  static bool guarded = false;
  if (guarded) {
    return false;
  }
  base::AutoReset<bool> guard(&guarded, true);

  // Only log last path component.  This matches logging.cc.
  if (file) {
    const char* slash = strrchr(file, '/');
    if (slash) {
      file = slash + 1;
    }
  }

  CHECK_LE(message_start, string.size());
  std::string message = base::StringPrintf("%s:%d: %s", file, line,
                                           string.c_str() + message_start);
  SetCrashKeyValue("LOG_FATAL", message);

  // Rather than including the code to force the crash here, allow the caller to
  // do it.
  return false;
}

void DumpWithoutCrashing() {
  CRASHPAD_SIMULATE_CRASH();
}

void InitializeCrashpadImpl(bool initial_client,
                            const std::string& process_type,
                            bool embedded_handler) {
  static bool initialized = false;
  DCHECK(!initialized);
  initialized = true;

  const bool browser_process = process_type.empty();
  CrashReporterClient* crash_reporter_client = GetCrashReporterClient();

  if (initial_client) {
#if defined(OS_MACOSX)
    // "relauncher" is hard-coded because it's a Chrome --type, but this
    // component can't see Chrome's switches. This is only used for argument
    // sanitization.
    DCHECK(browser_process || process_type == "relauncher");
#elif defined(OS_WIN)
    // "Chrome Installer" is the name historically used for installer binaries
    // as processed by the backend.
    DCHECK(browser_process || process_type == "Chrome Installer");
#else
#error Port.
#endif  // OS_MACOSX
  } else {
    DCHECK(!browser_process);
  }

  // database_path is only valid in the browser process.
  base::FilePath database_path = internal::PlatformCrashpadInitialization(
      initial_client, browser_process, embedded_handler);

  crashpad::CrashpadInfo* crashpad_info =
      crashpad::CrashpadInfo::GetCrashpadInfo();

#if defined(OS_MACOSX)
#if defined(NDEBUG)
  const bool is_debug_build = false;
#else
  const bool is_debug_build = true;
#endif

  // Disable forwarding to the system's crash reporter in processes other than
  // the browser process. For the browser, the system's crash reporter presents
  // the crash UI to the user, so it's desirable there. Additionally, having
  // crash reports appear in ~/Library/Logs/DiagnosticReports provides a
  // fallback. Forwarding is turned off for debug-mode builds even for the
  // browser process, because the system's crash reporter can take a very long
  // time to chew on symbols.
  if (!browser_process || is_debug_build) {
    crashpad_info->set_system_crash_reporter_forwarding(
        crashpad::TriState::kDisabled);
  }
#endif  // OS_MACOSX

  g_simple_string_dictionary = new crashpad::SimpleStringDictionary();
  crashpad_info->set_simple_annotations(g_simple_string_dictionary);

  // On Windows chrome_elf registers crash keys. This should work identically
  // for component and non component builds.
  base::debug::SetCrashKeyReportingFunctions(SetCrashKeyValue, ClearCrashKey);
  crash_reporter_client->RegisterCrashKeys();

  SetCrashKeyValue("ptype", browser_process ? base::StringPiece("browser")
                                            : base::StringPiece(process_type));
#if defined(OS_POSIX)
  SetCrashKeyValue("pid", base::IntToString(getpid()));
#elif defined(OS_WIN)
  SetCrashKeyValue("pid", base::IntToString(::GetCurrentProcessId()));
#endif

  logging::SetLogMessageHandler(LogMessageHandler);

  // If clients called CRASHPAD_SIMULATE_CRASH() instead of
  // base::debug::DumpWithoutCrashing(), these dumps would appear as crashes in
  // the correct function, at the correct file and line. This would be
  // preferable to having all occurrences show up in DumpWithoutCrashing() at
  // the same file and line.
  base::debug::SetDumpWithoutCrashingFunction(DumpWithoutCrashing);

#if defined(OS_MACOSX)
  // On Mac, we only want the browser to initialize the database, but not the
  // relauncher.
  const bool should_initialize_database_and_set_upload_policy = browser_process;
#elif defined(OS_WIN)
  // On Windows, we want both the browser process and the installer and any
  // other "main, first process" to initialize things. There is no "relauncher"
  // on Windows, so this is synonymous with initial_client.
  const bool should_initialize_database_and_set_upload_policy = initial_client;
#endif
  if (should_initialize_database_and_set_upload_policy) {
    g_database =
        crashpad::CrashReportDatabase::Initialize(database_path).release();

    SetUploadConsent(crash_reporter_client->GetCollectStatsConsent());
  }
}

}  // namespace

void InitializeCrashpad(bool initial_client, const std::string& process_type) {
  InitializeCrashpadImpl(initial_client, process_type, false);
}

#if defined(OS_WIN)
void InitializeCrashpadWithEmbeddedHandler(bool initial_client,
                                           const std::string& process_type) {
  InitializeCrashpadImpl(initial_client, process_type, true);
}
#endif  // OS_WIN

void SetUploadConsent(bool consent) {
  if (!g_database)
    return;

  bool enable_uploads = false;
  CrashReporterClient* crash_reporter_client = GetCrashReporterClient();
  if (!crash_reporter_client->ReportingIsEnforcedByPolicy(&enable_uploads)) {
    // Breakpad provided a --disable-breakpad switch to disable crash dumping
    // (not just uploading) here. Crashpad doesn't need it: dumping is enabled
    // unconditionally and uploading is gated on consent, which tests/bots
    // shouldn't have. As a precaution, uploading is also disabled on bots even
    // if consent is present.
    enable_uploads = consent && !crash_reporter_client->IsRunningUnattended();
  }

  crashpad::Settings* settings = g_database->GetSettings();
  settings->SetUploadsEnabled(enable_uploads &&
                              crash_reporter_client->GetCollectStatsInSample());
}

bool GetUploadsEnabled() {
  if (g_database) {
    crashpad::Settings* settings = g_database->GetSettings();
    bool enable_uploads;
    if (settings->GetUploadsEnabled(&enable_uploads)) {
      return enable_uploads;
    }
  }

  return false;
}

void GetReports(std::vector<Report>* reports) {
  reports->clear();

  if (!g_database) {
    return;
  }

  std::vector<crashpad::CrashReportDatabase::Report> completed_reports;
  crashpad::CrashReportDatabase::OperationStatus status =
      g_database->GetCompletedReports(&completed_reports);
  if (status != crashpad::CrashReportDatabase::kNoError) {
    return;
  }

  std::vector<crashpad::CrashReportDatabase::Report> pending_reports;
  status = g_database->GetPendingReports(&pending_reports);
  if (status != crashpad::CrashReportDatabase::kNoError) {
    return;
  }

  for (const crashpad::CrashReportDatabase::Report& completed_report :
       completed_reports) {
    Report report;
    report.local_id = completed_report.uuid.ToString();
    report.capture_time = completed_report.creation_time;
    report.remote_id = completed_report.id;
    if (completed_report.uploaded) {
      report.upload_time = completed_report.last_upload_attempt_time;
      report.state = ReportUploadState::Uploaded;
    } else {
      report.upload_time = 0;
      report.state = ReportUploadState::NotUploaded;
    }
    reports->push_back(report);
  }

  for (const crashpad::CrashReportDatabase::Report& pending_report :
       pending_reports) {
    Report report;
    report.local_id = pending_report.uuid.ToString();
    report.capture_time = pending_report.creation_time;
    report.upload_time = 0;
    report.state = pending_report.upload_explicitly_requested
                       ? ReportUploadState::Pending_UserRequested
                       : report.state = ReportUploadState::Pending;
    reports->push_back(report);
  }

  std::sort(reports->begin(), reports->end(),
            [](const Report& a, const Report& b) {
              return a.capture_time > b.capture_time;
            });
}

void RequestSingleCrashUpload(const std::string& local_id) {
  if (!g_database)
    return;
  crashpad::UUID uuid;
  uuid.InitializeFromString(local_id);
  g_database->RequestUpload(uuid);
}

}  // namespace crash_reporter

#if defined(OS_WIN)

extern "C" {

// This function is used in chrome_metrics_services_manager_client.cc to trigger
// changes to the upload-enabled state. This is done when the metrics services
// are initialized, and when the user changes their consent for uploads. See
// crash_reporter::SetUploadConsent for effects. The given consent value should
// be consistent with
// crash_reporter::GetCrashReporterClient()->GetCollectStatsConsent(), but it's
// not enforced to avoid blocking startup code on synchronizing them.
void __declspec(dllexport) __cdecl SetUploadConsentImpl(bool consent) {
  crash_reporter::SetUploadConsent(consent);
}

// NOTE: This function is used by SyzyASAN to annotate crash reports. If you
// change the name or signature of this function you will break SyzyASAN
// instrumented releases of Chrome. Please contact syzygy-team@chromium.org
// before doing so! See also http://crbug.com/567781.
void __declspec(dllexport) __cdecl SetCrashKeyValueImpl(const wchar_t* key,
                                                        const wchar_t* value) {
  crash_reporter::SetCrashKeyValue(base::UTF16ToUTF8(key),
                                   base::UTF16ToUTF8(value));
}

void __declspec(dllexport) __cdecl ClearCrashKeyValueImpl(const wchar_t* key) {
  crash_reporter::ClearCrashKey(base::UTF16ToUTF8(key));
}

// This helper is invoked by code in chrome.dll to request a single crash report
// upload. See CrashUploadListCrashpad.
void __declspec(dllexport)
    RequestSingleCrashUploadImpl(const std::string& local_id) {
  crash_reporter::RequestSingleCrashUpload(local_id);
}
}  // extern "C"

#endif  // OS_WIN
