// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/app/crashpad.h"

#include <stddef.h>
#include <string.h>

#if BUILDFLAG(ENABLE_KASKO)
#include <psapi.h>
#endif  // BUILDFLAG(ENABLE_KASKO)

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

#if BUILDFLAG(ENABLE_KASKO)
#include "base/win/scoped_handle.h"
#include "third_party/crashpad/crashpad/snapshot/api/module_annotations_win.h"
#endif

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

#if BUILDFLAG(ENABLE_KASKO)
// TODO(ananta)
// We cannot depend on functionality in base which pulls in dependencies on
// user32 directly or indirectly. The GetLoadedModulesSnapshot is a copy of the
// function in base/win/win_util.cc. Depending on the base function pulls in
// dependencies on user32 due to other functionality in win_util.cc. This
// function should be removed when KASKO is removed.
bool GetLoadedModulesSnapshot(HANDLE process, std::vector<HMODULE>* snapshot) {
  DCHECK(snapshot);
  DCHECK_EQ(0u, snapshot->size());
  snapshot->resize(128);

  // We will retry at least once after first determining |bytes_required|. If
  // the list of modules changes after we receive |bytes_required| we may retry
  // more than once.
  int retries_remaining = 5;
  do {
    DWORD bytes_required = 0;
    // EnumProcessModules returns 'success' even if the buffer size is too
    // small.
    DCHECK_GE(std::numeric_limits<DWORD>::max(),
      snapshot->size() * sizeof(HMODULE));
    if (!::EnumProcessModules(
      process, &(*snapshot)[0],
      static_cast<DWORD>(snapshot->size() * sizeof(HMODULE)),
      &bytes_required)) {
      DPLOG(ERROR) << "::EnumProcessModules failed.";
      return false;
    }
    DCHECK_EQ(0u, bytes_required % sizeof(HMODULE));
    size_t num_modules = bytes_required / sizeof(HMODULE);
    if (num_modules <= snapshot->size()) {
      // Buffer size was too big, presumably because a module was unloaded.
      snapshot->erase(snapshot->begin() + num_modules, snapshot->end());
      return true;
    } else if (num_modules == 0) {
      DLOG(ERROR) << "Can't determine the module list size.";
      return false;
    } else {
      // Buffer size was too small. Try again with a larger buffer. A little
      // more room is given to avoid multiple expensive calls to
      // ::EnumProcessModules() just because one module has been added.
      snapshot->resize(num_modules + 8, NULL);
    }
  } while (--retries_remaining);

  DLOG(ERROR) << "Failed to enumerate modules.";
  return false;
}

HMODULE GetModuleInProcess(base::ProcessHandle process,
                           const wchar_t* module_name) {
  std::vector<HMODULE> modules_snapshot;
  if (!GetLoadedModulesSnapshot(process, &modules_snapshot))
    return nullptr;

  for (HMODULE module : modules_snapshot) {
    wchar_t current_module_name[MAX_PATH];
    if (!::GetModuleBaseName(process, module, current_module_name, MAX_PATH))
      continue;

    if (std::wcscmp(module_name, current_module_name) == 0)
      return module;
  }
  return nullptr;
}
#endif  // BUILDFLAG(ENABLE_KASKO)

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

#if !defined(OS_WIN) || !defined(COMPONENT_BUILD)
  // chrome/common/child_process_logging_win.cc registers crash keys for
  // chrome.dll. In a component build, that is sufficient as chrome.dll and
  // chrome.exe share a copy of base (in base.dll). In a static build, the EXE
  // must separately initialize the crash keys configuration as it has its own
  // statically linked copy of base.
  base::debug::SetCrashKeyReportingFunctions(SetCrashKeyValue, ClearCrashKey);
  crash_reporter_client->RegisterCrashKeys();
#endif

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

    bool enable_uploads = false;
    if (!crash_reporter_client->ReportingIsEnforcedByPolicy(&enable_uploads)) {
      // Breakpad provided a --disable-breakpad switch to disable crash dumping
      // (not just uploading) here. Crashpad doesn't need it: dumping is enabled
      // unconditionally and uploading is gated on consent, which tests/bots
      // shouldn't have. As a precaution, uploading is also disabled on bots
      // even if consent is present.
      enable_uploads = crash_reporter_client->GetCollectStatsConsent() &&
                       !crash_reporter_client->IsRunningUnattended();
    }

    SetUploadsEnabled(enable_uploads);
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

void SetUploadsEnabled(bool enable_uploads) {
  if (g_database) {
    crashpad::Settings* settings = g_database->GetSettings();
    settings->SetUploadsEnabled(enable_uploads);
  }
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
    report.state = ReportUploadState::Pending;
    reports->push_back(report);
  }

  std::sort(reports->begin(), reports->end(),
            [](const Report& a, const Report& b) {
              return a.capture_time > b.capture_time;
            });
}

#if BUILDFLAG(ENABLE_KASKO)

void GetCrashKeysForKasko(std::vector<kasko::api::CrashKey>* crash_keys) {
  // Get the platform annotations.
  std::map<std::string, std::string> annotations;
  internal::GetPlatformCrashpadAnnotations(&annotations);

  // Reserve room for the GUID and the platform annotations.
  crash_keys->clear();
  crash_keys->reserve(
      g_simple_string_dictionary->GetCount() + 1 + annotations.size());

  // Set the Crashpad client ID in the crash keys.
  bool got_guid = false;
  if (g_database) {
    crashpad::Settings* settings = g_database->GetSettings();
    crashpad::UUID uuid;
    if (settings->GetClientID(&uuid)) {
      kasko::api::CrashKey kv;
      wcsncpy_s(kv.name, L"guid", _TRUNCATE);
      wcsncpy_s(kv.value, base::UTF8ToWide(uuid.ToString()).c_str(), _TRUNCATE);
      crash_keys->push_back(kv);
      got_guid = true;
    }
  }

  crashpad::SimpleStringDictionary::Iterator iter(*g_simple_string_dictionary);
  for (;;) {
    const auto* entry = iter.Next();
    if (!entry)
      break;

    // Skip the 'guid' key if it was already set.
    static const char kGuid[] = "guid";
    if (got_guid && ::strncmp(entry->key, kGuid, arraysize(kGuid)) == 0)
      continue;

    // Skip any platform annotations as they'll be set below.
    if (annotations.count(entry->key))
      continue;

    kasko::api::CrashKey kv;
    wcsncpy_s(kv.name, base::UTF8ToWide(entry->key).c_str(), _TRUNCATE);
    wcsncpy_s(kv.value, base::UTF8ToWide(entry->value).c_str(), _TRUNCATE);
    crash_keys->push_back(kv);
  }

  // Merge in the platform annotations.
  for (const auto& entry : annotations) {
    kasko::api::CrashKey kv;
    wcsncpy_s(kv.name, base::UTF8ToWide(entry.first).c_str(), _TRUNCATE);
    wcsncpy_s(kv.value, base::UTF8ToWide(entry.second).c_str(), _TRUNCATE);
    crash_keys->push_back(kv);
  }
}

void ReadMainModuleAnnotationsForKasko(
    const base::Process& process,
    std::vector<kasko::api::CrashKey>* crash_keys) {
  // Reopen process with necessary access.
  base::win::ScopedHandle process_handle(::OpenProcess(
      PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process.Pid()));
  if (!process_handle.IsValid())
    return;

  // The executable name is the same for the browser process and the crash
  // reporter.
  wchar_t exe_file[MAX_PATH] = {};
  CHECK(::GetModuleFileName(nullptr, exe_file, arraysize(exe_file)));

  base::FilePath exe_path(exe_file);

  HMODULE module = GetModuleInProcess(process_handle.Get(),
                                      exe_path.BaseName().value().c_str());
  if (!module)
    return;

  std::map<std::string, std::string> annotations;
  crashpad::ReadModuleAnnotations(process_handle.Get(), module, &annotations);

  // Append the annotations to the crash keys.
  for (const auto& entry : annotations) {
    kasko::api::CrashKey kv;
    wcsncpy_s(kv.name, base::UTF8ToWide(entry.first).c_str(), _TRUNCATE);
    wcsncpy_s(kv.value, base::UTF8ToWide(entry.second).c_str(), _TRUNCATE);
    crash_keys->push_back(kv);
  }
}

#endif  // BUILDFLAG(ENABLE_KASKO)

}  // namespace crash_reporter

#if defined(OS_WIN)

extern "C" {

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

}  // extern "C"

#endif  // OS_WIN
